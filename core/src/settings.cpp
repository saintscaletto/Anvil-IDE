#include "anvil/settings.hpp"

#include <cctype>
#include <utility>

#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/strings.hpp"

namespace anvil {
namespace {

constexpr const char* kFontSize = "editor.fontSize";
constexpr const char* kTabSize = "editor.tabSize";
constexpr const char* kWordWrap = "editor.wordWrap";
constexpr const char* kMinimap = "editor.minimap";
constexpr const char* kFormatOnSave = "editor.formatOnSave";
constexpr const char* kClangdPath = "lsp.clangdPath";
constexpr const char* kClangdArgs = "lsp.clangdArgs";
constexpr const char* kEnableProjectConfig = "lsp.enableProjectConfig";
constexpr const char* kAllowUnsandboxedLsp = "security.allowUnsandboxedLsp";
constexpr const char* kHideGit = "explorer.hideGit";
constexpr const char* kUiTheme = "ui.theme";
constexpr const char* kUiFontFamily = "ui.fontFamily";
constexpr const char* kUiFontSize = "ui.fontSize";
constexpr const char* kEditorFontFamily = "editor.fontFamily";
constexpr const char* kEditorFontLigatures = "editor.fontLigatures";
constexpr const char* kEditorLineHeight = "editor.lineHeight";
constexpr const char* kUiLanguage = "ui.language";

bool is_known_key(const std::string& key) {
    return key == kFontSize || key == kTabSize || key == kWordWrap || key == kMinimap ||
           key == kFormatOnSave || key == kClangdPath || key == kClangdArgs ||
           key == kEnableProjectConfig || key == kAllowUnsandboxedLsp || key == kHideGit ||
           key == kUiTheme || key == kUiFontFamily || key == kUiFontSize ||
           key == kEditorFontFamily || key == kEditorFontLigatures ||
           key == kEditorLineHeight || key == kUiLanguage;
}

int bounded_int(const Json& value, int fallback, int min_value, int max_value) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) return fallback;
    std::int64_t number = 0;
    if (value.is_number_unsigned()) {
        const auto unsigned_value = value.get<std::uint64_t>();
        if (unsigned_value > static_cast<std::uint64_t>(max_value)) return fallback;
        number = static_cast<std::int64_t>(unsigned_value);
    } else {
        number = value.get<std::int64_t>();
    }
    if (number < min_value || number > max_value) return fallback;
    return static_cast<int>(number);
}

bool bool_or(const Json& value, bool fallback) {
    return value.is_boolean() ? value.get<bool>() : fallback;
}

std::string string_or(const Json& value, const std::string& fallback) {
    return value.is_string() ? value.get<std::string>() : fallback;
}

std::vector<std::string> string_array_or(const Json& value) {
    std::vector<std::string> result;
    if (!value.is_array() || value.size() > 32) return result;
    for (const auto& item : value) {
        if (!item.is_string()) return {};
        const std::string text = item.get<std::string>();
        if (text.size() > 256 || text.find('\0') != std::string::npos) return {};
        result.push_back(text);
    }
    return result;
}

}  // namespace

// Font families are data, never CSS. Only printable family characters are
// accepted; quotes, braces, semicolons and control characters are rejected so
// a persisted value cannot break out of the CSS declaration built by the UI.
bool is_valid_font_family(std::string_view value) {
    if (value.size() > 128) return false;
    if (!strings::is_valid_utf8(value)) return false;
    for (const unsigned char c : value) {
        if (c < 0x20 || c == 0x7f) return false;
        if (c >= 0x80) continue;
        if (std::isalnum(c) != 0 || c == ' ' || c == '-' || c == '_' || c == '.' || c == '+') {
            continue;
        }
        return false;
    }
    return true;
}

bool is_valid_theme_id(std::string_view value) {
    return value == "auto" || value == "dark" || value == "light" || value == "high-contrast";
}

bool is_valid_line_height_mode(std::string_view value) {
    return value == "compact" || value == "normal" || value == "relaxed";
}

bool is_valid_language_preference(std::string_view value) {
    return value == "auto" || value == "en" || value == "ru";
}

void Settings::reset_defaults() {
    values_ = Json{
        {kFontSize, 14},
        {kTabSize, 4},
        {kWordWrap, false},
        {kMinimap, true},
        {kFormatOnSave, false},
        {kClangdPath, "clangd"},
        {kClangdArgs, Json::array()},
        {kEnableProjectConfig, false},
        {kAllowUnsandboxedLsp, false},
        {kHideGit, true},
        {kUiTheme, "auto"},
        {kUiFontFamily, ""},
        {kUiFontSize, 12},
        {kEditorFontFamily, ""},
        {kEditorFontLigatures, true},
        {kEditorLineHeight, "normal"},
        {kUiLanguage, "auto"},
    };
}

