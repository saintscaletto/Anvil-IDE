#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include <unistd.h>

#include "anvil/ipc/jsonrpc.hpp"
#include "anvil/ipc/server.hpp"
#include "anvil/ipc/validate.hpp"
#include "anvil/security/sandbox.hpp"
#include "anvil/security/seccomp.hpp"
#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/strings.hpp"
#include "anvil/version.hpp"
#include "anvil/workspace/workspace.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kUsage =
    "Usage: anvil-core [--stdio] [--version] [--self-test] [--dev]\n"
    "                  [--config-dir PATH] [--cache-dir PATH]\n"
    "\n"
    "Anvil native core. Speaks newline-delimited JSON-RPC 2.0 on stdin/stdout.\n";

fs::path home_directory() {
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') return fs::path(home);
    return fs::path("/tmp") / ("anvil-" + std::to_string(::getuid()));
}

fs::path default_config_dir() {
    const char* override_path = std::getenv("ANVIL_CONFIG_DIR");
    if (override_path != nullptr && *override_path != '\0') return fs::path(override_path);
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && *xdg != '\0') return fs::path(xdg) / "anvil";
    return home_directory() / ".config" / "anvil";
}

fs::path default_cache_dir() {
    const char* override_path = std::getenv("ANVIL_CACHE_DIR");
    if (override_path != nullptr && *override_path != '\0') return fs::path(override_path);
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    if (xdg != nullptr && *xdg != '\0') return fs::path(xdg) / "anvil";
    return home_directory() / ".cache" / "anvil";
}

bool rejects_traversal() {
    try {
        (void)anvil::ipc::validate::parse_fs_read(
            anvil::Json{{"path", "../etc/passwd"}});
        return false;
    } catch (const anvil::ApiError&) {
        return true;
    }
}

bool rejects_absolute_path() {
    try {
        (void)anvil::ipc::validate::parse_fs_read(
            anvil::Json{{"path", "/etc/passwd"}});
        return false;
    } catch (const anvil::ApiError&) {
        return true;
    }
}

int self_test(const fs::path& config_dir, const fs::path& cache_dir) {
    anvil::Json checks = anvil::Json::object();
    bool ok = true;

    checks["version"] = std::string(anvil::kVersion);
    checks["apiVersion"] = anvil::kApiVersion;
    checks["utf8Valid"] = anvil::strings::is_valid_utf8("Anvil — C++");
    checks["utf8RejectsOverlong"] = !anvil::strings::is_valid_utf8("\xC0\xAF");
    checks["pathTraversalRejected"] = rejects_traversal();
    checks["absolutePathRejected"] = rejects_absolute_path();
    checks["jsonDepthRejected"] =
        !anvil::ipc::json_depth_within(std::string(80, '{'), 64);
    checks["seccompFilterBuilt"] =
        !anvil::security::SeccompProgram(anvil::security::SeccompProfile::Lsp)
             .bytes()
             .empty();

    for (const char* flag : {"pathTraversalRejected", "absolutePathRejected",
                             "jsonDepthRejected", "seccompFilterBuilt"}) {
        if (checks[flag] != true) ok = false;
    }

    try {
        anvil::fsutil::ensure_private_directory(config_dir);
        anvil::fsutil::ensure_private_directory(cache_dir);
        const fs::path probe = config_dir / ".self-test-probe";
        anvil::fsutil::write_text_file_atomic(probe, "ok", std::nullopt);
        const std::string content = anvil::fsutil::read_text_file(probe, 1024);
        fs::remove(probe);
        checks["configWritable"] = content == "ok";
        if (!checks["configWritable"].get<bool>()) ok = false;
    } catch (const std::exception& error) {
        checks["configWritable"] = false;
        checks["configError"] = error.what();
        ok = false;
    }

    const auto& sandbox = anvil::security::sandbox_availability();
    checks["sandboxAvailable"] = sandbox.available;
    checks["sandboxReason"] = sandbox.reason;
    checks["sandboxNetworkNamespace"] = sandbox.network_namespace;

    std::cout << anvil::Json{{"ok", ok}, {"checks", checks}}.dump(2) << std::endl;
    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);

    fs::path config_dir = default_config_dir();
    fs::path cache_dir = default_cache_dir();
    bool dev_mode = false;
    bool self_test_requested = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--version" || argument == "-v") {
            std::cout << anvil::kCoreName << " " << anvil::kVersion << " (api "
                      << anvil::kApiVersion << ")" << std::endl;
            return 0;
        }
        if (argument == "--help" || argument == "-h") {
            std::cout << kUsage;
            return 0;
        }
        if (argument == "--stdio") continue;
        if (argument == "--self-test") {
            self_test_requested = true;
            continue;
        }
        if (argument == "--dev") {
            dev_mode = true;
            continue;
        }
        if (argument == "--config-dir" && i + 1 < argc) {
            config_dir = argv[++i];
            continue;
        }
        if (argument == "--cache-dir" && i + 1 < argc) {
            cache_dir = argv[++i];
            continue;
        }
        std::cerr << "anvil-core: unknown argument: " << argument << "\n" << kUsage;
        return 2;
    }

    try {
        anvil::fsutil::ensure_private_directory(config_dir);
        anvil::fsutil::ensure_private_directory(cache_dir);
        anvil::logging::initialize(config_dir, dev_mode);

        if (self_test_requested) {
            const int result = self_test(config_dir, cache_dir);
            anvil::logging::shutdown();
            return result;
        }

        anvil::logging::security_event(
            "core.start",
            anvil::Json{{"version", std::string(anvil::kVersion)},
                        {"apiVersion", anvil::kApiVersion},
                        {"devMode", dev_mode},
                        {"configDir", config_dir.string()}});
        anvil::ServerOptions options;
        options.config_dir = config_dir;
        options.cache_dir = cache_dir;
        options.dev_mode = dev_mode;
        anvil::Server server(std::move(options));
        const int result = server.run();
        anvil::logging::security_event("core.exit", anvil::Json{{"code", result}});
        anvil::logging::shutdown();
        return result;
    } catch (const std::exception& error) {
        std::cerr << "anvil-core: fatal: " << error.what() << std::endl;
        anvil::logging::error(std::string("fatal: ") + error.what());
        anvil::logging::shutdown();
        return 1;
    }
}
