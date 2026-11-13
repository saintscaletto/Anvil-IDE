#pragma once

#include <sys/types.h>

#include <cstddef>
#include <vector>

namespace anvil::security {

// Syscall profiles are deny-lists layered on top of an allow-by-default
// filter. The kernel still enforces DAC, namespaces and Landlock-like mounts
// from bubblewrap; seccomp closes the remaining high-risk primitives.
enum class SeccompProfile {
    Lsp,     // no sockets, no ptrace, no mount/namespace changes; exec allowed for bwrap
    LspDev,  // development-only fallback with the same syscall policy
    Build,   // exec allowed for compilers, no sockets, no ptrace/mount
    None,
};

class SeccompProgram {
public:
    explicit SeccompProgram(SeccompProfile profile);

    [[nodiscard]] SeccompProfile profile() const noexcept { return profile_; }
    [[nodiscard]] bool empty() const noexcept { return instructions_.empty(); }

    // Raw cBPF array, exactly the format expected by seccomp_export_bpf and
    // bwrap --seccomp. Size is always a multiple of 8 bytes.
    [[nodiscard]] const std::vector<unsigned char>& bytes() const noexcept {
        return instructions_;
    }

    // Installs the filter in the calling thread. Requires no_new_privs. Used
    // only for direct development spawns; production LSP processes get the
    // filter through bubblewrap.
    bool install_in_current_process() const;

private:
    SeccompProfile profile_;
    std::vector<unsigned char> instructions_;
};

}  // namespace anvil::security
