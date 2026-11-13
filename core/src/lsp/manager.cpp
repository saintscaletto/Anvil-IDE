#include "anvil/lsp/manager.hpp"

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <utility>

#include "anvil/lsp/uri.hpp"
#include "anvil/security/sandbox.hpp"
#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/process.hpp"
#include "anvil/util/strings.hpp"
#include "anvil/version.hpp"

namespace anvil::lsp {
namespace fs = std::filesystem;

struct Manager::State {
    explicit State(std::string family_id) : family(std::move(family_id)) {}

    std::string family;
    mutable std::mutex mutex;
    std::condition_variable cv;
    bool starting = false;
    std::shared_ptr<Client> client;
    std::string status = "stopped";
    std::string message;
    std::string server_name;
    std::string server_version;
    std::string binary;
    std::unordered_map<std::string, int> documents;
    std::chrono::steady_clock::time_point last_activity = std::chrono::steady_clock::now();
};

namespace {

constexpr int kDefaultRequestTimeoutMs = 15'000;
constexpr int kCompletionTimeoutMs = 8'000;
constexpr int kHoverTimeoutMs = 5'000;
constexpr int kIdleShutdownSeconds = 300;
constexpr std::size_t kMaxDiagnostics = 2'000;

std::string language_family_for_path(const std::string& path) {
    const std::string extension = fs::path(path).extension().string();
    if (extension == ".c") return "cpp";
    if (extension == ".cc" || extension == ".cpp" || extension == ".cxx" ||
        extension == ".c++" || extension == ".h" || extension == ".hh" ||
        extension == ".hpp" || extension == ".hxx" || extension == ".h++" ||
        extension == ".inl" || extension == ".ipp" || extension == ".tpp") {
        return "cpp";
    }
    return {};
}

std::optional<fs::path> find_compile_commands(const fs::path& root) {
    for (const char* relative : {"", "build", "cmake-build-debug", "cmake-build-release",
                                 "out", "build/Debug", "build/Release"}) {
        const fs::path directory = relative[0] == '\0' ? root : root / relative;
        std::error_code ec;
        if (fs::is_regular_file(directory / "compile_commands.json", ec)) {
            return directory;
        }
    }
    return std::nullopt;
}

std::string discover_clangd(const Settings& settings) {
    const std::string configured = settings.clangd_path();
    if (configured != "clangd") {
        const auto resolved = resolve_executable(configured);
        if (!resolved.has_value()) {
            throw ApiError(ErrorCode::LspUnavailable,
                           "Configured language server is not executable: " + configured);
        }
        return *resolved;
    }

    for (const char* name : {"clangd", "clangd-19", "clangd-18", "clangd-17", "clangd-16"}) {
        const auto resolved = resolve_executable(name);
        if (resolved.has_value()) return *resolved;
    }

    std::vector<fs::path> llvm_bins;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/usr/lib", ec)) {
        if (!entry.is_directory()) continue;
        const std::string name = entry.path().filename().string();
        if (!strings::starts_with(name, "llvm-")) continue;
        llvm_bins.push_back(entry.path() / "bin" / "clangd");
    }
    std::sort(llvm_bins.begin(), llvm_bins.end(), std::greater<>());
    for (const auto& candidate : llvm_bins) {
        if (::access(candidate.c_str(), X_OK) == 0) return candidate.string();
    }

