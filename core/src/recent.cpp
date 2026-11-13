#include "anvil/recent.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <utility>

#include "anvil/ipc/validate.hpp"
#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/strings.hpp"

namespace anvil {
namespace fs = std::filesystem;

namespace {

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool safe_state_key(const std::string& key) {
    if (key.empty() || key.size() > 128) return false;
    for (const char c : key) {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!allowed) return false;
    }
    return true;
}

}  // namespace

void RecentProjects::load(const fs::path& config_dir) {
    std::lock_guard lock(mutex_);
    config_dir_ = config_dir;
    entries_.clear();
    std::string error;
    const auto value = fsutil::try_read_json_file(config_dir / "recent.json",
                                                  256U * 1024U, &error);
    if (!value.has_value() || !value->is_array()) {
        if (fs::exists(config_dir / "recent.json")) {
            logging::warn("recent.json ignored: " + error);
        }
        return;
    }

    for (const auto& item : *value) {
        if (entries_.size() >= 20) break;
        if (!item.is_object()) continue;
        if (!item.contains("path") || !item["path"].is_string()) continue;
        if (!item.contains("name") || !item["name"].is_string()) continue;
        const std::string path = item["path"].get<std::string>();
        const std::string name = item["name"].get<std::string>();
        if (path.empty() || path.size() > 4096 || name.size() > 256) continue;
        if (path.find('\0') != std::string::npos) continue;
        std::error_code ec;
        if (!fs::is_directory(path, ec)) continue;
        Entry entry;
        entry.path = path;
        entry.name = name;
        entry.last_opened_ms = item.value("lastOpened", static_cast<std::int64_t>(0));
        entries_.push_back(std::move(entry));
    }
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        return a.last_opened_ms > b.last_opened_ms;
    });
}

void RecentProjects::add(const fs::path& root, const std::string& name) {
    std::error_code ec;
    const fs::path canonical = fs::canonical(root, ec);
    if (ec) return;
    std::lock_guard lock(mutex_);
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&](const Entry& entry) { return entry.path == canonical; }),
                   entries_.end());
    Entry entry;
    entry.path = canonical.string();
    entry.name = name.empty() ? canonical.filename().string() : name;
    entry.last_opened_ms = now_ms();
    entries_.insert(entries_.begin(), std::move(entry));
    if (entries_.size() > 20) entries_.resize(20);
}

bool RecentProjects::remove(const std::string& path) {
    std::lock_guard lock(mutex_);
    const auto before = entries_.size();
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&](const Entry& entry) { return entry.path == path; }),
                   entries_.end());
    return entries_.size() != before;
}

Json RecentProjects::list() const {
    std::lock_guard lock(mutex_);
    Json result = Json::array();
    for (const auto& entry : entries_) {
        result.push_back(Json{{"path", entry.path},
                              {"name", entry.name},
                              {"lastOpened", entry.last_opened_ms}});
    }
    return result;
}

void RecentProjects::save() const {
    std::lock_guard lock(mutex_);
    if (config_dir_.empty()) return;
    Json value = Json::array();
    for (const auto& entry : entries_) {
        value.push_back(Json{{"path", entry.path},
                             {"name", entry.name},
                             {"lastOpened", entry.last_opened_ms}});
    }
    try {
        fsutil::ensure_private_directory(config_dir_);
        fsutil::write_json_file_atomic(config_dir_ / "recent.json", value);
    } catch (const std::exception& error) {
        logging::error(std::string("failed to save recent projects: ") + error.what());
    }
}

WorkspaceStateStore::WorkspaceStateStore(fs::path config_dir)
    : directory_(std::move(config_dir) / "workspace") {}

Json WorkspaceStateStore::load(const std::string& state_key) const {
    if (!safe_state_key(state_key)) return Json{{"tabs", Json::array()}, {"active", ""}};
    std::string error;
    const auto value = fsutil::try_read_json_file(directory_ / (state_key + ".json"),
                                                  256U * 1024U, &error);
    if (!value.has_value()) {
        return Json{{"tabs", Json::array()}, {"active", ""}};
    }
    try {
        const auto state = ipc::validate::parse_workspace_save_state(*value);
        Json tabs = Json::array();
        for (const auto& tab : state.tabs) tabs.push_back(tab);
        return Json{{"tabs", tabs}, {"active", state.active}};
    } catch (const ApiError& parse_error) {
        logging::warn(std::string("workspace state ignored: ") + parse_error.what());
        return Json{{"tabs", Json::array()}, {"active", ""}};
    }
}

void WorkspaceStateStore::save(const std::string& state_key,
                               const std::vector<std::string>& tabs,
                               const std::string& active) const {
    if (!safe_state_key(state_key)) return;
    Json params{{"tabs", Json::array()}, {"active", active}};
    for (const auto& tab : tabs) params["tabs"].push_back(tab);
    try {
        const auto validated = ipc::validate::parse_workspace_save_state(params);
        Json tabs_json = Json::array();
        for (const auto& tab : validated.tabs) tabs_json.push_back(tab);
        fsutil::ensure_private_directory(directory_);
        fsutil::write_json_file_atomic(directory_ / (state_key + ".json"),
                                       Json{{"tabs", tabs_json}, {"active", validated.active}});
    } catch (const ApiError& error) {
        logging::security_event("workspace.state_rejected", Json{{"reason", error.what()}});
    } catch (const std::exception& error) {
        logging::error(std::string("failed to save workspace state: ") + error.what());
    }
}

}  // namespace anvil
