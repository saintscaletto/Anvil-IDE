#include "anvil/ipc/validate.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "anvil/settings.hpp"
#include "anvil/util/strings.hpp"

namespace anvil::ipc::validate {
namespace {

void require_object(const Json& params) {
    if (!params.is_object()) {
        throw ApiError(ErrorCode::InvalidParams, "params must be an object");
    }
}

const Json& required(const Json& object, std::string_view key) {
    require_object(object);
    const std::string name(key);
    if (!object.contains(name)) {
        throw ApiError(ErrorCode::InvalidParams, "Missing required parameter: " + name);
    }
    return object.at(name);
}

void require_utf8(const std::string& value, std::string_view field) {
    if (!strings::is_valid_utf8(value)) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) + "' is not valid UTF-8");
    }
    if (value.find('\0') != std::string::npos) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) + "' contains a NUL byte");
    }
}

std::string text_value(const Json& value, std::string_view field, std::size_t max_length) {
    if (!value.is_string()) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) + "' must be a string");
    }
    const std::string text = value.get<std::string>();
    if (text.size() > max_length) {
        throw ApiError(ErrorCode::PayloadTooLarge,
                       "Field '" + std::string(field) + "' exceeds the size limit");
    }
    require_utf8(text, field);
    return text;
}

std::string path_value(const Json& value, std::string_view field, bool allow_root) {
    const std::string path = text_value(value, field, kMaxPathBytes);
    if (path.empty()) {
        if (allow_root) return path;
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) + "' must not be empty");
    }
    if (path.front() == '/' || path.find('\\') != std::string::npos) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) +
                           "' must be a relative POSIX path without backslashes");
    }
    for (const char c : path) {
        if (static_cast<unsigned char>(c) < 0x20) {
            throw ApiError(ErrorCode::InvalidParams,
                           "Field '" + std::string(field) + "' contains control characters");
        }
    }
    const auto parts = strings::split(path, '/');
    for (const auto& part : parts) {
        if (part.empty() || part == "." || part == "..") {
            throw ApiError(ErrorCode::InvalidParams,
                           "Field '" + std::string(field) +
                               "' contains an empty, '.' or '..' component");
        }
        if (part.size() > 255) {
            throw ApiError(ErrorCode::InvalidParams,
                           "A path component in '" + std::string(field) + "' is too long");
        }
    }
    return path;
}

int64_t int_value(const Json& value, std::string_view field, int64_t min_value,
                  int64_t max_value) {
    if (value.is_number_unsigned()) {
        const auto unsigned_value = value.get<std::uint64_t>();
        if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<int64_t>::max())) {
            throw ApiError(ErrorCode::InvalidParams,
                           "Field '" + std::string(field) + "' is out of range");
        }
        const auto signed_value = static_cast<int64_t>(unsigned_value);
        if (signed_value < min_value || signed_value > max_value) {
            throw ApiError(ErrorCode::InvalidParams,
                           "Field '" + std::string(field) + "' is out of range");
        }
        return signed_value;
    }
    if (!value.is_number_integer()) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) + "' must be an integer");
    }
    const auto signed_value = value.get<int64_t>();
    if (signed_value < min_value || signed_value > max_value) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) + "' is out of range");
    }
    return signed_value;
}

Position parse_position_value(const Json& value, std::string_view field) {
    if (!value.is_object()) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) + "' must be an object");
    }
    reject_unknown(value, {"line", "character"});
    if (!value.contains("line") || !value.contains("character")) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Position '" + std::string(field) + "' requires line and character");
    }
    Position position;
    position.line = static_cast<int>(
        int_value(value.at("line"), "line", 0, kMaxLineOrColumn));
    position.character = static_cast<int>(
        int_value(value.at("character"), "character", 0, kMaxLineOrColumn));
    return position;
}

Range parse_range_value(const Json& value, std::string_view field) {
    if (!value.is_object()) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) + "' must be an object");
    }
    reject_unknown(value, {"start", "end"});
    if (!value.contains("start") || !value.contains("end")) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Range '" + std::string(field) + "' requires start and end");
    }
    return Range{parse_position_value(value.at("start"), "start"),
                 parse_position_value(value.at("end"), "end")};
}

std::string language_id_value(const Json& value, std::string_view field) {
    const std::string language = text_value(value, field, 32);
    if (language.empty()) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Field '" + std::string(field) + "' must not be empty");
    }
    for (const char c : language) {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '+';
        if (!allowed) {
            throw ApiError(ErrorCode::InvalidParams,
                           "Field '" + std::string(field) + "' contains invalid characters");
        }
    }
    return language;
}

