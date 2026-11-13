#include <gtest/gtest.h>

#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

#include "anvil/security/seccomp.hpp"

namespace {

using anvil::security::SeccompProfile;
using anvil::security::SeccompProgram;

int run_in_child(const SeccompProgram& program, void (*body)()) {
    const pid_t pid = ::fork();
    if (pid == 0) {
        if (!program.install_in_current_process()) _exit(125);
        body();
        _exit(0);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (!WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
}

void exec_true() {
    ::execl("/bin/true", "true", static_cast<char*>(nullptr));
    _exit(127);
}

void open_inet_socket() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    _exit(fd < 0 && errno == EPERM ? 0 : 1);
}

TEST(Seccomp, FilterIsCbpf) {
    for (const auto profile : {SeccompProfile::Lsp, SeccompProfile::LspDev, SeccompProfile::Build}) {
        const SeccompProgram program(profile);
        EXPECT_FALSE(program.bytes().empty());
        EXPECT_EQ(program.bytes().size() % 8, 0U);
    }
    const SeccompProgram none(SeccompProfile::None);
    EXPECT_TRUE(none.bytes().empty());
    EXPECT_TRUE(none.install_in_current_process());
}

TEST(Seccomp, DevProfileAllowsInitialExec) {
    const SeccompProgram program(SeccompProfile::LspDev);
    EXPECT_EQ(run_in_child(program, exec_true), 0);
}

TEST(Seccomp, LspProfileAllowsBwrapInitialExec) {
    // bwrap applies the filter immediately before execvp(); an exec-deny
    // filter would make the production sandbox unable to launch clangd.
    const SeccompProgram program(SeccompProfile::Lsp);
    EXPECT_EQ(run_in_child(program, exec_true), 0);
}

TEST(Seccomp, LspProfileDeniesInetSockets) {
    const SeccompProgram program(SeccompProfile::Lsp);
    EXPECT_EQ(run_in_child(program, open_inet_socket), 0);
}

}  // namespace