    throw ApiError(ErrorCode::LspUnavailable,
                   "clangd was not found. Install clangd-18 (or newer) and retry.");
}

Json location_to_json(const Workspace& workspace, const Json& item) {
    if (!item.is_object()) return nullptr;
    std::string uri;
    Json range;
    if (item.contains("targetUri") && item["targetUri"].is_string()) {
        uri = item["targetUri"].get<std::string>();
        range = item.value("targetSelectionRange", item.value("range", Json()));
    } else if (item.contains("uri") && item["uri"].is_string()) {
        uri = item["uri"].get<std::string>();
        range = item.value("range", Json());
    } else {
        return nullptr;
    }
    const auto path = uri_to_workspace_path(workspace, uri);
    if (!path.has_value()) return nullptr;
    Json location{{"path", *path}, {"range", range}};
    if (item.contains("originSelectionRange")) {
        location["originSelectionRange"] = item["originSelectionRange"];
    }
    return location;
}

Json normalize_locations(const Workspace& workspace, const Json& result) {
    Json locations = Json::array();
    if (result.is_null()) return Json{{"locations", locations}};
    if (result.is_object()) {
        Json location = location_to_json(workspace, result);
        if (!location.is_null()) locations.push_back(std::move(location));
        return Json{{"locations", locations}};
    }
    if (result.is_array()) {
        for (const auto& item : result) {
            Json location = location_to_json(workspace, item);
            if (!location.is_null()) locations.push_back(std::move(location));
        }
    }
    return Json{{"locations", locations}};
}

Json normalize_workspace_edit(const Workspace& workspace, const Json& result) {
    Json changes = Json::array();
    if (!result.is_object()) return Json{{"changes", changes}};

    auto add_uri_edits = [&](const std::string& uri, const Json& edits) {
        const auto path = uri_to_workspace_path(workspace, uri);
        if (!path.has_value() || !edits.is_array()) return;
        changes.push_back(Json{{"path", *path}, {"edits", edits}});
    };

    if (result.contains("changes") && result["changes"].is_object()) {
        for (const auto& [uri, edits] : result["changes"].items()) {
            add_uri_edits(uri, edits);
        }
    }
    if (result.contains("documentChanges") && result["documentChanges"].is_array()) {
        for (const auto& change : result["documentChanges"]) {
            if (!change.is_object() || !change.contains("textDocument")) continue;
            const auto& document = change["textDocument"];
            if (!document.is_object() || !document.contains("uri") ||
                !document["uri"].is_string()) {
                continue;
            }
            add_uri_edits(document["uri"].get<std::string>(),
                          change.value("edits", Json::array()));
        }
    }
    return Json{{"changes", changes}};
}

}  // namespace

Manager::Manager(const Workspace& workspace, const Settings& settings, fs::path cache_dir)
    : workspace_(workspace), settings_(settings), cache_dir_(std::move(cache_dir)) {
    janitor_ = std::jthread([this](const std::stop_token& token) { janitor_loop(token); });
}

Manager::~Manager() {
    janitor_.request_stop();
    if (janitor_.joinable()) janitor_.join();
    shutdown();
}

void Manager::set_event_sink(EventSink sink) {
    std::lock_guard lock(sink_mutex_);
    sink_ = std::move(sink);
}

void Manager::emit(const std::string& method, Json params) const {
    EventSink sink;
    {
        std::lock_guard lock(sink_mutex_);
        sink = sink_;
    }
    if (sink) sink(method, std::move(params));
}

std::string Manager::family_for(const std::string& language_id) {
    if (language_id == "c" || language_id == "cpp" || language_id == "c++") return "cpp";
    return {};
}

std::shared_ptr<Manager::State> Manager::state_for(const std::string& family) {
    std::lock_guard lock(map_mutex_);
    auto it = states_.find(family);
    if (it != states_.end()) return it->second;
    auto state = std::make_shared<State>(family);
    states_.emplace(family, state);
    return state;
}

void Manager::emit_status(const std::shared_ptr<State>& state) {
    Json payload;
    {
        std::lock_guard lock(state->mutex);
        payload = Json{{"languageId", state->family},
                       {"state", state->status},
                       {"message", state->message},
                       {"server", state->server_name},
                       {"version", state->server_version},
                       {"binary", state->binary},
                       {"openDocuments", state->documents.size()}};
    }
    emit("lsp:status", std::move(payload));
}

