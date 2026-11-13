#include "anvil/util/process.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>

#include "anvil/util/log.hpp"
#include "anvil/util/strings.hpp"

namespace anvil {
namespace {

void close_fd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

std::vector<char*> to_argv(const std::vector<std::string>& values) {
    std::vector<char*> result;
    result.reserve(values.size() + 1);
    for (const auto& value : values) {
        result.push_back(const_cast<char*>(value.c_str()));
    }
    result.push_back(nullptr);
    return result;
}

bool path_is_executable(const std::filesystem::path& path) {
    struct stat info {};
    return ::stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode) &&
           ::access(path.c_str(), X_OK) == 0;
}

}  // namespace

std::optional<std::string> resolve_executable(const std::string& name) {
    if (name.empty()) return std::nullopt;
    if (name.find('/') != std::string::npos) {
        const std::filesystem::path path(name);
        if (!path.is_absolute()) return std::nullopt;
        if (path_is_executable(path)) return path.string();
        return std::nullopt;
    }

    const char* path_env = std::getenv("PATH");
    const std::string search_path = path_env != nullptr ? path_env : "/usr/local/bin:/usr/bin:/bin";
    for (const auto& component : strings::split(search_path, ':')) {
        if (component.empty()) continue;
        const std::filesystem::path candidate =
            std::filesystem::path(component) / name;
        if (path_is_executable(candidate)) return candidate.string();
    }
    return std::nullopt;
}

ChildProcess::~ChildProcess() { close_all(); }

ChildProcess::ChildProcess(ChildProcess&& other) noexcept {
    *this = std::move(other);
}

ChildProcess& ChildProcess::operator=(ChildProcess&& other) noexcept {
    if (this == &other) return *this;
    close_all();
    pid_ = other.pid_;
    stdin_fd_ = other.stdin_fd_;
    stdout_fd_ = other.stdout_fd_;
    stderr_fd_ = other.stderr_fd_;
    reaped_ = other.reaped_;
    wait_status_ = other.wait_status_;
    other.pid_ = -1;
    other.stdin_fd_ = -1;
    other.stdout_fd_ = -1;
    other.stderr_fd_ = -1;
    return *this;
}

std::optional<ChildProcess> ChildProcess::spawn(const SpawnRequest& request,
                                                std::string* error_message) {
    if (request.argv.empty() || request.argv.front().empty()) {
        if (error_message != nullptr) *error_message = "empty argv";
        return std::nullopt;
    }
    if (!request.cwd.empty() && ::access(request.cwd.c_str(), X_OK) != 0) {
        if (error_message != nullptr) {
            *error_message = "working directory is not accessible: " + request.cwd.string();
        }
        return std::nullopt;
    }

    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    auto cleanup_pipes = [&]() {
        for (int* fd : {&stdin_pipe[0], &stdin_pipe[1], &stdout_pipe[0], &stdout_pipe[1],
                        &stderr_pipe[0], &stderr_pipe[1]}) {
            close_fd(*fd);
        }
    };

    if (request.stdin_pipe && (::pipe2(stdin_pipe, O_CLOEXEC) != 0)) {
        if (error_message != nullptr) *error_message = std::string("pipe2: ") + std::strerror(errno);
        return std::nullopt;
    }
    if (request.capture_stdout && (::pipe2(stdout_pipe, O_CLOEXEC) != 0)) {
        if (error_message != nullptr) *error_message = std::string("pipe2: ") + std::strerror(errno);
        cleanup_pipes();
        return std::nullopt;
    }
    if (request.capture_stderr && (::pipe2(stderr_pipe, O_CLOEXEC) != 0)) {
        if (error_message != nullptr) *error_message = std::string("pipe2: ") + std::strerror(errno);
        cleanup_pipes();
        return std::nullopt;
    }

    const std::vector<char*> argv = to_argv(request.argv);
    const std::vector<char*> envp = to_argv(request.env);
    const pid_t parent_pid = ::getpid();

    const pid_t pid = ::fork();
    if (pid < 0) {
        if (error_message != nullptr) *error_message = std::string("fork: ") + std::strerror(errno);
        cleanup_pipes();
        return std::nullopt;
    }

    if (pid == 0) {
        // Child: only async-signal-safe calls from here to execve.
        ::prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0);
        if (::getppid() != parent_pid) _exit(127);
        ::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

        if (request.new_session) ::setsid();

        if (request.stdin_pipe) {
            if (::dup2(stdin_pipe[0], STDIN_FILENO) < 0) _exit(127);
        } else {
            const int devnull = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            if (devnull < 0) _exit(127);
            if (::dup2(devnull, STDIN_FILENO) < 0) _exit(127);
            ::close(devnull);
        }

        if (request.capture_stdout) {
            if (::dup2(stdout_pipe[1], STDOUT_FILENO) < 0) _exit(127);
        } else {
            const int devnull = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
            if (devnull < 0) _exit(127);
            if (::dup2(devnull, STDOUT_FILENO) < 0) _exit(127);
            ::close(devnull);
        }

        if (request.capture_stderr) {
            if (::dup2(stderr_pipe[1], STDERR_FILENO) < 0) _exit(127);
        } else {
            const int devnull = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
            if (devnull < 0) _exit(127);
            if (::dup2(devnull, STDERR_FILENO) < 0) _exit(127);
            ::close(devnull);
        }

        if (request.inherited_fd >= 0) {
            if (::dup2(request.inherited_fd, 3) < 0) _exit(127);
        }

        // Close every descriptor except the standard three and the optional
        // inherited fd at 3.
        if (request.inherited_fd >= 0) {
            ::syscall(SYS_close_range, 4U, ~0U, 0U);
        } else {
            ::syscall(SYS_close_range, 3U, ~0U, 0U);
        }

        if (!request.cwd.empty() && ::chdir(request.cwd.c_str()) != 0) _exit(127);

        if (request.seccomp != nullptr && !request.seccomp->install_in_current_process()) {
            _exit(126);
        }

        ::execve(request.argv.front().c_str(), argv.data(), envp.data());
        _exit(127);
    }

    // Parent.
    if (stdin_pipe[0] >= 0) close_fd(stdin_pipe[0]);
    if (stdout_pipe[1] >= 0) close_fd(stdout_pipe[1]);
    if (stderr_pipe[1] >= 0) close_fd(stderr_pipe[1]);

    ChildProcess child;
    child.pid_ = pid;
    child.stdin_fd_ = stdin_pipe[1];
    child.stdout_fd_ = stdout_pipe[0];
    child.stderr_fd_ = stderr_pipe[0];
    return child;
}

