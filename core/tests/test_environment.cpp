#include <gtest/gtest.h>

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "anvil/environment.hpp"
#include "test_util.hpp"

namespace {

using anvil::environment::IncludePolicy;
namespace env = anvil::environment;
namespace fs = std::filesystem;

bool contains(const std::vector<std::string>& values, const std::string& needle) {
    return std::find(values.begin(), values.end(), needle) != values.end();
}

std::string home_dir() {
    const char* home = std::getenv("HOME");
    return home == nullptr ? std::string() : std::string(home);
}

TEST(Environment, SplitsCommandLines) {
    const auto simple = env::split_command_line("/usr/bin/c++ -I/a -std=c++20 -c x.cpp");
    ASSERT_EQ(simple.size(), 5U);
    EXPECT_EQ(simple[0], "/usr/bin/c++");
    EXPECT_EQ(simple[1], "-I/a");

    const auto quoted = env::split_command_line(R"(/usr/bin/c++ -DVALUE="a b" -I'/a b' x.cpp)");
    ASSERT_EQ(quoted.size(), 4U);
    EXPECT_EQ(quoted[1], "-DVALUE=a b");
    EXPECT_EQ(quoted[2], "-I/a b");
}

TEST(Environment, LoadsAndNormalisesCompileCommands) {
    anvil::test::TempDir temp;
    const auto project = temp.path() / "project";
    anvil::test::write_file(project / "src" / "main.cpp", "int main() {}\n");
    anvil::test::write_file(
        project / "compile_commands.json",
        R"([{"directory":")" + project.string() +
            R"(","file":"src/main.cpp","arguments":["/usr/bin/c++","-Iinclude","-c","src/main.cpp"]},
               {"directory":")" + project.string() +
            R"(","file":"src/other.cpp","command":"/usr/bin/c++ -isystem /opt/sdk/include -c src/other.cpp"}])");

    const auto entries = env::load_compile_commands(project / "compile_commands.json");
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_EQ(entries[0].file, (project / "src" / "main.cpp").lexically_normal().string());
    EXPECT_EQ(entries[0].directory, project.lexically_normal().string());
    ASSERT_EQ(entries[1].arguments.size(), 5U);
    EXPECT_EQ(entries[1].arguments[1], "-isystem");
}

TEST(Environment, RejectsMalformedDatabases) {
    anvil::test::TempDir temp;
    anvil::test::write_file(temp.path() / "compile_commands.json", R"({"not":"an array"})");
    EXPECT_TRUE(env::load_compile_commands(temp.path() / "compile_commands.json").empty());
    anvil::test::write_file(temp.path() / "broken.json", "{");
    EXPECT_TRUE(env::load_compile_commands(temp.path() / "broken.json").empty());
}

TEST(Environment, ExtractsIncludeRoots) {
    anvil::test::TempDir temp;
    const auto include = temp.path() / "include";
    const auto system = temp.path() / "system";
    std::filesystem::create_directories(include);
    std::filesystem::create_directories(system);

    env::CompileCommandEntry entry;
    entry.directory = temp.path().string();
    entry.file = (temp.path() / "main.cpp").string();
    entry.arguments = {"/usr/bin/c++", "-I", "include",        "-isystem", system.string(),
                       "-Irelative",   "-std=c++20"};

    const auto roots = env::extract_include_roots({entry});
    EXPECT_TRUE(contains(roots, include.string()));
    EXPECT_TRUE(contains(roots, system.string()));
    // A missing directory is skipped rather than mounted.
    EXPECT_FALSE(contains(roots, (temp.path() / "relative").string()));
}

TEST(Environment, DiscoversBuildLayouts) {
    anvil::test::TempDir temp;
    const auto project = temp.path() / "project";
    anvil::test::write_file(project / "compile_commands.json", "[]");
    anvil::test::write_file(project / "build" / "compile_commands.json", "[]");
    anvil::test::write_file(project / "build" / "pkg_a" / "compile_commands.json", "[]");
    anvil::test::write_file(project / "build" / "pkg_b" / "compile_commands.json", "[]");

    const auto found = env::discover_compile_commands(project);
    EXPECT_EQ(found.size(), 4U);
}

