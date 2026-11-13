#include "anvil/ipc/server.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <utility>

#include "anvil/error.hpp"
#include "anvil/git_branch.hpp"
#include "anvil/ipc/cancel.hpp"
#include "anvil/ipc/jsonrpc.hpp"
#include "anvil/ipc/validate.hpp"
#include "anvil/lsp/manager.hpp"
#include "anvil/recent.hpp"
#include "anvil/security/sandbox.hpp"
#include "anvil/settings.hpp"
#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/process.hpp"
#include "anvil/util/strings.hpp"
#include "anvil/util/thread_pool.hpp"
#include "anvil/version.hpp"
#include "anvil/workspace/watcher.hpp"
#include "anvil/workspace/workspace.hpp"

namespace anvil {
namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxDirectoryEntries = 2000;

std::string language_for_path(const std::string& path) {
    const std::string extension = fs::path(path).extension().string();
    if (extension == ".c" || extension == ".cc" || extension == ".cpp" ||
        extension == ".cxx" || extension == ".c++" || extension == ".h" ||
        extension == ".hh" || extension == ".hpp" || extension == ".hxx" ||
        extension == ".h++" || extension == ".inl" || extension == ".ipp" ||
        extension == ".tpp") {
        return "cpp";
    }
    if (extension == ".json") return "json";
    if (extension == ".md" || extension == ".markdown") return "markdown";
    if (extension == ".py") return "python";
    if (extension == ".rs") return "rust";
    if (extension == ".js" || extension == ".mjs" || extension == ".cjs") return "javascript";
    if (extension == ".ts" || extension == ".tsx") return "typescript";
    if (extension == ".yaml" || extension == ".yml") return "yaml";
    if (extension == ".sh" || extension == ".bash") return "shell";
    if (extension == ".cmake" || fs::path(path).filename() == "CMakeLists.txt") return "cmake";
    return "plaintext";
}

bool ignored_search_directory(const std::string& name) {
    return name == ".git" || name == "node_modules" || name == "build" || name == "target" ||
           name == ".cache" || name == "dist" || name == ".anvil";
}

}  // namespace

struct Server::Impl {
    explicit Impl(ServerOptions server_options)
        : options(std::move(server_options)),
          state_store(options.config_dir),
          lsp(workspace, settings, options.cache_dir),
          pool(std::max<std::size_t>(
              2, std::min<std::size_t>(8, std::thread::hardware_concurrency() == 0
                                               ? 4
                                               : std::thread::hardware_concurrency()))) {
        settings.load(this->options.config_dir);
        recent.load(this->options.config_dir);
        lsp.set_event_sink([this](const std::string& method, Json params) {
            send_notification(method, std::move(params));
        });
    }

    ServerOptions options;
    Workspace workspace;
    Settings settings;
    RecentProjects recent;
    WorkspaceStateStore state_store;
    lsp::Manager lsp;
    Watcher watcher;
    ThreadPool pool;

    std::atomic<bool> running{true};
    std::mutex out_mutex;
    std::mutex active_mutex;
    std::unordered_map<std::string, std::shared_ptr<CancelToken>> active;

    void send(Json message) {
        if (!running.load()) return;
        const std::string line = ipc::serialize_line(message);
        std::lock_guard lock(out_mutex);
        const char* data = line.data();
        std::size_t remaining = line.size();
        while (remaining > 0) {
            const ssize_t written = ::write(STDOUT_FILENO, data, remaining);
            if (written < 0) {
                if (errno == EINTR) continue;
                running = false;
                return;
            }
            if (written == 0) {
                running = false;
                return;
            }
            data += written;
            remaining -= static_cast<std::size_t>(written);
        }
    }

    void send_result(const Json& id, Json result) {
        send(ipc::make_result(id, std::move(result)));
    }

    void send_error(const Json& id, ErrorCode code, const std::string& message,
                    Json data = Json()) {
        send(ipc::make_error(id, code, logging::redact(message), std::move(data)));
    }

    void send_notification(const std::string& method, Json params) {
        send(ipc::make_notification(method, std::move(params)));
    }