std::string executable_name_value(const Json& value) {
    const std::string name = text_value(value, "lsp.clangdPath", 1024);
    if (name.empty()) {
        throw ApiError(ErrorCode::InvalidParams, "lsp.clangdPath must not be empty");
    }
    for (const char c : name) {
        if (static_cast<unsigned char>(c) < 0x20) {
            throw ApiError(ErrorCode::InvalidParams,
                           "lsp.clangdPath contains control characters");
        }
    }
    if (name.find('/') != std::string::npos) {
        if (name.front() != '/') {
            throw ApiError(ErrorCode::InvalidParams,
                           "lsp.clangdPath must be absolute when it contains a slash");
        }
    } else {
        for (const char c : name) {
            const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                 (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                                 c == '+';
            if (!allowed) {
                throw ApiError(ErrorCode::InvalidParams,
                               "lsp.clangdPath contains invalid characters");
            }
        }
    }
    return name;
}

}  // namespace

void reject_unknown(const Json& object, std::initializer_list<std::string_view> allowed) {
    require_object(object);
    for (const auto& [key, value] : object.items()) {
        const bool known = std::any_of(allowed.begin(), allowed.end(),
                                       [&](std::string_view name) { return key == name; });
        if (!known) {
            throw ApiError(ErrorCode::InvalidParams, "Unknown parameter: " + key);
        }
    }
}

std::string string_field(const Json& object, std::string_view key, std::size_t min_length,
                         std::size_t max_length) {
    require_object(object);
    const std::string name(key);
    if (!object.contains(name)) {
        throw ApiError(ErrorCode::InvalidParams, "Missing required parameter: " + name);
    }
    const std::string value = text_value(object.at(name), key, max_length);
    if (value.size() < min_length) {
        throw ApiError(ErrorCode::InvalidParams, "Parameter '" + name + "' is too short");
    }
    return value;
}

std::optional<std::string> optional_string_field(const Json& object, std::string_view key,
                                                 std::size_t max_length) {
    require_object(object);
    const std::string name(key);
    if (!object.contains(name) || object.at(name).is_null()) return std::nullopt;
    return text_value(object.at(name), key, max_length);
}

bool bool_field(const Json& object, std::string_view key, bool default_value) {
    require_object(object);
    const std::string name(key);
    if (!object.contains(name)) return default_value;
    if (!object.at(name).is_boolean()) {
        throw ApiError(ErrorCode::InvalidParams, "Parameter '" + name + "' must be boolean");
    }
    return object.at(name).get<bool>();
}

std::int64_t integer_field(const Json& object, std::string_view key, std::int64_t min_value,
                           std::int64_t max_value) {
    require_object(object);
    const std::string name(key);
    if (!object.contains(name)) {
        throw ApiError(ErrorCode::InvalidParams, "Missing required parameter: " + name);
    }
    return int_value(object.at(name), key, min_value, max_value);
}

std::optional<std::int64_t> optional_integer_field(const Json& object, std::string_view key,
                                                   std::int64_t min_value,
                                                   std::int64_t max_value) {
    require_object(object);
    const std::string name(key);
    if (!object.contains(name) || object.at(name).is_null()) return std::nullopt;
    return int_value(object.at(name), key, min_value, max_value);
}

Json array_field(const Json& object, std::string_view key, std::size_t min_items,
                 std::size_t max_items, bool required) {
    require_object(object);
    const std::string name(key);
    if (!object.contains(name)) {
        if (required) {
            throw ApiError(ErrorCode::InvalidParams, "Missing required parameter: " + name);
        }
        return Json::array();
    }
    const Json& value = object.at(name);
    if (!value.is_array()) {
        throw ApiError(ErrorCode::InvalidParams, "Parameter '" + name + "' must be an array");
    }
    if (value.size() < min_items || value.size() > max_items) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Parameter '" + name + "' has an invalid number of items");
    }
    return value;
}

std::string relative_path(const Json& object, std::string_view key, bool allow_root) {
    require_object(object);
    const std::string name(key);
    if (!object.contains(name)) {
        if (allow_root) return {};
        throw ApiError(ErrorCode::InvalidParams, "Missing required parameter: " + name);
    }
    return path_value(object.at(name), key, allow_root);
}

