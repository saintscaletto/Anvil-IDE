#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "anvil/security/seccomp.hpp"

namespace anvil::security {

struct SandboxAvailability {
    bool available = false;
    bool network_namespace = false;
    std::string bwrap_path;
    std::string reason;
};

// Probes bubblewrap once and caches the result. The probe is deliberately
// cheap and non-destructive: bwrap --version, then a minimal /usr/bin/true
// sandbox with network first, falling back to a seccomp-isolated network when
// the kernel refuses a network namespace.
const SandboxAvailability& sandbox_availability();
void reset_sandbox_probe_for_tests();

struct SandboxSpec {
    std::filesystem::path workspace;
    bool workspace_read_only = true;
    std::vector<std::filesystem::path> writable_paths;
    std::filesystem::path cache_dir;
    bool allow_network = false;
    bool allow_exec = false;
    std::vector<std::pair<std::string, std::string>> extra_env;
};

struct SandboxCommand {
    std::vector<std::string> argv;
    std::vector<std::string> env;
    std::filesystem::path cwd;
    int inherited_fd = -1;  // owned by the caller, close after spawn
    bool sandboxed = false;

    SandboxCommand() = default;
    SandboxCommand(const SandboxCommand&) = delete;
    SandboxCommand& operator=(const SandboxCommand&) = delete;
    SandboxCommand(SandboxCommand&& other) noexcept;
    SandboxCommand& operator=(SandboxCommand&& other) noexcept;
    ~SandboxCommand();

    void close_inherited_fd();
};

// Builds a bubblewrap invocation. Throws ApiError(SandboxUnavailable) when the
// sandbox cannot be used. Program must be an absolute, executable path under a
// trusted system prefix (or an absolute path inside /opt); language servers in
// user-writable locations are rejected.
SandboxCommand build_sandbox_command(const SandboxSpec& spec,
                                     const SeccompProgram& seccomp,
                                     const std::string& program,
                                     const std::vector<std::string>& arguments,
                                     const SandboxAvailability* availability_override = nullptr);

// Development-only fallback used when ANVIL_DEV_MODE is compiled in and the
// explicit ANVIL_DEV_UNSAFE_LSP=1 override is present. Applies seccomp but no
// filesystem namespace isolation. Every use is audited.
SandboxCommand build_unsandboxed_dev_command(const SandboxSpec& spec,
                                             const SeccompProgram& seccomp,
                                             const std::string& program,
                                             const std::vector<std::string>& arguments);

bool is_trusted_system_program(const std::string& absolute_path);

}  // namespace anvil::security
