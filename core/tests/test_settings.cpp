#include <gtest/gtest.h>

#include "anvil/settings.hpp"
#include "test_util.hpp"

namespace {

using anvil::ApiError;
using anvil::Settings;

TEST(Settings, DefaultsAndPersistence) {
    anvil::test::TempDir temp;
    const auto config = temp.path() / "config";
    Settings settings;
    settings.load(config);

    const auto defaults = settings.snapshot();
    EXPECT_EQ(defaults["ui.theme"], "auto");
    EXPECT_EQ(defaults["ui.fontFamily"], "");
    EXPECT_EQ(defaults["ui.fontSize"], 12);
    EXPECT_EQ(defaults["editor.fontFamily"], "");
    EXPECT_EQ(defaults["editor.fontLigatures"], true);
    EXPECT_EQ(defaults["editor.lineHeight"], "normal");
    EXPECT_EQ(defaults["ui.language"], "auto");

    settings.set("ui.theme", "light");
    settings.set("ui.fontFamily", "Noto Sans");
    settings.set("ui.fontSize", 15);
    settings.set("editor.fontFamily", "JetBrains Mono");
    settings.set("editor.fontLigatures", false);
    settings.set("editor.lineHeight", "relaxed");
    settings.set("ui.language", "ru");

    Settings reloaded;
    reloaded.load(config);
    const auto values = reloaded.snapshot();
    EXPECT_EQ(values["ui.theme"], "light");
    EXPECT_EQ(values["ui.fontFamily"], "Noto Sans");
    EXPECT_EQ(values["ui.fontSize"], 15);
    EXPECT_EQ(values["editor.fontFamily"], "JetBrains Mono");
    EXPECT_EQ(values["editor.fontLigatures"], false);
    EXPECT_EQ(values["editor.lineHeight"], "relaxed");
    EXPECT_EQ(values["ui.language"], "ru");
    EXPECT_EQ(reloaded.ui_theme(), "light");
    EXPECT_EQ(reloaded.ui_font_family(), "Noto Sans");
    EXPECT_EQ(reloaded.ui_font_size(), 15);
    EXPECT_EQ(reloaded.editor_font_family(), "JetBrains Mono");
    EXPECT_FALSE(reloaded.editor_font_ligatures());
    EXPECT_EQ(reloaded.editor_line_height(), "relaxed");
    EXPECT_EQ(reloaded.ui_language(), "ru");
}

TEST(Settings, RejectsInvalidValues) {
    anvil::test::TempDir temp;
    Settings settings;
    settings.load(temp.path() / "config");

    EXPECT_THROW(settings.set("ui.theme", "neon"), ApiError);
    EXPECT_THROW(settings.set("ui.theme", 1), ApiError);
    EXPECT_THROW(settings.set("ui.fontSize", 10), ApiError);
    EXPECT_THROW(settings.set("ui.fontSize", 19), ApiError);
    EXPECT_THROW(settings.set("ui.fontFamily", "Mono; body { display:none }"), ApiError);
    EXPECT_THROW(settings.set("editor.fontFamily", "\"Injected\""), ApiError);
    EXPECT_THROW(settings.set("editor.lineHeight", "huge"), ApiError);
    EXPECT_THROW(settings.set("editor.fontLigatures", "yes"), ApiError);
    EXPECT_THROW(settings.set("ui.language", "de"), ApiError);
    EXPECT_THROW(settings.set("ui.language", 1), ApiError);
    EXPECT_THROW(settings.set("unknown.key", true), ApiError);
}

TEST(Settings, IgnoresInvalidPersistedValues) {
    anvil::test::TempDir temp;
    const auto config = temp.path() / "config";
    anvil::test::write_file(
        config / "settings.json",
        R"({"ui.theme":"neon","ui.fontSize":99,"ui.fontFamily":"Mono;evil",
            "editor.lineHeight":"huge","editor.fontLigatures":false})");

    Settings settings;
    settings.load(config);
    const auto values = settings.snapshot();
    EXPECT_EQ(values["ui.theme"], "auto");
    EXPECT_EQ(values["ui.fontSize"], 12);
    EXPECT_EQ(values["ui.fontFamily"], "");
    EXPECT_EQ(values["editor.lineHeight"], "normal");
    // Valid keys in a partially invalid file still load.
    EXPECT_EQ(values["editor.fontLigatures"], false);
}

TEST(Settings, FontFamilyValidation) {
    EXPECT_TRUE(anvil::is_valid_font_family("JetBrains Mono"));
    EXPECT_TRUE(anvil::is_valid_font_family("Noto Sans CJK"));
    EXPECT_TRUE(anvil::is_valid_font_family(""));
    EXPECT_TRUE(anvil::is_valid_font_family("Fira Code"));
    EXPECT_TRUE(anvil::is_valid_font_family("M+ 1m"));
    EXPECT_FALSE(anvil::is_valid_font_family("Mono;"));
    EXPECT_FALSE(anvil::is_valid_font_family("Mono}"));
    EXPECT_FALSE(anvil::is_valid_font_family("Mono{"));
    EXPECT_FALSE(anvil::is_valid_font_family("Mono<"));
    EXPECT_FALSE(anvil::is_valid_font_family("Mono\""));
    EXPECT_FALSE(anvil::is_valid_font_family("Mono'"));
    EXPECT_FALSE(anvil::is_valid_font_family("Mono\\"));
    EXPECT_FALSE(anvil::is_valid_font_family("Mono\n"));
    EXPECT_FALSE(anvil::is_valid_font_family(std::string(129, 'a')));
    EXPECT_FALSE(anvil::is_valid_font_family(std::string("Mono\x01")));

    EXPECT_TRUE(anvil::is_valid_theme_id("auto"));
    EXPECT_TRUE(anvil::is_valid_theme_id("high-contrast"));
    EXPECT_FALSE(anvil::is_valid_theme_id("High-Contrast"));
    EXPECT_TRUE(anvil::is_valid_line_height_mode("compact"));
    EXPECT_FALSE(anvil::is_valid_line_height_mode("wide"));
    EXPECT_TRUE(anvil::is_valid_language_preference("auto"));
    EXPECT_TRUE(anvil::is_valid_language_preference("ru"));
    EXPECT_FALSE(anvil::is_valid_language_preference("ru-RU"));
}

}  // namespace