    void start_watcher() {
        watcher.stop();
        if (!workspace.is_open()) return;
        watcher.start(workspace.resolve(""), [this](Json event) {
            const std::string type = event.value("type", "");
            if (type == "changed") {
                send_notification("fs:changed", Json{{"changes", event.value("changes", Json::array())}});
            } else if (type == "rescan") {
                send_notification("fs:rescan", Json::object());
            } else if (type == "watchUnavailable") {
                send_notification("fs:watchUnavailable", event);
            }
        });
    }

    Json security_status() const {
        const auto& sandbox = security::sandbox_availability();
        return Json{
            {"sandbox",
             {{"available", sandbox.available},
              {"networkNamespace", sandbox.network_namespace},
              {"bwrap", sandbox.bwrap_path},
              {"reason", sandbox.reason}}},
            {"policy",
             {{"workspaceScoped", true},
              {"shellExecution", false},
              {"seccomp", true},
              {"lspIsolation", "bubblewrap + seccomp"},
              {"networkForLsp", false}}},
            {"logs", {{"security", logging::security_log_path().string()}}},
            {"workspace", {{"open", workspace.is_open()}, {"root", workspace.root_string()}}},
        };
    }

    Json system_info() const {
        // The bubblewrap probe is intentionally not on the startup critical
        // path; security/status performs it once the UI is interactive.
        const Json sandbox{{"available", false},
                           {"networkNamespace", false},
                           {"bwrap", ""},
                           {"reason", "sandbox probe pending"}};
        Json workspace_info{{"open", workspace.is_open()}};
        if (workspace.is_open()) {
            workspace_info["root"] = workspace.root_string();
            workspace_info["name"] = workspace.name();
        }
        return Json{
            {"product", std::string(kProductName)},
            {"version", std::string(kVersion)},
            {"apiVersion", kApiVersion},
            {"core", std::string(kCoreName)},
            {"platform", "linux"},
            {"configDir", options.config_dir.string()},
            {"cacheDir", options.cache_dir.string()},
            {"devMode", options.dev_mode},
            {"workspace", std::move(workspace_info)},
            {"settings", settings.snapshot()},
            {"recent", recent.list()},
            {"security", {{"sandbox", sandbox}}},
            {"capabilities",
             {{"fsWatch", watcher.running()},
              {"fileSearch", true},
              {"gitBranch", true},
              {"lspLanguages", Json::array({"c", "cpp"})}}},
        };
    }

    Json workspace_open(const Json& params) {
        const auto parsed = ipc::validate::parse_workspace_open(params);
        workspace.open(parsed.path);
        lsp.on_workspace_opened();
        recent.add(workspace.resolve(""), workspace.name());
        recent.save();
        settings.load(options.config_dir);
        start_watcher();
        Json state = state_store.load(workspace.state_key());
        logging::security_event("workspace.open_ui",
                                Json{{"root", workspace.root_string()},
                                     {"watcher", watcher.running()}});
        return Json{{"root", workspace.root_string()},
                    {"name", workspace.name()},
                    {"state", std::move(state)},
                    {"recent", recent.list()},
                    {"settings", settings.snapshot()},
                    {"git", read_git_branch(workspace)},
                    {"watch", watcher.running()}};
    }

    Json workspace_close() {
        watcher.stop();
        lsp.on_workspace_opened();
        workspace.close();
        return Json{{"ok", true}};
    }

    Json fs_read(const Json& params, const CancelToken& token) {
        const auto parsed = ipc::validate::parse_fs_read(params);
        if (token.cancelled()) {
            throw ApiError(ErrorCode::Cancelled, "Request cancelled");
        }
        const fs::path path = workspace.resolve(parsed.path);
        fsutil::FileStat before = fsutil::stat_path(path);
        std::string content = fsutil::read_text_file(path, parsed.max_bytes);
        fsutil::FileStat after = fsutil::stat_path(path);
        if (before.mtime_ms != after.mtime_ms) {
            // The file changed while it was being read. Read it once more so
            // the returned mtime always describes the returned content.
            content = fsutil::read_text_file(path, parsed.max_bytes);
            after = fsutil::stat_path(path);
        }
        if (token.cancelled()) {
            throw ApiError(ErrorCode::Cancelled, "Request cancelled");
        }
        return Json{{"path", parsed.path},
                    {"content", std::move(content)},
                    {"language", language_for_path(parsed.path)},
                    {"mtime", after.mtime_ms},
                    {"size", after.size}};
    }