std::shared_ptr<Manager::State> Manager::ensure_started(const std::string& family) {
    auto state = state_for(family);
    std::shared_ptr<Client> stale_client;

    std::unique_lock state_lock(state->mutex);
    while (true) {
        if (state->client && state->client->running()) {
            state->last_activity = std::chrono::steady_clock::now();
            return state;
        }
        if (state->starting) {
            state->cv.wait(state_lock);
            continue;
        }
        if (state->client) {
            stale_client = std::move(state->client);
            state->status = "stopped";
        }
        if (state->status == "error") {
            const auto since = std::chrono::steady_clock::now() - state->last_activity;
            if (since < std::chrono::seconds(2)) {
                throw ApiError(ErrorCode::LspUnavailable, state->message);
            }
        }
        state->starting = true;
        state->status = "starting";
        state->message.clear();
        break;
    }
    state_lock.unlock();
    emit_status(state);

    // Destroy a crashed client outside the state lock: its reader thread may
    // still be finishing and its exit handler needs the lock.
    if (stale_client) {
        stale_client->shutdown();
        stale_client.reset();
    }

    try {
        if (!workspace_.is_open()) {
            throw ApiError(ErrorCode::WorkspaceNotOpen, "No workspace is open");
        }
        const std::string root = workspace_.root_string();
        const std::string program = discover_clangd(settings_);

        std::vector<std::string> arguments{"--background-index", "--log=error"};
        if (const auto compile_dir = find_compile_commands(root); compile_dir.has_value()) {
            arguments.push_back("--compile-commands-dir=" + compile_dir->string());
        }
        if (!settings_.enable_project_config()) {
            arguments.push_back("--enable-config=false");
        }
        const auto user_args = settings_.clangd_args();
        arguments.insert(arguments.end(), user_args.begin(), user_args.end());

        const fs::path cache = cache_dir_ / "lsp" / workspace_.state_key() / family;
        fsutil::ensure_private_directory(cache);

        security::SandboxSpec sandbox;
        sandbox.workspace = root;
        sandbox.workspace_read_only = true;
        sandbox.cache_dir = cache;
        sandbox.allow_network = false;
        sandbox.allow_exec = false;

        bool dev_unsafe = false;
#if ANVIL_DEV_MODE
        const char* env_override = std::getenv("ANVIL_DEV_UNSAFE_LSP");
        dev_unsafe = settings_.allow_unsandboxed_lsp() && env_override != nullptr &&
                     std::string(env_override) == "1";
#endif
        const bool sandbox_ready = security::sandbox_availability().available;
        const bool use_unsandboxed = dev_unsafe && !sandbox_ready;
        if (use_unsandboxed) {
            logging::security_event("lsp.unsandboxed_dev_mode",
                                    Json{{"program", program}, {"root", root}});
        } else if (!security::is_trusted_system_program(program)) {
            throw ApiError(ErrorCode::LspUnavailable,
                           "clangd is outside trusted system prefixes: " + program);
        }
#if ANVIL_DEV_MODE
        if (use_unsandboxed) {
            if (const char* ld_path = std::getenv("ANVIL_DEV_LD_LIBRARY_PATH");
                ld_path != nullptr && *ld_path != '\0') {
                sandbox.extra_env.emplace_back("LD_LIBRARY_PATH", ld_path);
            }
        }
#endif

        ClientOptions options;
        options.program = program;
        options.arguments = std::move(arguments);
        options.root_uri = strings::path_to_file_uri(root);
        options.sandbox = std::move(sandbox);
        options.seccomp_profile = use_unsandboxed ? security::SeccompProfile::LspDev
                                                   : security::SeccompProfile::Lsp;
        options.allow_unsandboxed_dev = use_unsandboxed;
        options.default_timeout_ms = kDefaultRequestTimeoutMs;

        std::string server_name;
        std::string server_version;
        auto client = Client::create(options, &server_name, &server_version);
        if (!client->running()) {
            throw ApiError(ErrorCode::LspUnavailable,
                           "Language server exited during initialization");
        }

        std::weak_ptr<State> weak_state = state;
        client->set_notification_handler(
            [this, weak_state](const std::string& method, const Json& params) {
                if (auto locked = weak_state.lock()) handle_notification(locked, method, params);
            });
        client->set_log_handler([this, weak_state](const std::string& line) {
            if (auto locked = weak_state.lock()) {
                Json payload{{"languageId", locked->family}, {"line", line}};
                emit("lsp:log", std::move(payload));
            }
        });
        client->set_exit_handler([this, weak_state](int status) {
            auto locked = weak_state.lock();
            if (!locked) return;
            {
                std::lock_guard lock(locked->mutex);
                if (locked->status == "stopped") return;
                locked->status = "error";
                locked->message = "Language server exited unexpectedly (status " +
                                  std::to_string(status) + ")";
                locked->documents.clear();
            }
            emit_status(locked);
        });

        {
            std::lock_guard lock(state->mutex);
            state->client = std::shared_ptr<Client>(std::move(client));
            state->status = "running";
            state->message.clear();
            state->server_name = server_name;
            state->server_version = server_version;
            state->binary = program;
            state->starting = false;
            state->last_activity = std::chrono::steady_clock::now();
            state->cv.notify_all();
        }
        emit_status(state);
        return state;
    } catch (const ApiError& error) {
        {
            std::lock_guard lock(state->mutex);
            state->starting = false;
            state->status = "error";
            state->message = error.what();
            state->last_activity = std::chrono::steady_clock::now();
            state->cv.notify_all();
        }
        emit_status(state);
        throw;
    } catch (const std::exception& error) {
        {
            std::lock_guard lock(state->mutex);
            state->starting = false;
            state->status = "error";
            state->message = error.what();
            state->last_activity = std::chrono::steady_clock::now();
            state->cv.notify_all();
        }
        emit_status(state);
        throw ApiError(ErrorCode::LspUnavailable,
                       std::string("Failed to start language server: ") + error.what());
    }
}