TEST(Environment, ClassifiesIncludeRoots) {
    anvil::test::TempDir temp;
    const auto workspace = temp.path() / "workspace";
    const auto cache = temp.path() / "cache";
    const auto inside = workspace / "include";
    std::filesystem::create_directories(inside);
    std::filesystem::create_directories(cache);

    EXPECT_EQ(env::classify_include_root(inside, workspace, cache), IncludePolicy::skip);
    EXPECT_EQ(env::classify_include_root(cache, workspace, cache), IncludePolicy::skip);
    EXPECT_EQ(env::classify_include_root("/usr/include", workspace, cache),
              IncludePolicy::skip);
    EXPECT_EQ(env::classify_include_root("/opt", workspace, cache),
              IncludePolicy::automatic);
    EXPECT_EQ(env::classify_include_root("/usr/local/include", workspace, cache),
              IncludePolicy::automatic);

    const std::string home = home_dir();
    if (!home.empty()) {
        EXPECT_EQ(env::classify_include_root(home, workspace, cache), IncludePolicy::pending);
        const fs::path sensitive = fs::path(home) / ".ssh";
        if (fs::is_directory(sensitive)) {
            EXPECT_EQ(env::classify_include_root(sensitive, workspace, cache),
                      IncludePolicy::skip);
        }
    }
    // A random non-system directory must not be mounted automatically.
    EXPECT_EQ(env::classify_include_root(temp.path(), workspace, cache),
              IncludePolicy::pending);
}

TEST(Environment, SensitivePaths) {
    EXPECT_TRUE(env::is_sensitive_path("/home/user/.ssh"));
    EXPECT_TRUE(env::is_sensitive_path("/home/user/.ssh/id_ed25519"));
    EXPECT_TRUE(env::is_sensitive_path("/home/user/.gnupg"));
    EXPECT_TRUE(env::is_sensitive_path("/home/user/.local/share/keyrings"));
    EXPECT_TRUE(env::is_sensitive_path("/home/user/.aws"));
    EXPECT_FALSE(env::is_sensitive_path("/home/user/deps/include"));
    EXPECT_FALSE(env::is_sensitive_path("/opt/sdk/include"));
    EXPECT_FALSE(env::is_sensitive_path("/usr/include"));
}

TEST(Environment, MountPolicy) {
    anvil::test::TempDir temp;
    const auto workspace = temp.path() / "workspace";
    const auto cache = temp.path() / "cache";
    const auto deps = temp.path() / "deps";
    std::filesystem::create_directories(workspace);
    std::filesystem::create_directories(cache);
    std::filesystem::create_directories(deps);
    std::filesystem::create_directories(deps / ".ssh");

    // Configured roots may be mounted anywhere that is not reserved.
    EXPECT_TRUE(env::is_mountable_include_root(deps, true, workspace, cache));
    EXPECT_FALSE(env::is_mountable_include_root(deps / ".ssh", true, workspace, cache));
    EXPECT_FALSE(env::is_mountable_include_root(workspace, true, workspace, cache));
    EXPECT_FALSE(env::is_mountable_include_root(cache, true, workspace, cache));
    EXPECT_FALSE(env::is_mountable_include_root("/etc", true, workspace, cache));
    EXPECT_FALSE(env::is_mountable_include_root("/root", true, workspace, cache));

    // Automatic roots are limited to trusted prefixes and safe home dirs.
    EXPECT_FALSE(env::is_mountable_include_root(deps, false, workspace, cache));
    EXPECT_TRUE(env::is_mountable_include_root("/opt", false, workspace, cache));
}

TEST(Environment, ResolvesIncludeRootsFromDatabase) {
    anvil::test::TempDir temp;
    const auto project = temp.path() / "project";
    const auto cache = temp.path() / "cache";
    const auto include = project / "include";
    std::filesystem::create_directories(include);
    std::filesystem::create_directories(cache);
    anvil::test::write_file(project / "main.cpp", "int main() {}\n");
    anvil::test::write_file(
        project / "compile_commands.json",
        R"([{"directory":")" + project.string() + R"(","file":"main.cpp",
            "arguments":["/usr/bin/c++","-Iinclude","-c","main.cpp"]}])");

    const auto resolution = env::resolve(project, cache, {});
    ASSERT_FALSE(resolution.compile_commands_dir.empty());
    EXPECT_EQ(resolution.entry_count, 1U);
    EXPECT_TRUE(std::filesystem::is_regular_file(
        resolution.compile_commands_dir / "compile_commands.json"));
    // An include directory inside the workspace is already mounted.
    EXPECT_TRUE(resolution.automatic_include_roots.empty());
    EXPECT_TRUE(resolution.pending_include_roots.empty());
}

TEST(Environment, ValidatesPathsAndDrivers) {
    std::string reason;
    EXPECT_TRUE(env::is_valid_include_path("/opt/sdk/include", &reason));
    EXPECT_FALSE(env::is_valid_include_path("relative/path", &reason));
    EXPECT_FALSE(env::is_valid_include_path("/", &reason));
    EXPECT_FALSE(env::is_valid_include_path(std::string("/opt/") + std::string(1, '\0'),
                                            &reason));

    EXPECT_TRUE(env::is_valid_query_driver("", &reason));
    EXPECT_TRUE(env::is_valid_query_driver("/usr/bin/c++", &reason));
    EXPECT_FALSE(env::is_valid_query_driver("/usr/bin/notacompiler", &reason));
    EXPECT_FALSE(env::is_valid_query_driver("/etc/passwd", &reason));
}

}  // namespace
