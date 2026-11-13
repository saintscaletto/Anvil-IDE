#include <gtest/gtest.h>

#include "anvil/ipc/validate.hpp"

namespace {

using anvil::ApiError;
using anvil::ErrorCode;
using anvil::Json;
namespace validate = anvil::ipc::validate;

template <typename Function>
ErrorCode error_code(Function function) {
    try {
        function();
    } catch (const ApiError& error) {
        return error.code();
    }
    return static_cast<ErrorCode>(0);
}

TEST(Validate, RelativePaths) {
    EXPECT_EQ(validate::parse_fs_read(Json{{"path", "src/main.cpp"}}).path, "src/main.cpp");
    EXPECT_EQ(validate::parse_fs_list(Json{{"path", ""}}).path, "");
    EXPECT_EQ(validate::parse_fs_list(Json::object()).path, "");
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", "../etc/passwd"}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", "/etc/passwd"}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", "a//b"}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", "a/./b"}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", "a\\b"}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", std::string("a\x01" "b")}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", ""}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_fs_read(Json{{"path", std::string(1100, 'a')}});
              }),
              ErrorCode::PayloadTooLarge);
}

TEST(Validate, UnknownAndMissingFields) {
    EXPECT_EQ(error_code([] { validate::parse_no_params(Json{{"apiVersion", 1}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", "a"}, {"extra", 1}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", 3}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json::array()); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_write(Json{{"path", "a"}}); }),
              ErrorCode::InvalidParams);
}