Json Manager::ensure(const std::string& path, const std::string& language_id) {
    (void)path;
    const std::string family =
        language_id.empty() ? "cpp" : family_for(language_id);
    if (family.empty()) {
        throw ApiError(ErrorCode::Unsupported,
                       "Only C and C++ language servers are enabled in this release");
    }
    auto state = ensure_started(family);
    std::lock_guard lock(state->mutex);
    return Json{{"languageId", family},
                {"state", state->status},
                {"server", state->server_name},
                {"version", state->server_version},
                {"binary", state->binary}};
}

Json Manager::status() const {
    Json servers = Json::array();
    {
        std::lock_guard lock(map_mutex_);
        for (const auto& [family, state] : states_) {
            (void)family;
            std::lock_guard state_lock(state->mutex);
            servers.push_back(Json{{"languageId", state->family},
                                   {"state", state->status},
                                   {"message", state->message},
                                   {"server", state->server_name},
                                   {"version", state->server_version},
                                   {"binary", state->binary},
                                   {"openDocuments", state->documents.size()}});
        }
    }
    const auto& sandbox = security::sandbox_availability();
    return Json{{"servers", servers},
                {"sandbox",
                 {{"available", sandbox.available},
                  {"networkNamespace", sandbox.network_namespace},
                  {"bwrap", sandbox.bwrap_path},
                  {"reason", sandbox.reason}}}};
}

void Manager::did_open(const ipc::validate::LspDidOpen& params) {
    const std::string family = family_for(params.language_id);
    if (family.empty()) {
        throw ApiError(ErrorCode::Unsupported,
                       "Unsupported language: " + params.language_id);
    }
    const auto state = ensure_started(family);
    const std::string uri = path_to_uri(workspace_, params.path);

    std::lock_guard sync_lock(sync_mutex_);
    std::shared_ptr<Client> client;
    int previous_version = 0;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running()) {
            throw ApiError(ErrorCode::LspUnavailable, "Language server is not running");
        }
        const auto doc = state->documents.find(params.path);
        if (doc != state->documents.end()) previous_version = doc->second;
    }
    if (previous_version > params.version) return;

    client->notify("textDocument/didOpen",
                   Json{{"textDocument",
                         {{"uri", uri},
                          {"languageId", params.language_id},
                          {"version", params.version},
                          {"text", params.text}}}});
    std::lock_guard lock(state->mutex);
    state->documents[params.path] = params.version;
    state->last_activity = std::chrono::steady_clock::now();
}

Json Manager::did_change(const ipc::validate::LspDidChange& params) {
    const std::string family = language_family_for_path(params.path);
    if (family.empty()) {
        throw ApiError(ErrorCode::Unsupported, "Unsupported document language");
    }
    const auto state = ensure_started(family);
    std::lock_guard sync_lock(sync_mutex_);

    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running()) {
            throw ApiError(ErrorCode::LspUnavailable, "Language server is not running");
        }
        const auto doc = state->documents.find(params.path);
        if (doc == state->documents.end()) {
            throw ApiError(ErrorCode::InvalidParams,
                           "Document is not open in the language server");
        }
        if (params.version <= doc->second) return Json{{"ignored", true}};
    }

    Json changes = Json::array();
    if (params.has_full_text) {
        changes.push_back(Json{{"text", params.full_text}});
    } else {
        for (const auto& change : params.changes) {
            Json item{{"text", change.text}};
            if (change.range.has_value()) {
                item["range"] = Json{
                    {"start",
                     {{"line", change.range->start.line},
                      {"character", change.range->start.character}}},
                    {"end",
                     {{"line", change.range->end.line},
                      {"character", change.range->end.character}}}};
            }
            changes.push_back(std::move(item));
        }
    }
    const std::string uri = path_to_uri(workspace_, params.path);
    client->notify(
        "textDocument/didChange",
        Json{{"textDocument", {{"uri", uri}, {"version", params.version}}},
             {"contentChanges", std::move(changes)}});
    {
        std::lock_guard lock(state->mutex);
        state->documents[params.path] = params.version;
        state->last_activity = std::chrono::steady_clock::now();
    }
    return Json{{"version", params.version}};
}