std::string absolute_path(const Json& object, std::string_view key) {
    require_object(object);
    const std::string name(key);
    if (!object.contains(name)) {
        throw ApiError(ErrorCode::InvalidParams, "Missing required parameter: " + name);
    }
    const std::string path = text_value(object.at(name), key, 4096);
    if (path.empty() || path.front() != '/') {
        throw ApiError(ErrorCode::InvalidParams,
                       "Parameter '" + name + "' must be an absolute path");
    }
    for (const char c : path) {
        if (static_cast<unsigned char>(c) < 0x20) {
            throw ApiError(ErrorCode::InvalidParams,
                           "Parameter '" + name + "' contains control characters");
        }
    }
    return path;
}

NoParams parse_no_params(const Json& params) {
    reject_unknown(params, {});
    return {};
}

WorkspaceOpen parse_workspace_open(const Json& params) {
    reject_unknown(params, {"path"});
    return WorkspaceOpen{absolute_path(params, "path")};
}

WorkspaceSaveState parse_workspace_save_state(const Json& params) {
    reject_unknown(params, {"tabs", "active"});
    WorkspaceSaveState state;
    const Json tabs = array_field(params, "tabs", 0, 64, true);
    for (const auto& tab : tabs) {
        state.tabs.push_back(path_value(tab, "tabs[]", false));
    }
    if (params.contains("active")) {
        state.active = path_value(params.at("active"), "active", true);
    }
    if (!state.active.empty() &&
        std::find(state.tabs.begin(), state.tabs.end(), state.active) == state.tabs.end()) {
        throw ApiError(ErrorCode::InvalidParams, "active must be one of tabs");
    }
    return state;
}

FsRead parse_fs_read(const Json& params) {
    reject_unknown(params, {"path", "maxBytes"});
    FsRead out;
    out.path = relative_path(params, "path");
    if (params.contains("maxBytes")) {
        out.max_bytes = static_cast<std::size_t>(
            integer_field(params, "maxBytes", 1, static_cast<int64_t>(kMaxReadBytes)));
    }
    return out;
}

FsWrite parse_fs_write(const Json& params) {
    reject_unknown(params, {"path", "content", "expectedMtime", "force"});
    FsWrite out;
    out.path = relative_path(params, "path");
    if (!params.contains("content")) {
        throw ApiError(ErrorCode::InvalidParams, "Missing required parameter: content");
    }
    out.content = text_value(params.at("content"), "content", kMaxTextBytes);
    out.expected_mtime_ms = optional_integer_field(params, "expectedMtime", 0,
                                                   std::numeric_limits<int64_t>::max());
    out.force = bool_field(params, "force", false);
    return out;
}

FsList parse_fs_list(const Json& params) {
    reject_unknown(params, {"path"});
    return FsList{relative_path(params, "path", true)};
}

FsCreate parse_fs_create(const Json& params) {
    reject_unknown(params, {"path", "kind"});
    FsCreate out;
    out.path = relative_path(params, "path");
    const std::string kind = string_field(params, "kind", 1, 16);
    if (kind == "directory") {
        out.directory = true;
    } else if (kind != "file") {
        throw ApiError(ErrorCode::InvalidParams, "kind must be 'file' or 'directory'");
    }
    return out;
}

FsRename parse_fs_rename(const Json& params) {
    reject_unknown(params, {"from", "to"});
    return FsRename{relative_path(params, "from"), relative_path(params, "to")};
}

FsDelete parse_fs_delete(const Json& params) {
    reject_unknown(params, {"path", "recursive"});
    return FsDelete{relative_path(params, "path"), bool_field(params, "recursive", false)};
}

FsSearch parse_fs_search(const Json& params) {
    reject_unknown(params, {"query", "limit"});
    FsSearch out;
    out.query = string_field(params, "query", 1, 128);
    if (params.contains("limit")) {
        out.limit = static_cast<int>(integer_field(params, "limit", 1, 500));
    }
    return out;
}

