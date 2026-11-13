#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "anvil/error.hpp"

namespace anvil::ipc::validate {

inline constexpr std::size_t kMaxPathBytes = 1024;
inline constexpr std::size_t kMaxTextBytes = 16U * 1024U * 1024U;
inline constexpr std::size_t kMaxReadBytes = 8U * 1024U * 1024U;
inline constexpr int kMaxLineOrColumn = 10'000'000;

struct NoParams {};

struct WorkspaceOpen {
    std::string path;
};

struct WorkspaceSaveState {
    std::vector<std::string> tabs;
    std::string active;
};

struct FsRead {
    std::string path;
    std::size_t max_bytes = kMaxReadBytes;
};

struct FsWrite {
    std::string path;
    std::string content;
    std::optional<std::int64_t> expected_mtime_ms;
    bool force = false;
};

struct FsList {
    std::string path;
};

struct FsCreate {
    std::string path;
    bool directory = false;
};

struct FsRename {
    std::string from;
    std::string to;
};

struct FsDelete {
    std::string path;
    bool recursive = false;
};

struct FsSearch {
    std::string query;
    int limit = 200;
};

struct SettingsSet {
    std::string key;
    Json value;
};

struct LspEnsure {
    std::string path;
    std::string language_id;
};

struct LspDidOpen {
    std::string path;
    std::string language_id;
    std::string text;
    int version = 1;
};

struct Position {
    int line = 0;
    int character = 0;
};

struct Range {
    Position start;
    Position end;
};

struct TextChange {
    std::optional<Range> range;
    std::string text;
};

struct LspDidChange {
    std::string path;
    int version = 1;
    std::vector<TextChange> changes;
    std::string full_text;
    bool has_full_text = false;
};

struct LspPath {
    std::string path;
};

struct LspPosition {
    std::string path;
    Position position;
};

struct LspCompletion {
    std::string path;
    Position position;
    std::optional<std::string> trigger_character;
};

struct LspReferences {
    std::string path;
    Position position;
    bool include_declaration = false;
};

struct LspRename {
    std::string path;
    Position position;
    std::string new_name;
};

struct LspFormat {
    std::string path;
    int tab_size = 4;
    bool insert_spaces = true;
};

struct LspRestart {
    std::string language_id;
};

struct Cancel {
    Json target_id;
};

void reject_unknown(const Json& object, std::initializer_list<std::string_view> allowed);
std::string string_field(const Json& object, std::string_view key, std::size_t min_length,
                         std::size_t max_length);
std::optional<std::string> optional_string_field(const Json& object, std::string_view key,
                                                 std::size_t max_length);
bool bool_field(const Json& object, std::string_view key, bool default_value);
std::int64_t integer_field(const Json& object, std::string_view key, std::int64_t min_value,
                           std::int64_t max_value);
std::optional<std::int64_t> optional_integer_field(const Json& object, std::string_view key,
                                                   std::int64_t min_value,
                                                   std::int64_t max_value);
Json array_field(const Json& object, std::string_view key, std::size_t min_items,
                 std::size_t max_items, bool required);

// Paths in IPC messages are always workspace-relative POSIX paths. Absolute
// paths, "..", empty components, backslashes and control characters are
// rejected here, before they reach the filesystem layer.
std::string relative_path(const Json& object, std::string_view key, bool allow_root = false);
std::string absolute_path(const Json& object, std::string_view key);

NoParams parse_no_params(const Json& params);
WorkspaceOpen parse_workspace_open(const Json& params);
WorkspaceSaveState parse_workspace_save_state(const Json& params);
FsRead parse_fs_read(const Json& params);
FsWrite parse_fs_write(const Json& params);
FsList parse_fs_list(const Json& params);
FsCreate parse_fs_create(const Json& params);
FsRename parse_fs_rename(const Json& params);
FsDelete parse_fs_delete(const Json& params);
FsSearch parse_fs_search(const Json& params);
SettingsSet parse_settings_set(const Json& params);
LspEnsure parse_lsp_ensure(const Json& params);
LspDidOpen parse_lsp_did_open(const Json& params);
LspDidChange parse_lsp_did_change(const Json& params);
LspPath parse_lsp_path(const Json& params);
LspPosition parse_lsp_position(const Json& params);
LspCompletion parse_lsp_completion(const Json& params);
LspReferences parse_lsp_references(const Json& params);
LspRename parse_lsp_rename(const Json& params);
LspFormat parse_lsp_format(const Json& params);
LspRestart parse_lsp_restart(const Json& params);
Cancel parse_cancel(const Json& params);

}  // namespace anvil::ipc::validate