void Manager::did_save(const std::string& path) {
    const std::string family = language_family_for_path(path);
    if (family.empty()) return;
    const auto state = state_for(family);
    std::lock_guard sync_lock(sync_mutex_);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running()) return;
        if (state->documents.find(path) == state->documents.end()) return;
    }
    client->notify("textDocument/didSave",
                   Json{{"textDocument", {{"uri", path_to_uri(workspace_, path)}}}});
    std::lock_guard lock(state->mutex);
    state->last_activity = std::chrono::steady_clock::now();
}

void Manager::did_close(const std::string& path) {
    const std::string family = language_family_for_path(path);
    if (family.empty()) return;
    const auto state = state_for(family);
    const std::string uri = path_to_uri(workspace_, path);
    std::lock_guard sync_lock(sync_mutex_);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
    }
    if (client && client->running()) {
        client->notify("textDocument/didClose", Json{{"textDocument", {{"uri", uri}}}});
    }
    std::lock_guard lock(state->mutex);
    state->documents.erase(path);
    state->last_activity = std::chrono::steady_clock::now();
}

namespace {

Json position_json(const ipc::validate::Position& position) {
    return Json{{"line", position.line}, {"character", position.character}};
}

}  // namespace

Json Manager::hover(const ipc::validate::LspPosition& params, const CancelToken* token) {
    const std::string family = language_family_for_path(params.path);
    if (family.empty()) throw ApiError(ErrorCode::Unsupported, "Unsupported document language");
    const auto state = ensure_started(family);
    const std::string uri = path_to_uri(workspace_, params.path);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running() ||
            state->documents.find(params.path) == state->documents.end()) {
            throw ApiError(ErrorCode::LspUnavailable, "Document is not synchronized");
        }
        state->last_activity = std::chrono::steady_clock::now();
    }
    return client->request(
        "textDocument/hover",
        Json{{"textDocument", {{"uri", uri}}}, {"position", position_json(params.position)}},
        token, kHoverTimeoutMs);
}

Json Manager::completion(const ipc::validate::LspCompletion& params, const CancelToken* token) {
    const std::string family = language_family_for_path(params.path);
    if (family.empty()) throw ApiError(ErrorCode::Unsupported, "Unsupported document language");
    const auto state = ensure_started(family);
    const std::string uri = path_to_uri(workspace_, params.path);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running() ||
            state->documents.find(params.path) == state->documents.end()) {
            throw ApiError(ErrorCode::LspUnavailable, "Document is not synchronized");
        }
        state->last_activity = std::chrono::steady_clock::now();
    }
    Json lsp_params{{"textDocument", {{"uri", uri}}},
                    {"position", position_json(params.position)}};
    if (params.trigger_character.has_value()) {
        lsp_params["context"] = Json{{"triggerKind", 2},
                                     {"triggerCharacter", *params.trigger_character}};
    } else {
        lsp_params["context"] = Json{{"triggerKind", 1}};
    }
    return client->request("textDocument/completion", std::move(lsp_params), token,
                           kCompletionTimeoutMs);
}

Json Manager::definition(const ipc::validate::LspPosition& params, const CancelToken* token) {
    const std::string family = language_family_for_path(params.path);
    if (family.empty()) throw ApiError(ErrorCode::Unsupported, "Unsupported document language");
    const auto state = ensure_started(family);
    const std::string uri = path_to_uri(workspace_, params.path);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running() ||
            state->documents.find(params.path) == state->documents.end()) {
            throw ApiError(ErrorCode::LspUnavailable, "Document is not synchronized");
        }
        state->last_activity = std::chrono::steady_clock::now();
    }
    const Json result = client->request(
        "textDocument/definition",
        Json{{"textDocument", {{"uri", uri}}}, {"position", position_json(params.position)}},
        token, kDefaultRequestTimeoutMs);
    return normalize_locations(workspace_, result);
}

