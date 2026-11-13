#include "anvil/security/seccomp.hpp"

#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>

#if defined(__x86_64__)
#define ANVIL_AUDIT_ARCH AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
#define ANVIL_AUDIT_ARCH AUDIT_ARCH_AARCH64
#else
#define ANVIL_AUDIT_ARCH 0
#endif

namespace anvil::security {
namespace {

using Instruction = struct sock_filter;

void append(std::vector<Instruction>& program, Instruction instruction) {
    program.push_back(instruction);
}

void append_deny(std::vector<Instruction>& program, int syscall_number) {
    append(program, BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
                             static_cast<__u32>(syscall_number), 0, 1));
    append(program, BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM));
}

// socket(AF_UNIX, ...) and socketpair(AF_UNIX, ...) remain available for
// local IPC; every other address family is denied.
void append_deny_non_unix_socket(std::vector<Instruction>& program, int syscall_number) {
    append(program, BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
                             static_cast<__u32>(syscall_number), 0, 4));
    append(program, BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                             static_cast<__u32>(offsetof(struct seccomp_data, args[0]))));
    append(program, BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AF_UNIX, 1, 0));
    append(program, BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM));
    append(program, BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
}

std::vector<Instruction> build(SeccompProfile profile) {
    std::vector<Instruction> program;
    program.reserve(128);

    append(program, BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                             static_cast<__u32>(offsetof(struct seccomp_data, arch))));
    append(program, BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ANVIL_AUDIT_ARCH, 1, 0));
    append(program, BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));

    append(program, BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                             static_cast<__u32>(offsetof(struct seccomp_data, nr))));

    // Do not deny execve here. bubblewrap installs this filter as the last
    // step before its own execvp() of the payload, so denying exec would
    // prevent the language server from starting at all. no_new_privs, the
    // filesystem namespace, read-only workspace and network denial remain in
    // force; a compromised server can execute system binaries only inside the
    // same sandbox.
    if (profile == SeccompProfile::Lsp || profile == SeccompProfile::LspDev ||
        profile == SeccompProfile::Build) {
        append_deny(program, __NR_ptrace);
        append_deny(program, __NR_process_vm_readv);
        append_deny(program, __NR_process_vm_writev);
        append_deny(program, __NR_mount);
        append_deny(program, __NR_umount2);
        append_deny(program, __NR_pivot_root);
        append_deny(program, __NR_chroot);
        append_deny(program, __NR_unshare);
        append_deny(program, __NR_setns);
        append_deny(program, __NR_bpf);
        append_deny(program, __NR_keyctl);
        append_deny(program, __NR_add_key);
        append_deny(program, __NR_request_key);
        append_deny(program, __NR_kexec_load);
        append_deny(program, __NR_init_module);
        append_deny(program, __NR_finit_module);
        append_deny(program, __NR_open_by_handle_at);
        append_deny(program, __NR_name_to_handle_at);
        append_deny(program, __NR_perf_event_open);
        append_deny(program, __NR_userfaultfd);
        append_deny(program, __NR_io_uring_setup);
        append_deny(program, __NR_io_uring_enter);
        append_deny(program, __NR_io_uring_register);
        append_deny_non_unix_socket(program, __NR_socket);
        append_deny_non_unix_socket(program, __NR_socketpair);
        append_deny(program, __NR_connect);
        append_deny(program, __NR_bind);
        append_deny(program, __NR_listen);
        append_deny(program, __NR_accept);
        append_deny(program, __NR_accept4);
        append_deny(program, __NR_sendto);
        append_deny(program, __NR_recvfrom);
    }

    append(program, BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
    return program;
}

}  // namespace

SeccompProgram::SeccompProgram(SeccompProfile profile) : profile_(profile) {
    if (profile == SeccompProfile::None) return;
    const auto program = build(profile);
    instructions_.resize(program.size() * sizeof(Instruction));
    std::memcpy(instructions_.data(), program.data(), instructions_.size());
}

bool SeccompProgram::install_in_current_process() const {
    if (instructions_.empty()) return true;
    if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) return false;

    struct sock_fprog filter {};
    filter.len = static_cast<unsigned short>(instructions_.size() / sizeof(Instruction));
    filter.filter = reinterpret_cast<struct sock_filter*>(
        const_cast<unsigned char*>(instructions_.data()));

    return ::syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &filter) == 0;
}

}  // namespace anvil::security
