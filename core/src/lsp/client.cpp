#include "anvil/lsp/client.hpp"

#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <thread>
#include <utility>

#include "anvil/ipc/jsonrpc.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/process.hpp"
#include "anvil/version.hpp"

namespace anvil::lsp {
namespace {

Json initialize_params(const std::string& root_uri) {
    return Json{
        {"processId", static_cast<long long>(::getpid())},
        {"clientInfo", {{"name", std::string(kProductName)}, {"version", std::string(kVersion)}}},
        {"locale", "en"},
        {"rootUri", root_uri},
        {"trace", "off"},
        {"workspaceFolders", Json::array({{{"uri", root_uri}, {"name", "workspace"}}})},
        {"capabilities",
         {{"workspace",
           {{"workspaceFolders", true},
            {"configuration", false},
            {"applyEdit", false},
            {"workspaceEdit", {{"documentChanges", false}}}}},
          {"textDocument",
           {{"synchronization",
             {{"dynamicRegistration", false},
              {"willSave", false},
              {"willSaveWaitUntil", false},
              {"didSave", true}}},
            {"completion",
             {{"dynamicRegistration", false},
              {"completionItem",
               {{"snippetSupport", true},
                {"documentationFormat", Json::array({"markdown", "plaintext"})},
                {"resolveSupport", {{"properties", Json::array({"documentation", "detail"})}}}}}}},
            {"hover",
             {{"dynamicRegistration", false},
              {"contentFormat", Json::array({"markdown", "plaintext"})}}},
            {"definition", {{"dynamicRegistration", false}, {"linkSupport", true}}},
            {"references", {{"dynamicRegistration", false}}},
            {"documentSymbol",
             {{"dynamicRegistration", false}, {"hierarchicalDocumentSymbolSupport", true}}},
            {"signatureHelp", {{"dynamicRegistration", false}}},
            {"rename", {{"dynamicRegistration", false}, {"prepareSupport", false}}},
            {"formatting", {{"dynamicRegistration", false}}}}}}},
    };
}

std::string lsp_error_message(const Json& error) {
    if (!error.is_object()) return "language server returned an invalid error";
    const std::string message = error.value("message", "language server error");
    return "LSP: " + message;
}

}  // namespace

std::unique_ptr<Client> Client::create(const ClientOptions& options,
                                       std::string* initialized_server_name,
                                       std::string* initialized_server_version) {
    if (options.program.empty()) {
        throw ApiError(ErrorCode::LspUnavailable, "No language server program configured");
    }

    security::SeccompProgram seccomp(options.seccomp_profile);
    security::SandboxCommand command =
        options.allow_unsandboxed_dev
            ? security::build_unsandboxed_dev_command(options.sandbox, seccomp, options.program,
                                                      options.arguments)
            : security::build_sandbox_command(options.sandbox, seccomp, options.program,
                                              options.arguments);

    SpawnRequest request;
    request.argv = command.argv;
    request.env = command.env;
    request.cwd = command.cwd;
    request.stdin_pipe = true;
    request.capture_stdout = true;
    request.capture_stderr = true;
    request.inherited_fd = command.inherited_fd;
    request.seccomp = command.sandboxed ? nullptr : &seccomp;

    std::string spawn_error;
    auto child = ChildProcess::spawn(request, &spawn_error);
    if (!child.has_value()) {
        throw ApiError(ErrorCode::LspUnavailable,
                       "Failed to start language server: " + spawn_error);
    }
    command.close_inherited_fd();

    auto client = std::unique_ptr<Client>(new Client());
    client->default_timeout_ms_ = options.default_timeout_ms;
    client->root_uri_ = options.root_uri;
    client->stderr_fd_ = child->release_stderr();
    const int stdout_fd = child->release_stdout();
    const int stdin_fd = child->release_stdin();
    client->transport_ = std::make_unique<Transport>(stdout_fd, stdin_fd);
    client->child_ = std::move(*child);
    client->running_ = true;

    client->stderr_thread_ = std::thread([ptr = client.get()] { ptr->stderr_loop(); });
    client->reader_thread_ = std::thread([ptr = client.get()] { ptr->reader_loop(); });

    try {
        Json result = client->request("initialize", initialize_params(options.root_uri), nullptr,
                                      20'000);
        client->initialize_result_ = result;
        if (result.contains("serverInfo") && result["serverInfo"].is_object()) {
            const auto& info = result["serverInfo"];
            client->server_name_ = info.value("name", "");
            client->server_version_ = info.value("version", "");
        }
        if (initialized_server_name != nullptr) *initialized_server_name = client->server_name_;
        if (initialized_server_version != nullptr) {
            *initialized_server_version = client->server_version_;
        }
        client->notify("initialized", Json::object());
        logging::info("language server ready: " + client->server_name_ + " " +
                      client->server_version_);
    } catch (...) {
        client->shutdown();
        throw;
    }
    return client;
}

Client::~Client() { shutdown(); }

Json Client::request(const std::string& method, Json params, const CancelToken* cancel_token,
                     int timeout_ms) {
    if (!running_.load()) {
        throw ApiError(ErrorCode::LspUnavailable, "Language server is not running");
    }
    const auto id = next_id_.fetch_add(1);
    auto promise = std::make_shared<std::promise<Json>>();
    std::future<Json> future = promise->get_future();
    {
        std::lock_guard lock(pending_mutex_);
        pending_[id] = promise;
    }

    Json message{{"jsonrpc", "2.0"},
                 {"id", id},
                 {"method", method},
                 {"params", std::move(params)}};
    if (!transport_->write_message(message)) {
        std::lock_guard lock(pending_mutex_);
        pending_.erase(id);
        running_ = false;
        throw ApiError(ErrorCode::LspUnavailable, "Language server pipe is closed");
    }

    const int effective_timeout = timeout_ms > 0 ? timeout_ms : default_timeout_ms_;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(effective_timeout);
    while (true) {
        if (cancel_token != nullptr && cancel_token->cancelled()) {
            notify("$/cancelRequest", Json{{"id", id}});
            std::lock_guard lock(pending_mutex_);
            pending_.erase(id);
            throw ApiError(ErrorCode::Cancelled, "Language server request cancelled");
        }
        if (future.wait_for(std::chrono::milliseconds(50)) == std::future_status::ready) break;
        if (!running_.load()) {
            std::lock_guard lock(pending_mutex_);
            pending_.erase(id);
            throw ApiError(ErrorCode::LspUnavailable, "Language server exited");
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            notify("$/cancelRequest", Json{{"id", id}});
            std::lock_guard lock(pending_mutex_);
            pending_.erase(id);
            throw ApiError(ErrorCode::LspUnavailable,
                           "Language server request timed out: " + method);
        }
    }

    {
        std::lock_guard lock(pending_mutex_);
        pending_.erase(id);
    }
    return future.get();
}

void Client::notify(const std::string& method, Json params) {
    if (transport_ == nullptr) return;
    Json message{{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}};
    if (!transport_->write_message(message)) {
        logging::warn("failed to write LSP notification: " + method);
    }
}

void Client::set_notification_handler(NotificationHandler handler) {
    std::lock_guard lock(handler_mutex_);
    notification_handler_ = std::move(handler);
}

void Client::set_request_handler(RequestHandler handler) {
    std::lock_guard lock(handler_mutex_);
    request_handler_ = std::move(handler);
}

void Client::set_log_handler(LogHandler handler) {
    std::lock_guard lock(handler_mutex_);
    log_handler_ = std::move(handler);
}

void Client::set_exit_handler(ExitHandler handler) {
    std::lock_guard lock(handler_mutex_);
    exit_handler_ = std::move(handler);
}

void Client::reader_loop() {
    while (!stopping_.load()) {
        Json message;
        std::string error;
        if (!transport_->read_message(&message, &error)) {
            if (!stopping_.load()) {
                logging::warn("language server stream ended: " + error);
            }
            break;
        }
        dispatch(std::move(message));
    }
    running_ = false;
    fail_pending("Language server exited");
    ExitHandler exit_handler;
    {
        std::lock_guard lock(handler_mutex_);
        exit_handler = exit_handler_;
    }
    if (exit_handler) exit_handler(-1);
}

void Client::stderr_loop() {
    if (stderr_fd_ < 0) return;
    FdLineReader reader(stderr_fd_);
    std::string line;
    bool too_large = false;
    std::string error;
    while (!stopping_.load()) {
        if (!reader.read_line(&line, 64U * 1024U, &too_large, &error)) {
            if (too_large) logging::warn("language server stderr line exceeded the limit");
            break;
        }
        const std::string safe_line = logging::redact(line);
        logging::debug("lsp stderr: " + safe_line);
        LogHandler handler;
        {
            std::lock_guard lock(handler_mutex_);
            handler = log_handler_;
        }
        if (handler) handler(safe_line);
    }
    ::close(stderr_fd_);
    stderr_fd_ = -1;
}

void Client::dispatch(Json message) {
    if (!message.is_object()) return;
    auto method = message.find("method");
    auto id = message.find("id");
    if (method != message.end() && method->is_string()) {
        if (id != message.end()) {
            handle_server_request(message);
        } else {
            NotificationHandler handler;
            {
                std::lock_guard lock(handler_mutex_);
                handler = notification_handler_;
            }
            if (handler) {
                handler(method->get<std::string>(), message.value("params", Json::object()));
            }
        }
        return;
    }
    if (id == message.end()) return;
    if (!id->is_number_integer()) return;
    const int64_t numeric_id = id->get<int64_t>();

    std::shared_ptr<std::promise<Json>> promise;
    {
        std::lock_guard lock(pending_mutex_);
        auto it = pending_.find(numeric_id);
        if (it == pending_.end()) return;
        promise = it->second;
        pending_.erase(it);
    }

    auto error = message.find("error");
    if (error != message.end()) {
        try {
            promise->set_exception(std::make_exception_ptr(ApiError(
                ErrorCode::InternalError, lsp_error_message(*error),
                Json{{"lspError", *error}})));
        } catch (const std::future_error&) {
        }
        return;
    }
    try {
        promise->set_value(message.value("result", Json()));
    } catch (const std::future_error&) {
    }
}

void Client::handle_server_request(const Json& message) {
    const std::string method = message.value("method", "");
    const Json id = message.at("id");
    Json params = message.value("params", Json::object());
    Json result = nullptr;
    bool handled = false;

    RequestHandler handler;
    {
        std::lock_guard lock(handler_mutex_);
        handler = request_handler_;
    }
    if (handler) {
        try {
            result = handler(method, params);
            handled = true;
        } catch (const ApiError& error) {
            transport_->write_message(
                ipc::make_error(id, error.code(), error.what(), error.data()));
            return;
        } catch (const std::exception& error) {
            transport_->write_message(
                ipc::make_error(id, ErrorCode::InternalError, error.what()));
            return;
        }
    }

    if (!handled) {
        if (method == "workspace/configuration") {
            result = Json::array();
            handled = true;
        } else if (method == "client/registerCapability" ||
                   method == "client/unregisterCapability") {
            result = nullptr;
            handled = true;
        } else if (method == "window/workDoneProgress/create") {
            result = nullptr;
            handled = true;
        } else if (method == "window/showMessageRequest") {
            result = nullptr;
            handled = true;
        } else if (method == "workspace/applyEdit") {
            result = Json{{"applied", false}};
            handled = true;
        } else if (method == "workspace/workspaceFolders") {
            result = Json::array({{{"uri", root_uri_}, {"name", "workspace"}}});
            handled = true;
        }
    }

    if (handled) {
        transport_->write_message(ipc::make_result(id, std::move(result)));
    } else {
        transport_->write_message(
            ipc::make_error(id, ErrorCode::MethodNotFound, "Unsupported server request: " + method));
    }
}

void Client::fail_pending(const std::string& reason) {
    std::unordered_map<int64_t, std::shared_ptr<std::promise<Json>>> pending;
    {
        std::lock_guard lock(pending_mutex_);
        pending.swap(pending_);
    }
    for (auto& [id, promise] : pending) {
        (void)id;
        try {
            promise->set_exception(
                std::make_exception_ptr(ApiError(ErrorCode::LspUnavailable, reason)));
        } catch (const std::future_error&) {
        }
    }
}

void Client::shutdown() {
    std::lock_guard shutdown_lock(shutdown_mutex_);
    if (stopping_.exchange(true)) return;
    alive_->store(false);
    fail_pending("Language server shut down");
    if (transport_ != nullptr) {
        try {
            if (running_.load()) {
                Json params = Json::object();
                const auto id = next_id_.fetch_add(1);
                transport_->write_message(
                    Json{{"jsonrpc", "2.0"}, {"id", id}, {"method", "shutdown"}, {"params", params}});
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                transport_->write_message(
                    Json{{"jsonrpc", "2.0"}, {"method", "exit"}, {"params", params}});
            }
        } catch (...) {
        }
        transport_->close_write();
    }
    running_ = false;
    child_.terminate();
    if (reader_thread_.joinable()) reader_thread_.join();
    if (stderr_thread_.joinable()) stderr_thread_.join();
    if (transport_ != nullptr) transport_->close_read();
    if (stderr_fd_ >= 0) {
        ::close(stderr_fd_);
        stderr_fd_ = -1;
    }
    logging::info("language server stopped");
}

}  // namespace anvil::lsp