Json Manager::references(const ipc::validate::LspReferences& params, const CancelToken* token) {
    const std::string family = language_family_for_path(params.path);
    if (family.empty()) throw ApiError(ErrorCode::Unsupported, "Unsupported document language");
    const auto state = ensure_started(family);
    const std::string uri = path_to_uri(workspace_, params.path);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running() ||
            state->documents.find(params.path) == state->documents.end()) {
            throw ApiError(ErrorCode::LspUnavailable, "Document is not synchronized");
        }
        state->last_activity = std::chrono::steady_clock::now();
    }
    const Json result = client->request(
        "textDocument/references",
        Json{{"textDocument", {{"uri", uri}}},
             {"position", position_json(params.position)},
             {"context", {{"includeDeclaration", params.include_declaration}}}},
        token, kDefaultRequestTimeoutMs);
    return normalize_locations(workspace_, result);
}

Json Manager::document_symbols(const ipc::validate::LspPath& params, const CancelToken* token) {
    const std::string family = language_family_for_path(params.path);
    if (family.empty()) throw ApiError(ErrorCode::Unsupported, "Unsupported document language");
    const auto state = ensure_started(family);
    const std::string uri = path_to_uri(workspace_, params.path);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running() ||
            state->documents.find(params.path) == state->documents.end()) {
            throw ApiError(ErrorCode::LspUnavailable, "Document is not synchronized");
        }
        state->last_activity = std::chrono::steady_clock::now();
    }
    return client->request("textDocument/documentSymbol",
                           Json{{"textDocument", {{"uri", uri}}}}, token,
                           kDefaultRequestTimeoutMs);
}

Json Manager::signature_help(const ipc::validate::LspPosition& params, const CancelToken* token) {
    const std::string family = language_family_for_path(params.path);
    if (family.empty()) throw ApiError(ErrorCode::Unsupported, "Unsupported document language");
    const auto state = ensure_started(family);
    const std::string uri = path_to_uri(workspace_, params.path);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running() ||
            state->documents.find(params.path) == state->documents.end()) {
            throw ApiError(ErrorCode::LspUnavailable, "Document is not synchronized");
        }
        state->last_activity = std::chrono::steady_clock::now();
    }
    return client->request(
        "textDocument/signatureHelp",
        Json{{"textDocument", {{"uri", uri}}}, {"position", position_json(params.position)}},
        token, kHoverTimeoutMs);
}

Json Manager::rename(const ipc::validate::LspRename& params, const CancelToken* token) {
    const std::string family = language_family_for_path(params.path);
    if (family.empty()) throw ApiError(ErrorCode::Unsupported, "Unsupported document language");
    const auto state = ensure_started(family);
    const std::string uri = path_to_uri(workspace_, params.path);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running() ||
            state->documents.find(params.path) == state->documents.end()) {
            throw ApiError(ErrorCode::LspUnavailable, "Document is not synchronized");
        }
        state->last_activity = std::chrono::steady_clock::now();
    }
    const Json result = client->request(
        "textDocument/rename",
        Json{{"textDocument", {{"uri", uri}}},
             {"position", position_json(params.position)},
             {"newName", params.new_name}},
        token, kDefaultRequestTimeoutMs);
    return normalize_workspace_edit(workspace_, result);
}

Json Manager::format(const ipc::validate::LspFormat& params, const CancelToken* token) {
    const std::string family = language_family_for_path(params.path);
    if (family.empty()) throw ApiError(ErrorCode::Unsupported, "Unsupported document language");
    const auto state = ensure_started(family);
    const std::string uri = path_to_uri(workspace_, params.path);
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = state->client;
        if (!client || !client->running() ||
            state->documents.find(params.path) == state->documents.end()) {
            throw ApiError(ErrorCode::LspUnavailable, "Document is not synchronized");
        }
        state->last_activity = std::chrono::steady_clock::now();
    }
    return client->request(
        "textDocument/formatting",
        Json{{"textDocument", {{"uri", uri}}},
             {"options", {{"tabSize", params.tab_size}, {"insertSpaces", params.insert_spaces}}}},
        token, kDefaultRequestTimeoutMs);
}

