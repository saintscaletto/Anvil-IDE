#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "anvil/error.hpp"

namespace anvil {

// Recursive inotify watcher restricted to real directories inside the
// workspace. Symlinked directories are never followed. Events are debounced by
// batching every poll cycle and delivered through a thread-safe callback.
class Watcher {
public:
    using EventSink = std::function<void(Json)>;

    Watcher() = default;
    ~Watcher();

    Watcher(const Watcher&) = delete;
    Watcher& operator=(const Watcher&) = delete;

    void start(const std::filesystem::path& root, EventSink sink);
    void stop();
    [[nodiscard]] bool running() const noexcept { return running_; }

private:
    void loop(const std::stop_token& stop_token);
    bool add_watch(const std::filesystem::path& directory);
    void add_watch_tree(const std::filesystem::path& directory, int depth);
    void remove_watch_tree(const std::string& relative_path);
    bool ignored(const std::filesystem::path& path) const;
    void emit_changes(const std::map<std::string, std::string>& changes) const;

    std::filesystem::path root_;
    EventSink sink_;
    int inotify_fd_ = -1;
    int stop_fd_ = -1;
    std::jthread thread_;
    std::atomic<bool> running_{false};
    mutable std::mutex sink_mutex_;
    std::unordered_map<int, std::string> watch_paths_;
    std::set<std::string> ignored_names_;
    std::size_t watch_count_ = 0;
};

}  // namespace anvil