int ChildProcess::release_stdin() {
    const int fd = stdin_fd_;
    stdin_fd_ = -1;
    return fd;
}

int ChildProcess::release_stdout() {
    const int fd = stdout_fd_;
    stdout_fd_ = -1;
    return fd;
}

int ChildProcess::release_stderr() {
    const int fd = stderr_fd_;
    stderr_fd_ = -1;
    return fd;
}

bool ChildProcess::write_stdin(const char* data, std::size_t size) {
    if (stdin_fd_ < 0) return false;
    while (size > 0) {
        const ssize_t written = ::write(stdin_fd_, data, size);
        if (written < 0) {
            if (errno == EINTR) continue;
            if (errno == EPIPE) return false;
            return false;
        }
        if (written == 0) return false;
        data += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

bool ChildProcess::write_stdin(const std::string& data) {
    return write_stdin(data.data(), data.size());
}

void ChildProcess::close_stdin() { close_fd(stdin_fd_); }

void ChildProcess::close_output() {
    close_fd(stdout_fd_);
    close_fd(stderr_fd_);
}

int ChildProcess::wait() {
    if (pid_ <= 0) return -1;
    if (reaped_) return wait_status_;
    int status = 0;
    pid_t result = 0;
    do {
        result = ::waitpid(pid_, &status, 0);
    } while (result < 0 && errno == EINTR);
    if (result < 0) return -1;
    reaped_ = true;
    wait_status_ = status;
    return status;
}

std::optional<int> ChildProcess::try_wait() {
    if (pid_ <= 0) return std::nullopt;
    if (reaped_) return wait_status_;
    int status = 0;
    const pid_t result = ::waitpid(pid_, &status, WNOHANG);
    if (result == 0) return std::nullopt;
    if (result < 0) {
        if (errno == EINTR) return std::nullopt;
        return -1;
    }
    reaped_ = true;
    wait_status_ = status;
    return status;
}

void ChildProcess::kill(int signal) {
    if (pid_ > 0 && !reaped_) ::kill(pid_, signal);
}

void ChildProcess::terminate() {
    if (pid_ <= 0 || reaped_) return;
    ::kill(pid_, SIGTERM);
    for (int i = 0; i < 20; ++i) {
        const auto status = try_wait();
        if (status.has_value()) return;
        ::usleep(25'000);
    }
    ::kill(pid_, SIGKILL);
    wait();
}

void ChildProcess::close_all() {
    close_stdin();
    close_output();
    reap_if_needed();
}

void ChildProcess::reap_if_needed() {
    if (pid_ <= 0 || reaped_) return;
    const auto status = try_wait();
    if (status.has_value()) return;
    ::kill(pid_, SIGKILL);
    wait();
}

bool FdLineReader::read_line(std::string* line, std::size_t max_bytes, bool* too_large,
                             std::string* error_message) {
    if (too_large != nullptr) *too_large = false;
    line->clear();
    if (fd_ < 0) return false;
    std::array<char, 8192> chunk{};
    while (true) {
        const std::size_t newline = buffer_.find('\n');
        if (newline != std::string::npos) {
            if (newline > max_bytes) {
                if (too_large != nullptr) *too_large = true;
                return false;
            }
            line->assign(buffer_, 0, newline);
            buffer_.erase(0, newline + 1);
            return true;
        }
        if (buffer_.size() > max_bytes) {
            if (too_large != nullptr) *too_large = true;
            return false;
        }
        const ssize_t count = ::read(fd_, chunk.data(), chunk.size());
        if (count < 0) {
            if (errno == EINTR) continue;
            if (error_message != nullptr) *error_message = std::strerror(errno);
            return false;
        }
        if (count == 0) {
            if (!buffer_.empty()) {
                if (buffer_.size() > max_bytes) {
                    if (too_large != nullptr) *too_large = true;
                    return false;
                }
                line->swap(buffer_);
                buffer_.clear();
                return true;
            }
            return false;
        }
        buffer_.append(chunk.data(), static_cast<std::size_t>(count));
    }
}

}  // namespace anvil
