#pragma once

#include <sys/types.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "anvil/error.hpp"
#include "anvil/ipc/cancel.hpp"
#include "anvil/lsp/transport.hpp"
#include "anvil/security/sandbox.hpp"
#include "anvil/security/seccomp.hpp"
#include "anvil/util/process.hpp"

namespace anvil::lsp {

struct ClientOptions {
    std::string program;
    std::vector<std::string> arguments;
    std::string workspace_relative_root;
    std::string root_uri;
    security::SandboxSpec sandbox;
    security::SeccompProfile seccomp_profile = security::SeccompProfile::Lsp;
    bool allow_unsandboxed_dev = false;
    int default_timeout_ms = 15'000;
};

class Client {
public:
    using NotificationHandler = std::function<void(const std::string&, const Json&)>;
    using RequestHandler = std::function<Json(const std::string&, const Json&)>;
    using LogHandler = std::function<void(const std::string&)>;
    using ExitHandler = std::function<void(int)>;

    // Spawns the server, completes the LSP initialize handshake and starts the
    // reader threads. Throws ApiError on failure; the child is cleaned up.
    static std::unique_ptr<Client> create(const ClientOptions& options,
                                          std::string* initialized_server_name,
                                          std::string* initialized_server_version);

    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    Json request(const std::string& method, Json params, const CancelToken* cancel_token,
                 int timeout_ms = 0);
    void notify(const std::string& method, Json params);

    [[nodiscard]] bool running() const noexcept { return running_.load(); }
    [[nodiscard]] pid_t pid() const noexcept { return child_.pid(); }
    [[nodiscard]] const Json& initialize_result() const noexcept { return initialize_result_; }

    void set_notification_handler(NotificationHandler handler);
    void set_request_handler(RequestHandler handler);
    void set_log_handler(LogHandler handler);
    void set_exit_handler(ExitHandler handler);

    void shutdown();

private:
    Client() = default;

    void reader_loop();
    void stderr_loop();
    void dispatch(Json message);
    void fail_pending(const std::string& reason);
    void handle_server_request(const Json& message);

    std::mutex shutdown_mutex_;
    ChildProcess child_;
    std::unique_ptr<Transport> transport_;
    int stderr_fd_ = -1;
    std::thread reader_thread_;
    std::thread stderr_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    std::atomic<std::int64_t> next_id_{1};
    std::mutex pending_mutex_;
    std::unordered_map<std::int64_t, std::shared_ptr<std::promise<Json>>> pending_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    Json initialize_result_ = Json::object();
    std::string root_uri_;
    std::string server_name_;
    std::string server_version_;
    int default_timeout_ms_ = 15'000;

    std::mutex handler_mutex_;
    NotificationHandler notification_handler_;
    RequestHandler request_handler_;
    LogHandler log_handler_;
    ExitHandler exit_handler_;
};

}  // namespace anvil::lsp