SettingsSet parse_settings_set(const Json& params) {
    reject_unknown(params, {"key", "value"});
    SettingsSet out;
    out.key = string_field(params, "key", 1, 64);
    if (!params.contains("value")) {
        throw ApiError(ErrorCode::InvalidParams, "Missing required parameter: value");
    }
    out.value = params.at("value");
    if (out.value.dump().size() > 64U * 1024U) {
        throw ApiError(ErrorCode::PayloadTooLarge, "Setting value is too large");
    }

    if (out.key == "editor.fontSize") {
        out.value = integer_field(params, "value", 8, 32);
    } else if (out.key == "editor.tabSize") {
        out.value = integer_field(params, "value", 1, 8);
    } else if (out.key == "editor.wordWrap" || out.key == "editor.minimap" ||
               out.key == "editor.formatOnSave" || out.key == "lsp.enableProjectConfig" ||
               out.key == "security.allowUnsandboxedLsp" || out.key == "explorer.hideGit") {
        if (!out.value.is_boolean()) {
            throw ApiError(ErrorCode::InvalidParams, "Setting value must be boolean");
        }
    } else if (out.key == "lsp.clangdPath") {
        out.value = executable_name_value(out.value);
    } else if (out.key == "ui.theme") {
        if (!out.value.is_string() ||
            !anvil::is_valid_theme_id(out.value.get<std::string>())) {
            throw ApiError(ErrorCode::InvalidParams, "Invalid ui.theme");
        }
    } else if (out.key == "editor.lineHeight") {
        if (!out.value.is_string() ||
            !anvil::is_valid_line_height_mode(out.value.get<std::string>())) {
            throw ApiError(ErrorCode::InvalidParams, "Invalid editor.lineHeight");
        }
    } else if (out.key == "ui.fontSize") {
        out.value = integer_field(params, "value", 11, 18);
    } else if (out.key == "ui.fontFamily" || out.key == "editor.fontFamily") {
        if (!out.value.is_string()) {
            throw ApiError(ErrorCode::InvalidParams, "Setting value must be a string");
        }
        const std::string family = out.value.get<std::string>();
        if (family.size() > 128 || !anvil::is_valid_font_family(family)) {
            throw ApiError(ErrorCode::InvalidParams, "Invalid font family");
        }
    } else if (out.key == "editor.fontLigatures") {
        if (!out.value.is_boolean()) {
            throw ApiError(ErrorCode::InvalidParams, "Setting value must be boolean");
        }
    } else if (out.key == "lsp.clangdArgs") {
        if (!out.value.is_array() || out.value.size() > 32) {
            throw ApiError(ErrorCode::InvalidParams,
                           "lsp.clangdArgs must be an array of at most 32 strings");
        }
        for (const auto& argument : out.value) {
            if (!argument.is_string()) {
                throw ApiError(ErrorCode::InvalidParams, "lsp.clangdArgs entries must be strings");
            }
            const std::string text = argument.get<std::string>();
            if (text.size() > 256 || text.find('\0') != std::string::npos) {
                throw ApiError(ErrorCode::InvalidParams, "Invalid lsp.clangdArgs entry");
            }
        }
    } else {
        throw ApiError(ErrorCode::InvalidParams, "Unknown setting: " + out.key);
    }
    return out;
}

LspEnsure parse_lsp_ensure(const Json& params) {
    reject_unknown(params, {"path", "languageId"});
    LspEnsure out;
    out.path = relative_path(params, "path");
    if (params.contains("languageId")) {
        out.language_id = language_id_value(params.at("languageId"), "languageId");
    }
    return out;
}