void Settings::load(const std::filesystem::path& config_dir) {
    std::lock_guard lock(mutex_);
    config_dir_ = config_dir;
    reset_defaults();
    const auto path = config_dir_ / "settings.json";
    std::string error;
    const auto loaded = fsutil::try_read_json_file(path, 256U * 1024U, &error);
    if (!loaded.has_value()) {
        if (std::filesystem::exists(path)) {
            logging::warn("settings.json could not be loaded: " + error);
        }
        return;
    }
    if (!loaded->is_object()) {
        logging::warn("settings.json is not a JSON object; using defaults");
        return;
    }
    apply_loaded(*loaded);
    logging::info("settings loaded from " + path.string());
}

void Settings::apply_loaded(const Json& value) {
    for (const auto& [key, item] : value.items()) {
        if (!is_known_key(key)) {
            logging::warn("unknown setting ignored: " + key);
            continue;
        }
        try {
            validate_assignment(key, item);
            values_[key] = item;
        } catch (const ApiError& error) {
            logging::warn("invalid setting ignored (" + key + "): " + error.what());
        }
    }
}

void Settings::validate_assignment(const std::string& key, const Json& value) const {
    if (key == kFontSize) {
        if (!value.is_number_integer()) {
            throw ApiError(ErrorCode::InvalidParams, "fontSize must be an integer");
        }
        const auto number = value.get<std::int64_t>();
        if (number < 8 || number > 32) {
            throw ApiError(ErrorCode::InvalidParams, "fontSize must be 8..32");
        }
    } else if (key == kTabSize) {
        if (!value.is_number_integer()) {
            throw ApiError(ErrorCode::InvalidParams, "tabSize must be an integer");
        }
        const auto number = value.get<std::int64_t>();
        if (number < 1 || number > 8) {
            throw ApiError(ErrorCode::InvalidParams, "tabSize must be 1..8");
        }
    } else if (key == kWordWrap || key == kMinimap || key == kFormatOnSave ||
               key == kEnableProjectConfig || key == kAllowUnsandboxedLsp || key == kHideGit) {
        if (!value.is_boolean()) {
            throw ApiError(ErrorCode::InvalidParams, key + " must be boolean");
        }
    } else if (key == kClangdPath) {
        if (!value.is_string()) {
            throw ApiError(ErrorCode::InvalidParams, "lsp.clangdPath must be a string");
        }
        const std::string path = value.get<std::string>();
        if (path.empty() || path.size() > 1024 || path.find('\0') != std::string::npos) {
            throw ApiError(ErrorCode::InvalidParams, "Invalid lsp.clangdPath");
        }
        if (path.find('/') != std::string::npos && path.front() != '/') {
            throw ApiError(ErrorCode::InvalidParams,
                           "lsp.clangdPath must be absolute when it contains a slash");
        }
    } else if (key == kClangdArgs) {
        if (!value.is_array() || value.size() > 32) {
            throw ApiError(ErrorCode::InvalidParams, "lsp.clangdArgs must be a small array");
        }
        for (const auto& item : value) {
            if (!item.is_string() || item.get<std::string>().size() > 256 ||
                item.get<std::string>().find('\0') != std::string::npos) {
                throw ApiError(ErrorCode::InvalidParams, "Invalid lsp.clangdArgs entry");
            }
        }
    } else if (key == kUiTheme) {
        if (!value.is_string()) {
            throw ApiError(ErrorCode::InvalidParams, "ui.theme must be a string");
        }
        const std::string theme = value.get<std::string>();
        if (!is_valid_theme_id(theme)) {
            throw ApiError(ErrorCode::InvalidParams, "Invalid ui.theme");
        }
    } else if (key == kEditorLineHeight) {
        if (!value.is_string()) {
            throw ApiError(ErrorCode::InvalidParams, "editor.lineHeight must be a string");
        }
        const std::string mode = value.get<std::string>();
        if (!is_valid_line_height_mode(mode)) {
            throw ApiError(ErrorCode::InvalidParams, "Invalid editor.lineHeight");
        }
    } else if (key == kUiFontSize) {
        if (!value.is_number_integer()) {
            throw ApiError(ErrorCode::InvalidParams, "ui.fontSize must be an integer");
        }
        const auto number = value.get<std::int64_t>();
        if (number < 11 || number > 18) {
            throw ApiError(ErrorCode::InvalidParams, "ui.fontSize must be 11..18");
        }
    } else if (key == kUiFontFamily || key == kEditorFontFamily) {
        if (!value.is_string()) {
            throw ApiError(ErrorCode::InvalidParams, key + " must be a string");
        }
        if (!is_valid_font_family(value.get<std::string>())) {
            throw ApiError(ErrorCode::InvalidParams, "Invalid " + key);
        }
    } else if (key == kEditorFontLigatures) {
        if (!value.is_boolean()) {
            throw ApiError(ErrorCode::InvalidParams, key + " must be boolean");
        }
    } else if (key == kUiLanguage) {
        if (!value.is_string()) {
            throw ApiError(ErrorCode::InvalidParams, "ui.language must be a string");
        }
        if (!is_valid_language_preference(value.get<std::string>())) {
            throw ApiError(ErrorCode::InvalidParams, "Invalid ui.language");
        }
    }
}

