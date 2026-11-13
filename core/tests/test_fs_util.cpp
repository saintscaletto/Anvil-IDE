#include <gtest/gtest.h>

#include <sys/stat.h>

#include <thread>

#include "anvil/util/fs_util.hpp"
#include "test_util.hpp"

namespace {

using anvil::ApiError;
using anvil::ErrorCode;
namespace fsutil = anvil::fsutil;

TEST(FsUtil, AtomicWriteAndRead) {
    anvil::test::TempDir temp;
    const auto file = temp.path() / "main.cpp";
    fsutil::write_text_file_atomic(file, "int main() {}\n", std::nullopt);
    EXPECT_EQ(fsutil::read_text_file(file, 1024), "int main() {}\n");
    const auto stat = fsutil::stat_path(file);
    EXPECT_TRUE(stat.is_regular);
    EXPECT_GT(stat.size, 0U);
    EXPECT_THROW(fsutil::read_text_file(temp.path() / "missing.cpp", 1024), ApiError);
}

TEST(FsUtil, RejectsBinaryAndInvalidUtf8) {
    anvil::test::TempDir temp;
    const auto binary = temp.path() / "binary.bin";
    anvil::test::write_file(binary, std::string("a\0b", 3));
    EXPECT_EQ(anvil::test::read_file(binary).size(), 3U);
    EXPECT_THROW(fsutil::read_text_file(binary, 1024), ApiError);

    const auto invalid = temp.path() / "invalid.txt";
    anvil::test::write_file(invalid, std::string("\xC0\xAF"));
    try {
        (void)fsutil::read_text_file(invalid, 1024);
        FAIL() << "invalid UTF-8 accepted";
    } catch (const ApiError& error) {
        EXPECT_EQ(error.code(), ErrorCode::InvalidUtf8);
    }
    EXPECT_THROW(fsutil::write_text_file_atomic(temp.path() / "bad.txt", std::string("\xC0\xAF"),
                                                std::nullopt),
                 ApiError);
}

TEST(FsUtil, SizeLimit) {
    anvil::test::TempDir temp;
    const auto file = temp.path() / "large.txt";
    anvil::test::write_file(file, std::string(2048, 'a'));
    try {
        (void)fsutil::read_text_file(file, 100);
        FAIL() << "size limit not enforced";
    } catch (const ApiError& error) {
        EXPECT_EQ(error.code(), ErrorCode::PayloadTooLarge);
    }
}

TEST(FsUtil, OptimisticConcurrency) {
    anvil::test::TempDir temp;
    const auto file = temp.path() / "a.txt";
    fsutil::write_text_file_atomic(file, "one", std::nullopt);
    const auto first = fsutil::stat_path(file);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    fsutil::write_text_file_atomic(file, "two", first.mtime_ms);
    const auto second = fsutil::stat_path(file);
    try {
        fsutil::write_text_file_atomic(file, "three", first.mtime_ms);
        FAIL() << "stale mtime was accepted";
    } catch (const ApiError& error) {
        EXPECT_EQ(error.code(), ErrorCode::Conflict);
    }
    EXPECT_EQ(fsutil::read_text_file(file, 1024), "two");
    EXPECT_NE(first.mtime_ms, second.mtime_ms);
}

TEST(FsUtil, ListingAndContainment) {
    anvil::test::TempDir temp;
    anvil::test::write_file(temp.path() / "src" / "a.cpp", "a");
    anvil::test::write_file(temp.path() / "README.md", "b");
    std::filesystem::create_directories(temp.path() / "include");
    bool truncated = false;
    const auto entries = fsutil::list_directory(temp.path(), 100, &truncated);
    ASSERT_GE(entries.size(), 3U);
    EXPECT_TRUE(entries[0].is_directory);
    EXPECT_FALSE(truncated);

    const auto root = std::filesystem::canonical(temp.path());
    EXPECT_TRUE(fsutil::is_within(root, std::filesystem::canonical(temp.path() / "src")));
    EXPECT_FALSE(fsutil::is_within(root, std::filesystem::path("/etc")));
    const auto relative = fsutil::relative_to(root, temp.path() / "src" / "a.cpp");
    ASSERT_TRUE(relative.has_value());
    EXPECT_EQ(*relative, "src/a.cpp");
}

TEST(FsUtil, JsonAndPrivateDirectories) {
    anvil::test::TempDir temp;
    const auto directory = temp.path() / "config";
    fsutil::ensure_private_directory(directory);
    struct stat info {};
    ASSERT_EQ(::stat(directory.c_str(), &info), 0);
    EXPECT_EQ(info.st_mode & 0777, 0700);
    fsutil::write_json_file_atomic(directory / "settings.json", anvil::Json{{"x", 1}});
    std::string error;
    const auto value = fsutil::try_read_json_file(directory / "settings.json", 4096, &error);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ((*value)["x"], 1);
    EXPECT_FALSE(fsutil::try_read_json_file(directory / "missing.json", 4096, &error).has_value());
}

}  // namespace
