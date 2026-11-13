#pragma once

#include <sys/types.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "anvil/security/seccomp.hpp"

namespace anvil {

struct SpawnRequest {
    std::vector<std::string> argv;
    std::vector<std::string> env;
    std::filesystem::path cwd;
    bool new_session = false;
    bool stdin_pipe = true;
    bool capture_stdout = true;
    bool capture_stderr = true;
    // Applied in the child before execve. Used for direct development spawns;
    // production processes get their filter through bubblewrap.
    const security::SeccompProgram* seccomp = nullptr;
    // When set, the fd is duplicated to fd 3 in the child (used to hand a
    // compiled BPF program to bwrap --seccomp).
    int inherited_fd = -1;
};

// Owns a child process and its pipes. All fd access is non-owning for the
// caller: the ChildProcess closes them on destruction.
class ChildProcess {
public:
    ChildProcess() = default;
    ~ChildProcess();

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
    ChildProcess(ChildProcess&& other) noexcept;
    ChildProcess& operator=(ChildProcess&& other) noexcept;

    // On failure returns nullopt and fills error_message. The child does not
    // use a shell under any circumstances.
    static std::optional<ChildProcess> spawn(const SpawnRequest& request,
                                             std::string* error_message);

    [[nodiscard]] pid_t pid() const noexcept { return pid_; }
    [[nodiscard]] int stdin_fd() const noexcept { return stdin_fd_; }
    [[nodiscard]] int stdout_fd() const noexcept { return stdout_fd_; }
    [[nodiscard]] int stderr_fd() const noexcept { return stderr_fd_; }
    [[nodiscard]] bool valid() const noexcept { return pid_ > 0; }

    // Transfers ownership of a pipe end to the caller. The ChildProcess will
    // no longer close it.
    int release_stdin();
    int release_stdout();
    int release_stderr();

    bool write_stdin(const char* data, std::size_t size);
    bool write_stdin(const std::string& data);
    void close_stdin();

    // Returns the raw wait status; -1 on error. Does not close fds.
    int wait();
    // Polls with WNOHANG; nullopt when still running.
    std::optional<int> try_wait();
    void kill(int signal);
    void terminate();

    // Closes stdout/stderr reading ends. Safe to call more than once.
    void close_output();

private:
    void close_all();
    void reap_if_needed();

    pid_t pid_ = -1;
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    int stderr_fd_ = -1;
    bool reaped_ = false;
    int wait_status_ = 0;
};

// Resolves a bare executable name through PATH or validates an absolute path.
// Returns an empty optional if it is not executable.
std::optional<std::string> resolve_executable(const std::string& name);

// Buffered newline-delimited reader. Unlike a bare read() loop it never
// discards bytes that follow a newline, which matters for JSON-RPC streams.
class FdLineReader {
public:
    explicit FdLineReader(int fd) : fd_(fd) {}

    // Blocks until a full line, EOF or an oversized line. Returns false on
    // EOF/error. When too_large is set the caller must treat the stream as
    // compromised and stop using it.
    bool read_line(std::string* line, std::size_t max_bytes, bool* too_large,
                   std::string* error_message);

private:
    int fd_;
    std::string buffer_;
};


}  // namespace anvil