TEST(Validate, ReadWrite) {
    EXPECT_EQ(validate::parse_fs_read(Json{{"path", "a"}, {"maxBytes", 100}}).max_bytes, 100U);
    EXPECT_EQ(error_code([] { validate::parse_fs_read(Json{{"path", "a"}, {"maxBytes", 0}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_fs_read(
                      Json{{"path", "a"}, {"maxBytes", static_cast<int64_t>(validate::kMaxReadBytes + 1)}});
              }),
              ErrorCode::InvalidParams);

    const auto write = validate::parse_fs_write(
        Json{{"path", "src/a.cpp"}, {"content", "int main() {}\n"}, {"expectedMtime", 42}, {"force", true}});
    EXPECT_EQ(write.content, "int main() {}\n");
    EXPECT_TRUE(write.expected_mtime_ms.has_value());
    EXPECT_EQ(*write.expected_mtime_ms, 42);
    EXPECT_TRUE(write.force);

    EXPECT_EQ(error_code([] {
                  validate::parse_fs_write(Json{{"path", "a"}, {"content", std::string(1, '\0')}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_fs_write(Json{{"path", "a"}, {"content", "\xC0\xAF"}});
              }),
              ErrorCode::InvalidParams);
}

TEST(Validate, CreateRenameDeleteSearch) {
    const auto create = validate::parse_fs_create(Json{{"path", "src/a.cpp"}, {"kind", "file"}});
    EXPECT_FALSE(create.directory);
    EXPECT_TRUE(validate::parse_fs_create(Json{{"path", "src"}, {"kind", "directory"}}).directory);
    EXPECT_EQ(error_code([] { validate::parse_fs_create(Json{{"path", "x"}, {"kind", "socket"}}); }),
              ErrorCode::InvalidParams);

    const auto rename = validate::parse_fs_rename(Json{{"from", "a"}, {"to", "b"}});
    EXPECT_EQ(rename.from, "a");
    EXPECT_EQ(rename.to, "b");
    EXPECT_EQ(error_code([] { validate::parse_fs_rename(Json{{"from", "a"}, {"to", "../b"}}); }),
              ErrorCode::InvalidParams);

    const auto remove = validate::parse_fs_delete(Json{{"path", "build"}, {"recursive", true}});
    EXPECT_TRUE(remove.recursive);
    EXPECT_EQ(error_code([] { validate::parse_fs_delete(Json{{"path", "build"}, {"recursive", 1}}); }),
              ErrorCode::InvalidParams);

    const auto search = validate::parse_fs_search(Json{{"query", "main"}, {"limit", 10}});
    EXPECT_EQ(search.query, "main");
    EXPECT_EQ(search.limit, 10);
    EXPECT_EQ(error_code([] { validate::parse_fs_search(Json{{"query", ""}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_fs_search(Json{{"query", "x"}, {"limit", 501}}); }),
              ErrorCode::InvalidParams);
}

TEST(Validate, WorkspaceAndSettings) {
    const auto open = validate::parse_workspace_open(Json{{"path", "/home/user/project"}});
    EXPECT_EQ(open.path, "/home/user/project");
    EXPECT_EQ(error_code([] { validate::parse_workspace_open(Json{{"path", "relative"}}); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::parse_workspace_open(Json{{"path", "/tmp/../etc"}}); }),
              static_cast<ErrorCode>(0));

    const auto state = validate::parse_workspace_save_state(
        Json{{"tabs", Json::array({"src/a.cpp", "include/b.h"})}, {"active", "src/a.cpp"}});
    EXPECT_EQ(state.tabs.size(), 2U);
    EXPECT_EQ(state.active, "src/a.cpp");
    EXPECT_EQ(error_code([] {
                  validate::parse_workspace_save_state(
                      Json{{"tabs", Json::array({"a"})}, {"active", "b"}});
              }),
              ErrorCode::InvalidParams);

    EXPECT_EQ(validate::parse_settings_set(
                  Json{{"key", "editor.fontSize"}, {"value", 18}})
                  .value.get<int>(),
              18);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(Json{{"key", "editor.fontSize"}, {"value", 2}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(Json{{"key", "not.real"}, {"value", true}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(Json{{"key", "lsp.clangdPath"}, {"value", "bin/clangd"}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(validate::parse_settings_set(
                  Json{{"key", "lsp.clangdArgs"}, {"value", Json::array({"--background-index"})}})
                  .value.size(),
              1U);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(
                      Json{{"key", "lsp.clangdArgs"}, {"value", Json::array({1})}});
              }),
              ErrorCode::InvalidParams);

    for (const char* theme : {"auto", "dark", "light", "high-contrast"}) {
        EXPECT_EQ(validate::parse_settings_set(Json{{"key", "ui.theme"}, {"value", theme}})
                      .value.get<std::string>(),
                  theme);
    }
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(Json{{"key", "ui.theme"}, {"value", "neon"}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(Json{{"key", "ui.theme"}, {"value", 3}});
              }),
              ErrorCode::InvalidParams);

    EXPECT_EQ(validate::parse_settings_set(Json{{"key", "ui.fontSize"}, {"value", 18}})
                  .value.get<int>(),
              18);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(Json{{"key", "ui.fontSize"}, {"value", 10}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(Json{{"key", "ui.fontSize"}, {"value", 19}});
              }),
              ErrorCode::InvalidParams);

    EXPECT_EQ(validate::parse_settings_set(
                  Json{{"key", "ui.fontFamily"}, {"value", "JetBrains Mono"}})
                  .value.get<std::string>(),
              "JetBrains Mono");
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(
                      Json{{"key", "editor.fontFamily"}, {"value", "Mono; } body { display:none"}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(
                      Json{{"key", "ui.fontFamily"}, {"value", std::string(129, 'a')}});
              }),
              ErrorCode::InvalidParams);

    for (const char* mode : {"compact", "normal", "relaxed"}) {
        EXPECT_EQ(validate::parse_settings_set(
                      Json{{"key", "editor.lineHeight"}, {"value", mode}})
                      .value.get<std::string>(),
                  mode);
    }
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(
                      Json{{"key", "editor.lineHeight"}, {"value", "huge"}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_FALSE(validate::parse_settings_set(
                     Json{{"key", "editor.fontLigatures"}, {"value", false}})
                     .value.get<bool>());
    for (const char* language : {"auto", "en", "ru"}) {
        EXPECT_EQ(validate::parse_settings_set(
                      Json{{"key", "ui.language"}, {"value", language}})
                      .value.get<std::string>(),
                  language);
    }
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(
                      Json{{"key", "ui.language"}, {"value", "de"}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(
                      Json{{"key", "ui.language"}, {"value", 42}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_settings_set(
                      Json{{"key", "editor.fontLigatures"}, {"value", "yes"}});
              }),
              ErrorCode::InvalidParams);
}

TEST(Validate, LspDocuments) {
    const auto open = validate::parse_lsp_did_open(Json{{"path", "src/a.cpp"},
                                                        {"languageId", "cpp"},
                                                        {"text", "int x;"},
                                                        {"version", 1}});
    EXPECT_EQ(open.language_id, "cpp");
    EXPECT_EQ(open.version, 1);
    EXPECT_EQ(error_code([] {
                  validate::parse_lsp_did_open(
                      Json{{"path", "src/a.cpp"}, {"languageId", "cpp"}, {"version", 1}});
              }),
              ErrorCode::InvalidParams);

    const auto change = validate::parse_lsp_did_change(
        Json{{"path", "src/a.cpp"},
             {"version", 2},
             {"changes", Json::array({{{"range",
                                         {{"start", {{"line", 0}, {"character", 0}}},
                                          {"end", {{"line", 0}, {"character", 3}}}}},
                                        {"text", "int"}}})}});
    ASSERT_EQ(change.changes.size(), 1U);
    EXPECT_TRUE(change.changes[0].range.has_value());
    EXPECT_EQ(change.changes[0].text, "int");

    EXPECT_EQ(error_code([] {
                  validate::parse_lsp_did_change(
                      Json{{"path", "a"}, {"version", 2}, {"changes", Json::array()}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_lsp_did_change(
                      Json{{"path", "a"},
                           {"version", 2},
                           {"changes", Json::array({{{"text", "x"}}})},
                           {"fullText", "x"}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_lsp_did_change(
                      Json{{"path", "a"},
                           {"version", 2},
                           {"changes",
                            Json::array({{{"range",
                                           {{"start", {{"line", 2}, {"character", 0}}},
                                            {"end", {{"line", 1}, {"character", 0}}}}},
                                          {"text", "x"}}})}});
              }),
              ErrorCode::InvalidParams);

    const auto position = validate::parse_lsp_position(
        Json{{"path", "src/a.cpp"}, {"line", 4}, {"character", 2}});
    EXPECT_EQ(position.position.line, 4);
    EXPECT_EQ(error_code([] {
                  validate::parse_lsp_position(
                      Json{{"path", "a"}, {"line", -1}, {"character", 0}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] {
                  validate::parse_lsp_completion(
                      Json{{"path", "a"}, {"line", 0}, {"character", 0}, {"triggerCharacter", "trigger!!"}});
              }),
              ErrorCode::PayloadTooLarge);
}

TEST(Validate, LspRequestsAndCancel) {
    EXPECT_EQ(validate::parse_lsp_references(
                  Json{{"path", "a"}, {"line", 1}, {"character", 2}, {"includeDeclaration", false}})
                  .include_declaration,
              false);
    EXPECT_EQ(validate::parse_lsp_rename(
                  Json{{"path", "a"}, {"line", 1}, {"character", 2}, {"newName", "value"}})
                  .new_name,
              "value");
    EXPECT_EQ(error_code([] {
                  validate::parse_lsp_rename(
                      Json{{"path", "a"}, {"line", 1}, {"character", 2}, {"newName", "bad\nname"}});
              }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(validate::parse_lsp_format(
                  Json{{"path", "a"}, {"tabSize", 2}, {"insertSpaces", true}})
                  .tab_size,
              2);
    EXPECT_EQ(validate::parse_lsp_restart(Json::object()).language_id, "");
    EXPECT_EQ(validate::parse_cancel(Json{{"targetId", 9}}).target_id.get<int>(), 9);
    EXPECT_EQ(error_code([] { validate::parse_cancel(Json{{"targetId", "9"}}); }),
              ErrorCode::InvalidParams);
}

TEST(Validate, AbsolutePathAndRecent) {
    EXPECT_EQ(validate::absolute_path(Json{{"path", "/tmp/x"}}, "path"), "/tmp/x");
    EXPECT_EQ(error_code([] { validate::absolute_path(Json{{"path", "tmp/x"}}, "path"); }),
              ErrorCode::InvalidParams);
    EXPECT_EQ(error_code([] { validate::absolute_path(Json{{"path", "/tmp/\x01"}}, "path"); }),
              ErrorCode::InvalidParams);
}

}  // namespace
