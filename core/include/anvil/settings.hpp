#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "anvil/error.hpp"

namespace anvil {

class Settings {
public:
    void load(const std::filesystem::path& config_dir);
    void save() const;

    [[nodiscard]] Json snapshot() const;
    void set(const std::string& key, const Json& value);

    [[nodiscard]] int font_size() const;
    [[nodiscard]] int tab_size() const;
    [[nodiscard]] bool word_wrap() const;
    [[nodiscard]] bool minimap() const;
    [[nodiscard]] bool format_on_save() const;
    [[nodiscard]] std::string clangd_path() const;
    [[nodiscard]] std::vector<std::string> clangd_args() const;
    [[nodiscard]] bool enable_project_config() const;
    [[nodiscard]] bool allow_unsandboxed_lsp() const;
    [[nodiscard]] bool hide_git() const;

    [[nodiscard]] std::filesystem::path file_path() const;

private:
    void reset_defaults();
    void apply_loaded(const Json& value);
    void validate_assignment(const std::string& key, const Json& value) const;

    mutable std::mutex mutex_;
    std::filesystem::path config_dir_;
    Json values_ = Json::object();
};

}  // namespace anvil
