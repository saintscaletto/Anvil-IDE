#include <gtest/gtest.h>

#include "anvil/util/strings.hpp"

namespace {

TEST(Strings, Utf8Validation) {
    EXPECT_TRUE(anvil::strings::is_valid_utf8("plain ASCII"));
    EXPECT_TRUE(anvil::strings::is_valid_utf8("Anvil — C++"));
    EXPECT_TRUE(anvil::strings::is_valid_utf8("\xF0\x9F\x94\xA8"));  // hammer emoji
    EXPECT_FALSE(anvil::strings::is_valid_utf8("\xC0\xAF"));         // overlong slash
    EXPECT_FALSE(anvil::strings::is_valid_utf8("\xED\xA0\x80"));     // surrogate
    EXPECT_FALSE(anvil::strings::is_valid_utf8("\xF0\x9F\x94"));     // truncated
}

TEST(Strings, FileUriRoundTrip) {
    const std::string path = "/tmp/anvil/a b/\xD1\x84\xD0\xB0\xD0\xB9\xD0\xBB.cpp";
    const std::string uri = anvil::strings::path_to_file_uri(path);
    EXPECT_EQ(uri.find(' '), std::string::npos);
    const auto decoded = anvil::strings::file_uri_to_path(uri);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, path);
}

TEST(Strings, FileUriRejectsNonFileAndMalformed) {
    EXPECT_FALSE(anvil::strings::file_uri_to_path("https://example.com/a.cpp").has_value());
    EXPECT_FALSE(anvil::strings::file_uri_to_path("file://remotehost/a.cpp").has_value());
    EXPECT_FALSE(anvil::strings::file_uri_to_path("file:///tmp/%ZZ").has_value());
    EXPECT_FALSE(anvil::strings::file_uri_to_path("file:///tmp/%00").has_value());
    EXPECT_FALSE(anvil::strings::file_uri_to_path("relative/path").has_value());
}

TEST(Strings, Helpers) {
    EXPECT_EQ(anvil::strings::to_lower("AnViL"), "anvil");
    EXPECT_EQ(anvil::strings::trim("  x \n"), "x");
    EXPECT_TRUE(anvil::strings::starts_with("file:///x", "file://"));
    EXPECT_TRUE(anvil::strings::ends_with("main.cpp", ".cpp"));
    EXPECT_EQ(anvil::strings::split("a/b/c", '/').size(), 3U);
    EXPECT_EQ(anvil::strings::join({"a", "b"}, "/"), "a/b");
    EXPECT_EQ(anvil::strings::random_hex(8).size(), 16U);
    EXPECT_EQ(anvil::strings::fnv1a64("anvil"), anvil::strings::fnv1a64("anvil"));
    EXPECT_NE(anvil::strings::fnv1a64("anvil"), anvil::strings::fnv1a64("Anvil"));
}

}  // namespace