void Manager::handle_notification(const std::shared_ptr<State>& state,
                                  const std::string& method, const Json& params) {
    if (method == "textDocument/publishDiagnostics") {
        if (!params.is_object() || !params.contains("uri") || !params["uri"].is_string()) return;
        const auto path = uri_to_workspace_path(workspace_, params["uri"].get<std::string>());
        if (!path.has_value()) return;
        Json diagnostics = Json::array();
        if (params.contains("diagnostics") && params["diagnostics"].is_array()) {
            for (const auto& diagnostic : params["diagnostics"]) {
                if (diagnostics.size() >= kMaxDiagnostics) break;
                diagnostics.push_back(diagnostic);
            }
        }
        Json payload{{"languageId", state->family},
                     {"path", *path},
                     {"diagnostics", std::move(diagnostics)}};
        if (params.contains("version") && params["version"].is_number_integer()) {
            payload["version"] = params["version"];
        }
        emit("lsp:diagnostics", std::move(payload));
        return;
    }
    if (method == "window/showMessage") {
        emit("lsp:message",
             Json{{"languageId", state->family},
                  {"type", params.value("type", 3)},
                  {"message", logging::redact(params.value("message", ""))}});
        return;
    }
    if (method == "window/logMessage") {
        emit("lsp:log",
             Json{{"languageId", state->family},
                  {"type", params.value("type", 4)},
                  {"line", logging::redact(params.value("message", ""))}});
        return;
    }
    if (method == "$/progress") {
        emit("lsp:progress",
             Json{{"languageId", state->family},
                  {"token", params.value("token", Json())},
                  {"value", params.value("value", Json())}});
        return;
    }
    if (method == "telemetry/event") return;
    logging::debug("unhandled LSP notification: " + method);
}

void Manager::stop_state(const std::shared_ptr<State>& state) {
    {
        std::lock_guard lock(map_mutex_);
        auto it = states_.find(state->family);
        if (it != states_.end() && it->second == state) states_.erase(it);
    }
    std::shared_ptr<Client> client;
    {
        std::lock_guard lock(state->mutex);
        client = std::move(state->client);
        state->status = "stopped";
        state->starting = false;
        state->documents.clear();
        state->cv.notify_all();
    }
    if (client) client.reset();
    emit_status(state);
}

void Manager::restart(const std::string& language_id) {
    const std::string family =
        language_id.empty() ? "" : family_for(language_id);
    if (language_id.empty()) {
        std::vector<std::shared_ptr<State>> snapshot;
        {
            std::lock_guard lock(map_mutex_);
            for (const auto& [name, state] : states_) {
                (void)name;
                snapshot.push_back(state);
            }
        }
        for (const auto& state : snapshot) stop_state(state);
        return;
    }
    if (family.empty()) {
        throw ApiError(ErrorCode::Unsupported, "Unsupported language: " + language_id);
    }
    std::shared_ptr<State> state;
    {
        std::lock_guard lock(map_mutex_);
        auto it = states_.find(family);
        if (it != states_.end()) state = it->second;
    }
    if (state) stop_state(state);
}

void Manager::on_workspace_opened() {
    restart("");
}

void Manager::shutdown() {
    if (shutting_down_.exchange(true)) return;
    std::vector<std::shared_ptr<State>> snapshot;
    {
        std::lock_guard lock(map_mutex_);
        for (const auto& [name, state] : states_) {
            (void)name;
            snapshot.push_back(state);
        }
        states_.clear();
    }
    for (const auto& state : snapshot) {
        std::shared_ptr<Client> client;
        {
            std::lock_guard lock(state->mutex);
            client = std::move(state->client);
            state->status = "stopped";
            state->starting = false;
            state->cv.notify_all();
        }
        if (client) client.reset();
        emit_status(state);
    }
}

void Manager::janitor_loop(const std::stop_token& stop_token) {
    while (!stop_token.stop_requested()) {
        for (int i = 0; i < 30 && !stop_token.stop_requested(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (stop_token.stop_requested() || shutting_down_.load()) return;

        std::vector<std::shared_ptr<State>> idle;
        {
            std::lock_guard lock(map_mutex_);
            for (const auto& [name, state] : states_) {
                (void)name;
                std::lock_guard state_lock(state->mutex);
                if (state->starting || !state->documents.empty()) continue;
                const auto idle_for = std::chrono::steady_clock::now() - state->last_activity;
                if (idle_for >= std::chrono::seconds(kIdleShutdownSeconds)) idle.push_back(state);
            }
        }
        for (const auto& state : idle) {
            logging::info("stopping idle language server: " + state->family);
            stop_state(state);
        }
    }
}

}  // namespace anvil::lsp