    Json fs_write(const Json& params) {
        const auto parsed = ipc::validate::parse_fs_write(params);
        const fs::path path = workspace.resolve_for_create(parsed.path);
        std::error_code ec;
        const bool exists = fs::exists(path, ec);
        if (!parsed.force && exists && !parsed.expected_mtime_ms.has_value()) {
            throw ApiError(ErrorCode::Conflict,
                           "Refusing to overwrite an existing file without expectedMtime");
        }
        if (!parsed.force && !exists && parsed.expected_mtime_ms.has_value()) {
            throw ApiError(ErrorCode::Conflict,
                           "File no longer exists; create it explicitly before saving");
        }
        fsutil::write_text_file_atomic(path, parsed.content,
                                       parsed.force ? std::nullopt : parsed.expected_mtime_ms);
        const auto stat = fsutil::stat_path(path);
        logging::security_event("fs.write",
                                Json{{"path", parsed.path},
                                     {"size", stat.size},
                                     {"forced", parsed.force}});
        return Json{{"path", parsed.path}, {"mtime", stat.mtime_ms}, {"size", stat.size}};
    }

    Json fs_list(const Json& params) {
        const auto parsed = ipc::validate::parse_fs_list(params);
        const fs::path directory = workspace.resolve(parsed.path);
        const auto stat = fsutil::stat_path(directory);
        if (!stat.is_directory) {
            throw ApiError(ErrorCode::InvalidParams, "Not a directory: " + parsed.path);
        }
        bool truncated = false;
        const auto entries = fsutil::list_directory(directory, kMaxDirectoryEntries, &truncated);
        Json items = Json::array();
        for (const auto& entry : entries) {
            const std::string relative =
                parsed.path.empty() ? entry.name : parsed.path + "/" + entry.name;
            items.push_back(Json{{"name", entry.name},
                                 {"path", relative},
                                 {"isDir", entry.is_directory},
                                 {"isSymlink", entry.is_symlink},
                                 {"size", entry.size},
                                 {"mtime", entry.mtime_ms}});
        }
        return Json{{"path", parsed.path}, {"entries", std::move(items)}, {"truncated", truncated}};
    }

    Json fs_stat(const Json& params) {
        const auto parsed = ipc::validate::parse_lsp_path(params);  // same {path} shape
        const auto stat = fsutil::stat_path(workspace.resolve_nofollow(parsed.path), false);
        return Json{{"path", parsed.path},
                    {"isDir", stat.is_directory},
                    {"isFile", stat.is_regular},
                    {"isSymlink", stat.is_symlink},
                    {"size", stat.size},
                    {"mtime", stat.mtime_ms}};
    }

    Json fs_create(const Json& params) {
        const auto parsed = ipc::validate::parse_fs_create(params);
        const fs::path path = workspace.resolve_for_create(parsed.path);
        std::error_code ec;
        if (fs::exists(path, ec)) {
            throw ApiError(ErrorCode::AlreadyExists, "Path already exists: " + parsed.path);
        }
        if (parsed.directory) {
            if (!fs::create_directory(path, ec) || ec) {
                throw ApiError(ErrorCode::PermissionDenied,
                               "Cannot create directory: " + parsed.path);
            }
            ::chmod(path.c_str(), 0755);
        } else {
            const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
            if (fd < 0) {
                throw ApiError(ErrorCode::PermissionDenied,
                               "Cannot create file: " + parsed.path);
            }
            ::close(fd);
        }
        logging::security_event("fs.create",
                                Json{{"path", parsed.path}, {"directory", parsed.directory}});
        return Json{{"path", parsed.path},
                    {"isDir", parsed.directory},
                    {"isFile", !parsed.directory}};
    }

    Json fs_rename(const Json& params) {
        const auto parsed = ipc::validate::parse_fs_rename(params);
        const fs::path from = workspace.resolve_nofollow(parsed.from);
        const fs::path to = workspace.resolve_for_create(parsed.to);
        std::error_code ec;
        if (fs::exists(to, ec)) {
            throw ApiError(ErrorCode::AlreadyExists, "Destination already exists: " + parsed.to);
        }
        fs::rename(from, to, ec);
        if (ec) {
            throw ApiError(ErrorCode::PermissionDenied,
                           "Rename failed: " + ec.message());
        }
        logging::security_event("fs.rename",
                                Json{{"from", parsed.from}, {"to", parsed.to}});
        return Json{{"path", parsed.to}};
    }