void Settings::save() const {
    std::lock_guard lock(mutex_);
    if (config_dir_.empty()) return;
    try {
        fsutil::ensure_private_directory(config_dir_);
        fsutil::write_json_file_atomic(config_dir_ / "settings.json", values_);
    } catch (const std::exception& error) {
        logging::error(std::string("failed to save settings: ") + error.what());
    }
}

Json Settings::snapshot() const {
    std::lock_guard lock(mutex_);
    return values_;
}

void Settings::set(const std::string& key, const Json& value) {
    {
        std::lock_guard lock(mutex_);
        if (!is_known_key(key)) {
            throw ApiError(ErrorCode::InvalidParams, "Unknown setting: " + key);
        }
        validate_assignment(key, value);
        values_[key] = value;
    }
    save();
}

int Settings::font_size() const {
    std::lock_guard lock(mutex_);
    return bounded_int(values_.value(kFontSize, Json(14)), 14, 8, 32);
}

int Settings::tab_size() const {
    std::lock_guard lock(mutex_);
    return bounded_int(values_.value(kTabSize, Json(4)), 4, 1, 8);
}

bool Settings::word_wrap() const {
    std::lock_guard lock(mutex_);
    return bool_or(values_.value(kWordWrap, Json(false)), false);
}

bool Settings::minimap() const {
    std::lock_guard lock(mutex_);
    return bool_or(values_.value(kMinimap, Json(true)), true);
}

bool Settings::format_on_save() const {
    std::lock_guard lock(mutex_);
    return bool_or(values_.value(kFormatOnSave, Json(false)), false);
}

std::string Settings::clangd_path() const {
    std::lock_guard lock(mutex_);
    return string_or(values_.value(kClangdPath, Json("clangd")), "clangd");
}

std::vector<std::string> Settings::clangd_args() const {
    std::lock_guard lock(mutex_);
    return string_array_or(values_.value(kClangdArgs, Json::array()));
}

bool Settings::enable_project_config() const {
    std::lock_guard lock(mutex_);
    return bool_or(values_.value(kEnableProjectConfig, Json(false)), false);
}

bool Settings::allow_unsandboxed_lsp() const {
    std::lock_guard lock(mutex_);
    return bool_or(values_.value(kAllowUnsandboxedLsp, Json(false)), false);
}

bool Settings::hide_git() const {
    std::lock_guard lock(mutex_);
    return bool_or(values_.value(kHideGit, Json(true)), true);
}

std::string Settings::ui_theme() const {
    std::lock_guard lock(mutex_);
    return string_or(values_.value(kUiTheme, Json("auto")), "auto");
}

std::string Settings::ui_font_family() const {
    std::lock_guard lock(mutex_);
    return string_or(values_.value(kUiFontFamily, Json("")), "");
}

int Settings::ui_font_size() const {
    std::lock_guard lock(mutex_);
    return bounded_int(values_.value(kUiFontSize, Json(12)), 12, 11, 18);
}

std::string Settings::editor_font_family() const {
    std::lock_guard lock(mutex_);
    return string_or(values_.value(kEditorFontFamily, Json("")), "");
}

bool Settings::editor_font_ligatures() const {
    std::lock_guard lock(mutex_);
    return bool_or(values_.value(kEditorFontLigatures, Json(true)), true);
}

std::string Settings::editor_line_height() const {
    std::lock_guard lock(mutex_);
    return string_or(values_.value(kEditorLineHeight, Json("normal")), "normal");
}

std::string Settings::ui_language() const {
    std::lock_guard lock(mutex_);
    return string_or(values_.value(kUiLanguage, Json("auto")), "auto");
}

std::filesystem::path Settings::file_path() const {
    std::lock_guard lock(mutex_);
    return config_dir_ / "settings.json";
}

}  // namespace anvil
