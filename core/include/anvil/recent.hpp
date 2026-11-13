#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "anvil/error.hpp"

namespace anvil {

class RecentProjects {
public:
    void load(const std::filesystem::path& config_dir);
    void add(const std::filesystem::path& root, const std::string& name);
    bool remove(const std::string& path);
    [[nodiscard]] Json list() const;
    void save() const;

private:
    struct Entry {
        std::string path;
        std::string name;
        std::int64_t last_opened_ms = 0;
    };

    mutable std::mutex mutex_;
    std::filesystem::path config_dir_;
    std::vector<Entry> entries_;
};

class WorkspaceStateStore {
public:
    explicit WorkspaceStateStore(std::filesystem::path config_dir);

    [[nodiscard]] Json load(const std::string& state_key) const;
    void save(const std::string& state_key, const std::vector<std::string>& tabs,
              const std::string& active) const;

private:
    std::filesystem::path directory_;
};

}  // namespace anvil
