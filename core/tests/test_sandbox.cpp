#include <gtest/gtest.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>

#include "anvil/security/sandbox.hpp"
#include "anvil/security/seccomp.hpp"
#include "anvil/util/fs_util.hpp"
#include "test_util.hpp"

namespace {

using anvil::security::SandboxAvailability;
using anvil::security::SandboxSpec;
using anvil::security::SeccompProfile;
using anvil::security::SeccompProgram;

bool contains(const std::vector<std::string>& values, const std::string& needle) {
    return std::find(values.begin(), values.end(), needle) != values.end();
}

TEST(Sandbox, TrustedPrograms) {
    EXPECT_TRUE(anvil::security::is_trusted_system_program("/usr/bin/true"));
    EXPECT_TRUE(anvil::security::is_trusted_system_program("/bin/true"));
    anvil::test::TempDir temp;
    anvil::test::write_file(temp.path() / "prog", "x");
    ::chmod((temp.path() / "prog").c_str(), 0755);
    EXPECT_FALSE(anvil::security::is_trusted_system_program((temp.path() / "prog").string()));
}

TEST(Sandbox, BuildsIsolatedArgv) {
    anvil::test::TempDir temp;
    const auto workspace = temp.path() / "project";
    const auto build = workspace / "build";
    const auto cache = temp.path() / "cache";
    std::filesystem::create_directories(build);
    std::filesystem::create_directories(cache);
    anvil::test::write_file(workspace / "main.cpp", "int main() {}\n");

    SandboxSpec spec;
    spec.workspace = workspace;
    spec.workspace_read_only = true;
    spec.writable_paths = {build};
    spec.cache_dir = cache;
    spec.allow_network = false;

    const SeccompProgram seccomp(SeccompProfile::Lsp);
    const SandboxAvailability availability{
        .available = true,
        .network_namespace = true,
        .bwrap_path = "/usr/bin/bwrap",
        .reason = "test",
    };
    auto command = anvil::security::build_sandbox_command(
        spec, seccomp, "/usr/bin/true", {"--version"}, &availability);

    EXPECT_TRUE(command.sandboxed);
    EXPECT_EQ(command.argv.front(), "/usr/bin/bwrap");
    EXPECT_TRUE(contains(command.argv, "--unshare-net"));
    EXPECT_TRUE(contains(command.argv, "--unshare-pid"));
    EXPECT_TRUE(contains(command.argv, "--die-with-parent"));
    EXPECT_TRUE(contains(command.argv, "--new-session"));
    EXPECT_TRUE(contains(command.argv, "--clearenv"));
    EXPECT_TRUE(contains(command.argv, "--seccomp"));
    EXPECT_TRUE(contains(command.argv, "3"));
    EXPECT_TRUE(contains(command.argv, "--cap-drop"));
    EXPECT_TRUE(contains(command.argv, "ALL"));
    EXPECT_TRUE(contains(command.argv, "--ro-bind"));
    EXPECT_TRUE(contains(command.argv, "--bind"));
    EXPECT_TRUE(contains(command.argv, "/usr/bin/true"));
    EXPECT_TRUE(contains(command.argv, "--"));
    EXPECT_GE(command.inherited_fd, 0);
    EXPECT_TRUE(std::any_of(command.env.begin(), command.env.end(), [](const std::string& entry) {
        return entry.rfind("HOME=", 0) == 0;
    }));
    EXPECT_TRUE(std::none_of(command.argv.begin(), command.argv.end(), [](const std::string& arg) {
        return arg == "sh" || arg == "-c";
    }));

    const SandboxAvailability no_net{
        .available = true,
        .network_namespace = false,
        .bwrap_path = "/usr/bin/bwrap",
        .reason = "test",
    };
    auto no_net_command = anvil::security::build_sandbox_command(
        spec, seccomp, "/usr/bin/true", {}, &no_net);
    EXPECT_FALSE(contains(no_net_command.argv, "--unshare-net"));
}

TEST(Sandbox, RejectsUnsafeWritablePathsAndPrograms) {
    anvil::test::TempDir temp;
    const auto workspace = temp.path() / "project";
    const auto cache = temp.path() / "cache";
    std::filesystem::create_directories(workspace);
    std::filesystem::create_directories(cache);
    anvil::test::write_file(temp.path() / "prog", "x");
    ::chmod((temp.path() / "prog").c_str(), 0755);

    SandboxSpec spec;
    spec.workspace = workspace;
    spec.cache_dir = cache;
    spec.writable_paths = {temp.path()};
    const SeccompProgram seccomp(SeccompProfile::Lsp);
    const SandboxAvailability availability{
        .available = true,
        .network_namespace = true,
        .bwrap_path = "/usr/bin/bwrap",
        .reason = "test",
    };
    EXPECT_THROW(anvil::security::build_sandbox_command(
                     spec, seccomp, "/usr/bin/true", {}, &availability),
                 anvil::ApiError);

    spec.writable_paths.clear();
    EXPECT_THROW(anvil::security::build_sandbox_command(
                     spec, seccomp, (temp.path() / "prog").string(), {}, &availability),
                 anvil::ApiError);
}

TEST(Sandbox, UnavailableFailsClosed) {
    anvil::test::TempDir temp;
    SandboxSpec spec;
    spec.workspace = temp.path();
    spec.cache_dir = temp.path() / "cache";
    const SeccompProgram seccomp(SeccompProfile::Lsp);
    const SandboxAvailability unavailable{false, false, "", "no bwrap"};
    EXPECT_THROW(anvil::security::build_sandbox_command(
                     spec, seccomp, "/usr/bin/true", {}, &unavailable),
                 anvil::ApiError);
}

}  // namespace
