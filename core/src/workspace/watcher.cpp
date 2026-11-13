#include "anvil/workspace/watcher.hpp"

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <utility>

#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/strings.hpp"

namespace anvil {
namespace fs = std::filesystem;
namespace {

constexpr std::size_t kMaxWatches = 4096;
constexpr int kMaxWatchDepth = 32;
constexpr std::size_t kMaxChangesPerEvent = 500;
constexpr std::uint32_t kWatchMask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_ATTRIB |
                                    IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE |
                                    IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR;

std::string event_kind(std::uint32_t mask) {
    if ((mask & IN_CREATE) != 0) return "created";
    if ((mask & IN_DELETE) != 0 || (mask & IN_DELETE_SELF) != 0) return "deleted";
    if ((mask & IN_MOVED_FROM) != 0 || (mask & IN_MOVED_TO) != 0) return "renamed";
    if ((mask & IN_MODIFY) != 0 || (mask & IN_CLOSE_WRITE) != 0 ||
        (mask & IN_ATTRIB) != 0) {
        return "modified";
    }
    return "modified";
}

std::string join_relative(const std::string& base, const std::string& name) {
    if (base.empty()) return name;
    if (name.empty()) return base;
    return base + "/" + name;
}

bool safe_name(const std::string& name) {
    return !name.empty() && name != "." && name != ".." &&
           name.find('/') == std::string::npos && name.find('\0') == std::string::npos;
}

}  // namespace

Watcher::~Watcher() { stop(); }

void Watcher::start(const fs::path& root, EventSink sink) {
    stop();
    root_ = fs::canonical(root);
    sink_ = std::move(sink);
    ignored_names_ = {".git", "node_modules", "build", "target", ".cache", "dist", ".anvil"};
    watch_paths_.clear();
    watch_count_ = 0;

    inotify_fd_ = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    stop_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (inotify_fd_ < 0 || stop_fd_ < 0) {
        const std::string reason = std::string("inotify unavailable: ") + std::strerror(errno);
        logging::warn(reason);
        if (inotify_fd_ >= 0) ::close(inotify_fd_);
        if (stop_fd_ >= 0) ::close(stop_fd_);
        inotify_fd_ = -1;
        stop_fd_ = -1;
        if (sink_) sink_(Json{{"type", "watchUnavailable"}, {"reason", reason}});
        return;
    }

    add_watch_tree(root_, 0);
    running_ = true;
    thread_ = std::jthread([this](const std::stop_token& token) { loop(token); });
}

void Watcher::stop() {
    if (thread_.joinable()) {
        thread_.request_stop();
        if (stop_fd_ >= 0) {
            const std::uint64_t value = 1;
            const ssize_t ignored = ::write(stop_fd_, &value, sizeof(value));
            (void)ignored;
        }
        thread_.join();
    }
    running_ = false;
    if (inotify_fd_ >= 0) {
        ::close(inotify_fd_);
        inotify_fd_ = -1;
    }
    if (stop_fd_ >= 0) {
        ::close(stop_fd_);
        stop_fd_ = -1;
    }
    watch_paths_.clear();
    watch_count_ = 0;
}

bool Watcher::ignored(const fs::path& path) const {
    if (path == root_) return false;
    const auto relative = fsutil::relative_to(root_, path);
    if (!relative.has_value()) return true;
    for (const auto& component : fs::path(*relative)) {
        if (ignored_names_.count(component.string()) != 0) return true;
    }
    return false;
}

bool Watcher::add_watch(const fs::path& directory) {
    if (inotify_fd_ < 0 || watch_count_ >= kMaxWatches) return false;
    std::error_code ec;
    if (!fs::is_directory(directory, ec) || fs::is_symlink(fs::symlink_status(directory, ec))) {
        return false;
    }
    const auto relative = fsutil::relative_to(root_, directory);
    if (!relative.has_value()) return false;
    if (ignored(directory)) return false;

    const int wd = ::inotify_add_watch(inotify_fd_, directory.c_str(), kWatchMask);
    if (wd < 0) {
        logging::warn("inotify_add_watch failed for " + directory.string() + ": " +
                      std::strerror(errno));
        return false;
    }
    watch_paths_[wd] = *relative;
    ++watch_count_;
    return true;
}

void Watcher::add_watch_tree(const fs::path& directory, int depth) {
    if (depth > kMaxWatchDepth || watch_count_ >= kMaxWatches) return;
    if (!add_watch(directory)) return;
    std::error_code ec;
    fs::directory_iterator iterator(directory, fs::directory_options::skip_permission_denied, ec);
    if (ec) return;
    for (const auto& entry : iterator) {
        if (watch_count_ >= kMaxWatches) return;
        const auto status = entry.symlink_status(ec);
        if (ec || !fs::is_directory(status) || fs::is_symlink(status)) continue;
        if (ignored(entry.path())) continue;
        add_watch_tree(entry.path(), depth + 1);
    }
}

void Watcher::remove_watch_tree(const std::string& relative_path) {
    for (auto it = watch_paths_.begin(); it != watch_paths_.end();) {
        const std::string& watched = it->second;
        if (watched == relative_path ||
            strings::starts_with(watched, relative_path + "/")) {
            ::inotify_rm_watch(inotify_fd_, it->first);
            it = watch_paths_.erase(it);
            if (watch_count_ > 0) --watch_count_;
        } else {
            ++it;
        }
    }
}

void Watcher::emit_changes(const std::map<std::string, std::string>& changes) const {
    if (changes.empty()) return;
    Json items = Json::array();
    std::size_t count = 0;
    for (const auto& [path, kind] : changes) {
        if (count++ >= kMaxChangesPerEvent) break;
        items.push_back(Json{{"path", path}, {"kind", kind}});
    }
    EventSink sink;
    {
        std::lock_guard lock(sink_mutex_);
        sink = sink_;
    }
    if (sink) sink(Json{{"type", "changed"}, {"changes", std::move(items)}});
}

void Watcher::loop(const std::stop_token& stop_token) {
    std::array<char, 64U * 1024U> buffer{};
    std::map<std::string, std::string> changes;

    while (!stop_token.stop_requested()) {
        struct pollfd fds[2];
        fds[0] = {.fd = inotify_fd_, .events = POLLIN, .revents = 0};
        fds[1] = {.fd = stop_fd_, .events = POLLIN, .revents = 0};
        const int ready = ::poll(fds, 2, 200);
        if (ready < 0) {
            if (errno == EINTR) continue;
            logging::warn(std::string("inotify poll failed: ") + std::strerror(errno));
            break;
        }
        if ((fds[1].revents & POLLIN) != 0 || stop_token.stop_requested()) break;
        if ((fds[0].revents & POLLIN) == 0) {
            emit_changes(changes);
            changes.clear();
            continue;
        }

        while (true) {
            const ssize_t count = ::read(inotify_fd_, buffer.data(), buffer.size());
            if (count < 0) {
                if (errno == EAGAIN) break;
                if (errno == EINTR) continue;
                logging::warn(std::string("inotify read failed: ") + std::strerror(errno));
                break;
            }
            if (count == 0) break;
            ssize_t offset = 0;
            while (offset < count) {
                const auto* event = reinterpret_cast<const struct inotify_event*>(
                    buffer.data() + offset);
                offset += static_cast<ssize_t>(sizeof(struct inotify_event) + event->len);
                if ((event->mask & IN_Q_OVERFLOW) != 0) {
                    EventSink sink;
                    {
                        std::lock_guard lock(sink_mutex_);
                        sink = sink_;
                    }
                    if (sink) sink(Json{{"type", "rescan"}});
                    continue;
                }
                if ((event->mask & IN_IGNORED) != 0) {
                    watch_paths_.erase(event->wd);
                    continue;
                }
                const auto watched = watch_paths_.find(event->wd);
                if (watched == watch_paths_.end()) continue;
                std::string name;
                if (event->len > 0) {
                    name.assign(event->name);
                    if (!safe_name(name)) continue;
                }
                const std::string relative = join_relative(watched->second, name);

                if ((event->mask & (IN_CREATE | IN_MOVED_TO)) != 0 &&
                    (event->mask & IN_ISDIR) != 0 && !name.empty()) {
                    add_watch_tree(root_ / fs::path(relative), 0);
                }
                if (((event->mask & IN_DELETE) != 0 || (event->mask & IN_MOVED_FROM) != 0) &&
                    (event->mask & IN_ISDIR) != 0 && !name.empty()) {
                    remove_watch_tree(relative);
                }
                if ((event->mask & IN_IGNORED) == 0 && !relative.empty()) {
                    changes[relative] = event_kind(event->mask);
                }
            }
        }
        emit_changes(changes);
        changes.clear();
    }
    logging::info("workspace watcher stopped");
}

}  // namespace anvil