    Json fs_delete(const Json& params) {
        const auto parsed = ipc::validate::parse_fs_delete(params);
        if (parsed.path.empty()) {
            throw ApiError(ErrorCode::InvalidParams, "Refusing to delete the workspace root");
        }
        const fs::path path = workspace.resolve_nofollow(parsed.path);
        std::error_code ec;
        if (parsed.path.empty()) {
            throw ApiError(ErrorCode::InvalidParams, "Refusing to delete the workspace root");
        }
        const auto status = fs::symlink_status(path, ec);
        if (ec) throw ApiError(ErrorCode::NotFound, "Path not found: " + parsed.path);
        if (fs::is_directory(status) && !parsed.recursive) {
            const bool empty = fs::is_empty(path, ec);
            if (ec) throw ApiError(ErrorCode::PermissionDenied, "Cannot inspect directory");
            if (!empty) {
                throw ApiError(ErrorCode::Conflict,
                               "Directory is not empty; recursive deletion was not confirmed");
            }
        }
        const std::uintmax_t removed = parsed.recursive ? fs::remove_all(path, ec)
                                                        : (fs::remove(path, ec) ? 1 : 0);
        if (ec) {
            throw ApiError(ErrorCode::PermissionDenied,
                           "Delete failed: " + ec.message());
        }
        logging::security_event("fs.delete",
                                Json{{"path", parsed.path},
                                     {"recursive", parsed.recursive},
                                     {"removed", removed}});
        return Json{{"ok", true}};
    }

