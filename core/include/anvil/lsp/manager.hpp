#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <vector>

#include "anvil/ipc/validate.hpp"
#include "anvil/lsp/client.hpp"
#include "anvil/settings.hpp"
#include "anvil/workspace/workspace.hpp"

namespace anvil::lsp {

// Manages at most one language server per language family. Only C and C++ are
// wired in 0.0.1; the interface is generic so Rust/Python can be added without
// changing the IPC contract.
class Manager {
public:
    using EventSink = std::function<void(const std::string&, Json)>;

    Manager(const Workspace& workspace, const Settings& settings,
            std::filesystem::path cache_dir);
    ~Manager();

    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;

    void set_event_sink(EventSink sink);
    void on_workspace_opened();
    void shutdown();

    Json ensure(const std::string& path, const std::string& language_id);
    Json status() const;

    void did_open(const ipc::validate::LspDidOpen& params);
    Json did_change(const ipc::validate::LspDidChange& params);
    void did_save(const std::string& path);
    void did_close(const std::string& path);

    Json hover(const ipc::validate::LspPosition& params, const CancelToken* token);
    Json completion(const ipc::validate::LspCompletion& params, const CancelToken* token);
    Json definition(const ipc::validate::LspPosition& params, const CancelToken* token);
    Json references(const ipc::validate::LspReferences& params, const CancelToken* token);
    Json document_symbols(const ipc::validate::LspPath& params, const CancelToken* token);
    Json signature_help(const ipc::validate::LspPosition& params, const CancelToken* token);
    Json rename(const ipc::validate::LspRename& params, const CancelToken* token);
    Json format(const ipc::validate::LspFormat& params, const CancelToken* token);
    void restart(const std::string& language_id);

    struct State;

private:
    static std::string family_for(const std::string& language_id);
    std::shared_ptr<State> state_for(const std::string& family);
    std::shared_ptr<State> ensure_started(const std::string& family);
    void emit(const std::string& method, Json params) const;
    void emit_status(const std::shared_ptr<State>& state);
    void handle_notification(const std::shared_ptr<State>& state, const std::string& method,
                             const Json& params);
    void stop_state(const std::shared_ptr<State>& state);
    void janitor_loop(const std::stop_token& stop_token);

    const Workspace& workspace_;
    const Settings& settings_;
    std::filesystem::path cache_dir_;

    mutable std::mutex map_mutex_;
    std::unordered_map<std::string, std::shared_ptr<State>> states_;
    mutable std::mutex sink_mutex_;
    EventSink sink_;

    std::mutex sync_mutex_;
    std::jthread janitor_;
    std::atomic<bool> shutting_down_{false};
};

}  // namespace anvil::lsp
