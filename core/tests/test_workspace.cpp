#include <gtest/gtest.h>

#include "anvil/error.hpp"
#include "anvil/workspace/workspace.hpp"
#include "test_util.hpp"

namespace {

using anvil::ApiError;
using anvil::ErrorCode;
using anvil::Workspace;

TEST(Workspace, OpensAndResolvesInsideRoot) {
    anvil::test::TempDir temp;
    anvil::test::write_file(temp.path() / "src" / "main.cpp", "int main() {}\n");
    Workspace workspace;
    workspace.open(temp.path());
    EXPECT_TRUE(workspace.is_open());
    EXPECT_EQ(workspace.name(), temp.path().filename().string());
    EXPECT_FALSE(workspace.state_key().empty());
    EXPECT_EQ(workspace.resolve("src/main.cpp"),
              std::filesystem::canonical(temp.path() / "src/main.cpp"));
    EXPECT_EQ(workspace.resolve(""), std::filesystem::canonical(temp.path()));
    EXPECT_EQ(workspace.relative(temp.path() / "src" / "main.cpp"), "src/main.cpp");
}

TEST(Workspace, RejectsTraversalAndMissingPaths) {
    anvil::test::TempDir temp;
    anvil::test::write_file(temp.path() / "a.txt", "a");
    Workspace workspace;
    workspace.open(temp.path());
    EXPECT_THROW(workspace.resolve("missing.txt"), ApiError);
    EXPECT_THROW(workspace.resolve("../outside"), ApiError);
    EXPECT_THROW(workspace.relative("/etc/passwd"), ApiError);
}

TEST(Workspace, RejectsSymlinkEscape) {
    anvil::test::TempDir temp;
    std::error_code ec;
    std::filesystem::create_directories(temp.path() / "inside");
    std::filesystem::create_symlink("/etc", temp.path() / "escape", ec);
    ASSERT_FALSE(ec) << ec.message();
    Workspace workspace;
    workspace.open(temp.path());
    try {
        (void)workspace.resolve("escape/passwd");
        FAIL() << "symlink escape was accepted";
    } catch (const ApiError& error) {
        EXPECT_EQ(error.code(), ErrorCode::PathOutsideWorkspace);
    }
    // The link itself is inside and may be manipulated without following it.
    const auto link = workspace.resolve_nofollow("escape");
    EXPECT_TRUE(std::filesystem::is_symlink(link));
}

TEST(Workspace, CreateResolution) {
    anvil::test::TempDir temp;
    std::filesystem::create_directories(temp.path() / "src");
    Workspace workspace;
    workspace.open(temp.path());
    const auto new_file = workspace.resolve_for_create("src/new.cpp");
    EXPECT_EQ(new_file, std::filesystem::canonical(temp.path() / "src") / "new.cpp");
    EXPECT_THROW(workspace.resolve_for_create("missing/new.cpp"), ApiError);
    EXPECT_THROW(workspace.resolve_for_create("../new.cpp"), ApiError);
}

TEST(Workspace, RefusesFilesystemRootAndFiles) {
    Workspace workspace;
    EXPECT_THROW(workspace.open("/"), ApiError);
    anvil::test::TempDir temp;
    anvil::test::write_file(temp.path() / "file.txt", "x");
    EXPECT_THROW(workspace.open(temp.path() / "file.txt"), ApiError);
}

TEST(Workspace, CloseClearsRoot) {
    anvil::test::TempDir temp;
    Workspace workspace;
    workspace.open(temp.path());
    workspace.close();
    EXPECT_FALSE(workspace.is_open());
    try {
        (void)workspace.resolve("");
        FAIL() << "resolve succeeded after close";
    } catch (const ApiError& error) {
        EXPECT_EQ(error.code(), ErrorCode::WorkspaceNotOpen);
    }
}

}  // namespace