    Json fs_search_files(const Json& params, const CancelToken& token) {
        const auto parsed = ipc::validate::parse_fs_search(params);
        const std::string needle = strings::to_lower(parsed.query);
        struct Hit {
            int score;
            std::size_t length;
            std::string path;
        };
        std::vector<Hit> hits;
        std::error_code ec;
        const fs::path root = workspace.resolve("");
        fs::recursive_directory_iterator iterator(
            root, fs::directory_options::skip_permission_denied, ec);
        if (ec) throw ApiError(ErrorCode::PermissionDenied, "Cannot search workspace");
        std::size_t visited = 0;
        bool truncated = false;
        for (const auto& entry : iterator) {
            if (token.cancelled()) throw ApiError(ErrorCode::Cancelled, "Search cancelled");
            if (++visited % 256 == 0 && token.cancelled()) {
                throw ApiError(ErrorCode::Cancelled, "Search cancelled");
            }
            if (it_depth(iterator) > 32) {
                iterator.disable_recursion_pending();
                continue;
            }
            const auto status = entry.symlink_status(ec);
            if (ec) continue;
            if (fs::is_symlink(status)) continue;
            if (fs::is_directory(status)) {
                if (ignored_search_directory(entry.path().filename().string())) {
                    iterator.disable_recursion_pending();
                }
                continue;
            }
            if (!fs::is_regular_file(status)) continue;
            const std::string name = entry.path().filename().string();
            const std::string lower_name = strings::to_lower(name);
            const std::size_t position = lower_name.find(needle);
            if (position == std::string::npos) continue;
            const auto relative = fsutil::relative_to(root, entry.path());
            if (!relative.has_value()) continue;
            const int score = position == 0 ? (name.size() == needle.size() ? 0 : 1) : 2;
            hits.push_back({score, relative->size(), *relative});
            if (hits.size() >= static_cast<std::size_t>(parsed.limit)) {
                truncated = true;
                break;
            }
        }
        std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
            if (a.score != b.score) return a.score < b.score;
            return a.length < b.length;
        });
        Json results = Json::array();
        for (const auto& hit : hits) {
            results.push_back(Json{{"path", hit.path},
                                   {"name", fs::path(hit.path).filename().string()}});
        }
        return Json{{"results", std::move(results)}, {"truncated", truncated}};
    }

    static int it_depth(const fs::recursive_directory_iterator& iterator) {
        return iterator.depth();
    }

    Json dispatch(const ipc::Request& request, const CancelToken& token) {
        const std::string& method = request.method;
        const Json& params = request.params;

        if (method == "system/ping") {
            ipc::validate::parse_no_params(params);
            return Json{{"pong", true}, {"time", std::chrono::duration_cast<std::chrono::milliseconds>(
                                                       std::chrono::system_clock::now()
                                                           .time_since_epoch())
                                                       .count()}};
        }
        if (method == "system/info") {
            ipc::validate::parse_no_params(params);
            return system_info();
        }
        if (method == "system/shutdown") {
            ipc::validate::parse_no_params(params);
            running = false;
            return Json{{"ok", true}};
        }
        if (method == "security/status") {
            ipc::validate::parse_no_params(params);
            return security_status();
        }
        if (method == "settings/get") {
            ipc::validate::parse_no_params(params);
            return settings.snapshot();
        }
        if (method == "settings/set") {
            const auto parsed = ipc::validate::parse_settings_set(params);
            settings.set(parsed.key, parsed.value);
            return settings.snapshot();
        }
        if (method == "workspace/open") {
            return workspace_open(params);
        }
        if (method == "workspace/close") {
            ipc::validate::parse_no_params(params);
            return workspace_close();
        }
        if (method == "workspace/saveState") {
            const auto parsed = ipc::validate::parse_workspace_save_state(params);
            if (!workspace.is_open()) {
                throw ApiError(ErrorCode::WorkspaceNotOpen, "No workspace is open");
            }
            state_store.save(workspace.state_key(), parsed.tabs, parsed.active);
            return Json{{"ok", true}};
        }
        if (method == "fs/read") return fs_read(params, token);
        if (method == "fs/write") return fs_write(params);
        if (method == "fs/list") return fs_list(params);
        if (method == "fs/stat") return fs_stat(params);
        if (method == "fs/create") return fs_create(params);
        if (method == "fs/rename") return fs_rename(params);
        if (method == "fs/delete") return fs_delete(params);
        if (method == "fs/searchFiles") return fs_search_files(params, token);
        if (method == "git/branch") {
            ipc::validate::parse_no_params(params);
            return read_git_branch(workspace);
        }
        if (method == "recent/list") {
            ipc::validate::parse_no_params(params);
            return Json{{"recent", recent.list()}};
        }
        if (method == "recent/remove") {
            const auto path = ipc::validate::absolute_path(params, "path");
            const bool removed = recent.remove(path);
            recent.save();
            return Json{{"removed", removed}};
        }
        if (method == "lsp/ensure") {
            const auto parsed = ipc::validate::parse_lsp_ensure(params);
            return lsp.ensure(parsed.path, parsed.language_id);
        }
        if (method == "lsp/didOpen") {
            lsp.did_open(ipc::validate::parse_lsp_did_open(params));
            return Json{{"ok", true}};
        }
        if (method == "lsp/didChange") {
            return lsp.did_change(ipc::validate::parse_lsp_did_change(params));
        }
        if (method == "lsp/didSave") {
            lsp.did_save(ipc::validate::parse_lsp_path(params).path);
            return Json{{"ok", true}};
        }
        if (method == "lsp/didClose") {
            lsp.did_close(ipc::validate::parse_lsp_path(params).path);
            return Json{{"ok", true}};
        }
        if (method == "lsp/hover") {
            return lsp.hover(ipc::validate::parse_lsp_position(params), &token);
        }
        if (method == "lsp/completion") {
            return lsp.completion(ipc::validate::parse_lsp_completion(params), &token);
        }
        if (method == "lsp/definition") {
            return lsp.definition(ipc::validate::parse_lsp_position(params), &token);
        }
        if (method == "lsp/references") {
            return lsp.references(ipc::validate::parse_lsp_references(params), &token);
        }
        if (method == "lsp/documentSymbols") {
            return lsp.document_symbols(ipc::validate::parse_lsp_path(params), &token);
        }
        if (method == "lsp/signatureHelp") {
            return lsp.signature_help(ipc::validate::parse_lsp_position(params), &token);
        }
        if (method == "lsp/rename") {
            return lsp.rename(ipc::validate::parse_lsp_rename(params), &token);
        }
        if (method == "lsp/format") {
            return lsp.format(ipc::validate::parse_lsp_format(params), &token);
        }
        if (method == "lsp/restart") {
            const auto parsed = ipc::validate::parse_lsp_restart(params);
            lsp.restart(parsed.language_id);
            return Json{{"ok", true}};
        }
        if (method == "lsp/status") {
            ipc::validate::parse_no_params(params);
            return lsp.status();
        }

        throw ApiError(ErrorCode::MethodNotFound, "Unknown method: " + method);
    }

    void process_request(ipc::Request request, std::shared_ptr<CancelToken> token) {
        const bool notification = request.is_notification;
        try {
            if (token->cancelled()) {
                throw ApiError(ErrorCode::Cancelled, "Request cancelled");
            }
            Json result = dispatch(request, *token);
            if (!notification) send_result(request.id, std::move(result));
        } catch (const ApiError& error) {
            if (!notification) {
                send_error(request.id, error.code(), error.what(), error.data());
            }
        } catch (const std::exception& error) {
            logging::error(std::string("request failed: ") + error.what());
            if (!notification) {
                send_error(request.id, ErrorCode::InternalError, error.what());
            }
        }
        {
            std::lock_guard lock(active_mutex);
            const auto it = active.find(request.id_key());
            if (it != active.end() && it->second == token) active.erase(it);
        }
    }

    void handle_cancel(const Json& params) {
        const auto parsed = ipc::validate::parse_cancel(params);
        const std::string key = "n:" + std::to_string(parsed.target_id.get<std::int64_t>());
        std::shared_ptr<CancelToken> token;
        {
            std::lock_guard lock(active_mutex);
            const auto it = active.find(key);
            if (it != active.end()) token = it->second;
        }
        if (token) {
            token->cancel();
            logging::security_event("request.cancel", Json{{"target", key}});
        }
    }

    int run() {
        logging::security_event("core.ready",
                                Json{{"version", std::string(kVersion)},
                                     {"apiVersion", kApiVersion},
                                     {"devMode", options.dev_mode}});
        FdLineReader reader(STDIN_FILENO);
        std::string line;
        while (running.load()) {
            bool too_large = false;
            std::string read_error;
            if (!reader.read_line(&line, ipc::kMaxJsonLineBytes, &too_large, &read_error)) {
                if (too_large) {
                    logging::security_event("ipc.oversized_message", Json::object());
                    send_error(Json(), ErrorCode::PayloadTooLarge,
                               "JSON-RPC message exceeds the size limit");
                } else if (!read_error.empty() && read_error != "end of stream") {
                    logging::warn("stdin read ended: " + read_error);
                }
                break;
            }
            if (line.empty()) continue;

            ipc::Request request;
            try {
                request = ipc::parse_request(line);
            } catch (const ApiError& error) {
                send_error(Json(), error.code(), error.what(), error.data());
                continue;
            }

            if (request.api_version != kApiVersion) {
                if (!request.is_notification) {
                    send_error(request.id, ErrorCode::InvalidRequest,
                               "apiVersion 1 is required");
                }
                continue;
            }

            if (request.is_notification) {
                if (request.method == "request/cancel") {
                    try {
                        handle_cancel(request.params);
                    } catch (const ApiError& error) {
                        logging::warn(std::string("invalid cancel: ") + error.what());
                    }
                }
                continue;
            }

            if (request.method == "system/shutdown") {
                send_result(request.id, Json{{"ok", true}});
                running = false;
                break;
            }

            auto token = std::make_shared<CancelToken>();
            const Json request_id = request.id;
            const std::string request_key = request.id_key();
            {
                std::lock_guard lock(active_mutex);
                if (active.count(request_key) != 0) {
                    send_error(request.id, ErrorCode::InvalidRequest,
                               "Duplicate request id");
                    continue;
                }
                active.emplace(request_key, token);
            }
            const bool queued = pool.enqueue(
                [this, request = std::move(request), token]() mutable {
                    process_request(std::move(request), std::move(token));
                });
            if (!queued) {
                {
                    std::lock_guard lock(active_mutex);
                    active.erase(request_key);
                }
                send_error(request_id, ErrorCode::Overloaded, "Core is overloaded");
            }
        }

        running = false;
        watcher.stop();
        lsp.shutdown();
        pool.stop();
        {
            std::lock_guard lock(active_mutex);
            active.clear();
        }
        logging::security_event("core.stop", Json::object());
        return 0;
    }
};

Server::Server(ServerOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}

Server::~Server() = default;

int Server::run() { return impl_->run(); }

}  // namespace anvil