LspDidOpen parse_lsp_did_open(const Json& params) {
    reject_unknown(params, {"path", "languageId", "text", "version"});
    LspDidOpen out;
    out.path = relative_path(params, "path");
    out.language_id = language_id_value(required(params, "languageId"), "languageId");
    out.text = text_value(required(params, "text"), "text", kMaxTextBytes);
    out.version = static_cast<int>(integer_field(params, "version", 1, 2'000'000'000));
    return out;
}

LspDidChange parse_lsp_did_change(const Json& params) {
    reject_unknown(params, {"path", "version", "changes", "fullText"});
    LspDidChange out;
    out.path = relative_path(params, "path");
    out.version = static_cast<int>(integer_field(params, "version", 1, 2'000'000'000));
    const bool has_changes = params.contains("changes");
    const bool has_full = params.contains("fullText");
    if (has_changes == has_full) {
        throw ApiError(ErrorCode::InvalidParams,
                       "Exactly one of changes or fullText must be supplied");
    }
    if (has_full) {
        out.has_full_text = true;
        out.full_text = text_value(params.at("fullText"), "fullText", kMaxTextBytes);
        return out;
    }

    const Json changes = array_field(params, "changes", 1, 512, true);
    std::size_t total = 0;
    for (const auto& change : changes) {
        if (!change.is_object()) {
            throw ApiError(ErrorCode::InvalidParams, "Each change must be an object");
        }
        reject_unknown(change, {"range", "text"});
        if (!change.contains("text")) {
            throw ApiError(ErrorCode::InvalidParams, "Each change requires text");
        }
        TextChange parsed;
        parsed.text = text_value(change.at("text"), "changes[].text", kMaxTextBytes);
        total += parsed.text.size();
        if (total > kMaxTextBytes) {
            throw ApiError(ErrorCode::PayloadTooLarge, "Combined changes exceed the limit");
        }
        if (change.contains("range")) {
            parsed.range = parse_range_value(change.at("range"), "range");
            if (parsed.range->end.line < parsed.range->start.line ||
                (parsed.range->end.line == parsed.range->start.line &&
                 parsed.range->end.character < parsed.range->start.character)) {
                throw ApiError(ErrorCode::InvalidParams, "Range end precedes range start");
            }
        }
        out.changes.push_back(std::move(parsed));
    }
    return out;
}

LspPath parse_lsp_path(const Json& params) {
    reject_unknown(params, {"path"});
    return LspPath{relative_path(params, "path")};
}

LspPosition parse_lsp_position(const Json& params) {
    reject_unknown(params, {"path", "line", "character"});
    LspPosition out;
    out.path = relative_path(params, "path");
    out.position.line = static_cast<int>(integer_field(params, "line", 0, kMaxLineOrColumn));
    out.position.character =
        static_cast<int>(integer_field(params, "character", 0, kMaxLineOrColumn));
    return out;
}

LspCompletion parse_lsp_completion(const Json& params) {
    reject_unknown(params, {"path", "line", "character", "triggerCharacter"});
    LspCompletion out;
    out.path = relative_path(params, "path");
    out.position.line = static_cast<int>(integer_field(params, "line", 0, kMaxLineOrColumn));
    out.position.character =
        static_cast<int>(integer_field(params, "character", 0, kMaxLineOrColumn));
    out.trigger_character = optional_string_field(params, "triggerCharacter", 8);
    return out;
}

LspReferences parse_lsp_references(const Json& params) {
    reject_unknown(params, {"path", "line", "character", "includeDeclaration"});
    LspReferences out;
    out.path = relative_path(params, "path");
    out.position.line = static_cast<int>(integer_field(params, "line", 0, kMaxLineOrColumn));
    out.position.character =
        static_cast<int>(integer_field(params, "character", 0, kMaxLineOrColumn));
    out.include_declaration = bool_field(params, "includeDeclaration", true);
    return out;
}

LspRename parse_lsp_rename(const Json& params) {
    reject_unknown(params, {"path", "line", "character", "newName"});
    LspRename out;
    out.path = relative_path(params, "path");
    out.position.line = static_cast<int>(integer_field(params, "line", 0, kMaxLineOrColumn));
    out.position.character =
        static_cast<int>(integer_field(params, "character", 0, kMaxLineOrColumn));
    out.new_name = string_field(params, "newName", 1, 256);
    if (out.new_name.find('\0') != std::string::npos ||
        out.new_name.find_first_of("\r\n") != std::string::npos) {
        throw ApiError(ErrorCode::InvalidParams, "newName contains invalid characters");
    }
    return out;
}

LspFormat parse_lsp_format(const Json& params) {
    reject_unknown(params, {"path", "tabSize", "insertSpaces"});
    LspFormat out;
    out.path = relative_path(params, "path");
    out.tab_size = static_cast<int>(integer_field(params, "tabSize", 1, 8));
    out.insert_spaces = bool_field(params, "insertSpaces", true);
    return out;
}

LspRestart parse_lsp_restart(const Json& params) {
    reject_unknown(params, {"languageId"});
    LspRestart out;
    if (params.contains("languageId")) {
        out.language_id = language_id_value(params.at("languageId"), "languageId");
    }
    return out;
}

Cancel parse_cancel(const Json& params) {
    reject_unknown(params, {"targetId"});
    if (!params.contains("targetId")) {
        throw ApiError(ErrorCode::InvalidParams, "Missing required parameter: targetId");
    }
    const Json& target = params.at("targetId");
    if (!target.is_number_integer()) {
        throw ApiError(ErrorCode::InvalidParams, "targetId must be an integer");
    }
    return Cancel{target};
}

}  // namespace anvil::ipc::validate
