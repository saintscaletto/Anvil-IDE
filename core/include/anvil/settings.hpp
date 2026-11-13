#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "anvil/error.hpp"

namespace anvil {

// Shared validation helpers. The UI persists these values and the core must
// enforce the same rules at the IPC boundary and when settings.json is loaded.
[[nodiscard]] bool is_valid_font_family(std::string_view value);
[[nodiscard]] bool is_valid_theme_id(std::string_view value);
[[nodiscard]] bool is_valid_line_height_mode(std::string_view value);

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
    [[nodiscard]] std::string ui_theme() const;
    [[nodiscard]] std::string ui_font_family() const;
    [[nodiscard]] int ui_font_size() const;
    [[nodiscard]] std::string editor_font_family() const;
    [[nodiscard]] bool editor_font_ligatures() const;
    [[nodiscard]] std::string editor_line_height() const;

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
