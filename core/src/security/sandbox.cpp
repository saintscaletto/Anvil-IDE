#include "anvil/security/sandbox.hpp"

#include <fcntl.h>
#include <linux/memfd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <set>
#include <system_error>
#include <utility>

#include "anvil/error.hpp"
#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/process.hpp"

namespace anvil::security {
namespace fs = std::filesystem;
namespace {

std::mutex g_probe_mutex;
std::optional<SandboxAvailability> g_probe;

struct ProbeResult {
    int exit_code = -1;
    std::string output;
    bool spawn_ok = false;
};

void append_mount(std::vector<std::string>& argv, const std::string& flag,
                  const fs::path& source, const fs::path& target) {
    argv.push_back(flag);
    argv.push_back(source.string());
    argv.push_back(target.string());
}

void append_symlink(std::vector<std::string>& argv, const std::string& target,
                    const fs::path& link) {
    argv.push_back("--symlink");
    argv.push_back(target);
    argv.push_back(link.string());
}

void append_ro_bind_if_exists(std::vector<std::string>& argv, const fs::path& path) {
    std::error_code ec;
    if (fs::exists(path, ec)) {
        argv.push_back("--ro-bind");
        argv.push_back(path.string());
        argv.push_back(path.string());
    }
}

std::string read_all(int fd, std::size_t max_bytes) {
    std::string out;
    std::array<char, 4096> buffer{};
    while (out.size() < max_bytes) {
        const ssize_t count = ::read(fd, buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (count == 0) break;
        out.append(buffer.data(), static_cast<std::size_t>(count));
    }
    return out;
}

ProbeResult run_probe(const std::vector<std::string>& argv) {
    SpawnRequest request;
    request.argv = argv;
    request.env = {"PATH=/usr/local/bin:/usr/bin:/bin", "LANG=C.UTF-8"};
    request.cwd = "/";
    request.stdin_pipe = false;
    request.capture_stdout = true;
    request.capture_stderr = true;
    request.new_session = true;

    std::string error;
    auto child = ChildProcess::spawn(request, &error);
    if (!child.has_value()) {
        return {.exit_code = -1, .output = error, .spawn_ok = false};
    }
    child->close_stdin();
    const std::string stdout_text = read_all(child->stdout_fd(), 64U * 1024U);
    const std::string stderr_text = read_all(child->stderr_fd(), 64U * 1024U);
    const int status = child->wait();
    ProbeResult result;
    result.spawn_ok = status >= 0;
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result.output = !stderr_text.empty() ? stderr_text : stdout_text;
    return result;
}

bool is_merged_usr_path(const char* path, const char* expected_target) {
    std::error_code ec;
    const fs::path candidate(path);
    if (!fs::is_symlink(candidate, ec)) return false;
    const fs::path target = fs::read_symlink(candidate, ec);
    return !ec && target == expected_target;
}

std::vector<std::string> base_mount_arguments() {
    std::vector<std::string> argv;
    append_mount(argv, "--ro-bind", "/usr", "/usr");

    // Ubuntu 24.04 is merged-/usr. On other layouts fall back to read-only
    // binds so /bin/sh and the ELF loader remain available.
    if (is_merged_usr_path("/bin", "usr/bin")) {
        append_symlink(argv, "usr/bin", "/bin");
    } else {
        append_ro_bind_if_exists(argv, "/bin");
    }
    if (is_merged_usr_path("/sbin", "usr/sbin")) {
        append_symlink(argv, "usr/sbin", "/sbin");
    } else {
        append_ro_bind_if_exists(argv, "/sbin");
    }
    if (is_merged_usr_path("/lib", "usr/lib")) {
        append_symlink(argv, "usr/lib", "/lib");
    } else {
        append_ro_bind_if_exists(argv, "/lib");
    }
    if (is_merged_usr_path("/lib64", "usr/lib64")) {
        append_symlink(argv, "usr/lib64", "/lib64");
    } else {
        append_ro_bind_if_exists(argv, "/lib64");
    }

    for (const char* path : {"/etc/ld.so.cache", "/etc/alternatives", "/etc/passwd",
                             "/etc/group", "/etc/nsswitch.conf", "/etc/localtime"}) {
        append_ro_bind_if_exists(argv, path);
    }

    argv.insert(argv.end(), {"--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp"});
    argv.insert(argv.end(), {"--unshare-pid", "--unshare-ipc", "--unshare-uts",
                             "--die-with-parent", "--new-session", "--cap-drop", "ALL"});
    return argv;
}

SandboxAvailability probe_uncached() {
    SandboxAvailability availability;
    const auto bwrap = resolve_executable("bwrap");
    if (!bwrap.has_value()) {
        availability.reason = "bubblewrap (bwrap) is not installed";
        return availability;
    }
    availability.bwrap_path = *bwrap;

    const ProbeResult version = run_probe({*bwrap, "--version"});
    if (!version.spawn_ok || version.exit_code != 0) {
        availability.reason = "bubblewrap could not be executed: " + version.output;
        return availability;
    }

    std::vector<std::string> base{*bwrap};
    const auto mounts = base_mount_arguments();
    base.insert(base.end(), mounts.begin(), mounts.end());

    auto with_net = base;
    with_net.insert(with_net.end(), {"--unshare-net", "/usr/bin/true"});
    const ProbeResult net_result = run_probe(with_net);
    if (net_result.spawn_ok && net_result.exit_code == 0) {
        availability.available = true;
        availability.network_namespace = true;
        availability.reason = "bubblewrap with network namespace isolation";
        return availability;
    }

    auto without_net = base;
    without_net.push_back("/usr/bin/true");
    const ProbeResult basic_result = run_probe(without_net);
    if (basic_result.spawn_ok && basic_result.exit_code == 0) {
        availability.available = true;
        availability.network_namespace = false;
        availability.reason =
            "bubblewrap without a network namespace; seccomp still denies sockets";
        return availability;
    }

    availability.reason = "bubblewrap sandbox probe failed: " + basic_result.output;
    return availability;
}

std::string resolve_real_program(const std::string& program) {
    if (program.empty() || program.front() != '/') {
        throw ApiError(ErrorCode::SandboxUnavailable,
                       "Sandboxed programs must be specified by absolute path");
    }
    std::error_code ec;
    const fs::path resolved = fs::canonical(program, ec);
    if (ec) {
        throw ApiError(ErrorCode::SandboxUnavailable,
                       "Cannot resolve sandboxed program: " + program);
    }
    if (::access(resolved.c_str(), X_OK) != 0) {
        throw ApiError(ErrorCode::SandboxUnavailable,
                       "Sandboxed program is not executable: " + resolved.string());
    }
    return resolved.string();
}

bool has_prefix(const fs::path& path, const fs::path& prefix) {
    auto path_it = path.begin();
    auto prefix_it = prefix.begin();
    for (; prefix_it != prefix.end(); ++prefix_it, ++path_it) {
        if (path_it == path.end() || *path_it != *prefix_it) return false;
    }
    return true;
}

std::vector<std::string> clean_environment(const fs::path& cache_dir,
                                           const SandboxSpec& spec) {
    std::vector<std::pair<std::string, std::string>> env{
        {"PATH", "/usr/local/bin:/usr/bin:/bin"},
        {"HOME", (cache_dir / "home").string()},
        {"XDG_CACHE_HOME", (cache_dir / "xdg-cache").string()},
        {"XDG_CONFIG_HOME", (cache_dir / "xdg-config").string()},
        {"TMPDIR", "/tmp"},
        {"LANG", "C.UTF-8"},
        {"TERM", "dumb"},
        {"NO_COLOR", "1"},
        {"ANVIL_SANDBOX", "1"},
    };
    for (const auto& [key, value] : spec.extra_env) {
        bool valid_key = !key.empty() &&
                         ((key.front() >= 'A' && key.front() <= 'Z') ||
                          (key.front() >= 'a' && key.front() <= 'z') || key.front() == '_');
        for (const char c : key) {
            valid_key = valid_key &&
                        ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                         (c >= '0' && c <= '9') || c == '_');
        }
        if (!valid_key || value.find('\0') != std::string::npos) {
            throw ApiError(ErrorCode::InvalidParams, "Invalid extra environment entry");
        }
        if (key == "PATH" || key == "HOME" || key == "XDG_CACHE_HOME" ||
            key == "XDG_CONFIG_HOME" || key == "TMPDIR") {
            continue;  // never let a project override sandbox paths
        }
        auto existing = std::find_if(env.begin(), env.end(),
                                     [&](const auto& item) { return item.first == key; });
        if (existing != env.end()) {
            existing->second = value;
        } else {
            env.emplace_back(key, value);
        }
    }
    std::vector<std::string> result;
    result.reserve(env.size());
    for (auto& [key, value] : env) {
        result.push_back(key + "=" + value);
    }
    return result;
}

void prepare_cache_dir(const fs::path& cache_dir) {
    if (cache_dir.empty() || !cache_dir.is_absolute()) {
        throw ApiError(ErrorCode::SandboxUnavailable, "Sandbox cache directory is invalid");
    }
    fsutil::ensure_private_directory(cache_dir);
    for (const auto& subdir : {"home", "xdg-cache", "xdg-config"}) {
        fsutil::ensure_private_directory(cache_dir / subdir);
    }
    struct stat info {};
    if (::lstat(cache_dir.c_str(), &info) != 0 || !S_ISDIR(info.st_mode) ||
        info.st_uid != ::geteuid()) {
        throw ApiError(ErrorCode::SandboxUnavailable,
                       "Sandbox cache directory must be a directory owned by the user");
    }
}

int create_seccomp_memfd(const SeccompProgram& seccomp) {
    const int fd = static_cast<int>(
        ::syscall(SYS_memfd_create, "anvil-seccomp", MFD_CLOEXEC));
    if (fd < 0) {
        throw ApiError(ErrorCode::SandboxUnavailable,
                       std::string("memfd_create failed: ") + std::strerror(errno));
    }
    const auto& bytes = seccomp.bytes();
    std::size_t written = 0;
    while (written < bytes.size()) {
        const ssize_t count = ::write(fd, bytes.data() + written, bytes.size() - written);
        if (count < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            throw ApiError(ErrorCode::SandboxUnavailable,
                           std::string("writing seccomp filter failed: ") + std::strerror(errno));
        }
        written += static_cast<std::size_t>(count);
    }
    if (::lseek(fd, 0, SEEK_SET) != 0) {
        ::close(fd);
        throw ApiError(ErrorCode::SandboxUnavailable, "seeking seccomp filter failed");
    }
    return fd;
}

}  // namespace

const SandboxAvailability& sandbox_availability() {
    std::lock_guard lock(g_probe_mutex);
    if (!g_probe.has_value()) {
        g_probe = probe_uncached();
        if (g_probe->available) {
            logging::info("sandbox available: " + g_probe->reason);
        } else {
            logging::warn("sandbox unavailable: " + g_probe->reason);
            logging::security_event("sandbox.unavailable", Json{{"reason", g_probe->reason}});
        }
    }
    return *g_probe;
}

void reset_sandbox_probe_for_tests() {
    std::lock_guard lock(g_probe_mutex);
    g_probe.reset();
}

SandboxCommand::SandboxCommand(SandboxCommand&& other) noexcept {
    *this = std::move(other);
}

SandboxCommand& SandboxCommand::operator=(SandboxCommand&& other) noexcept {
    if (this == &other) return *this;
    close_inherited_fd();
    argv = std::move(other.argv);
    env = std::move(other.env);
    cwd = std::move(other.cwd);
    inherited_fd = other.inherited_fd;
    sandboxed = other.sandboxed;
    other.inherited_fd = -1;
    return *this;
}

SandboxCommand::~SandboxCommand() { close_inherited_fd(); }

void SandboxCommand::close_inherited_fd() {
    if (inherited_fd >= 0) {
        ::close(inherited_fd);
        inherited_fd = -1;
    }
}

bool is_trusted_system_program(const std::string& absolute_path) {
    std::error_code ec;
    const fs::path resolved = fs::canonical(absolute_path, ec);
    if (ec) return false;
    for (const char* prefix : {"/usr", "/bin", "/sbin", "/lib", "/lib64", "/opt"}) {
        if (has_prefix(resolved, prefix)) return true;
    }
    return false;
}

SandboxCommand build_sandbox_command(const SandboxSpec& spec, const SeccompProgram& seccomp,
                                     const std::string& program,
                                     const std::vector<std::string>& arguments,
                                     const SandboxAvailability* availability_override) {
    const SandboxAvailability& availability =
        availability_override != nullptr ? *availability_override : sandbox_availability();
    if (!availability.available) {
        throw ApiError(ErrorCode::SandboxUnavailable,
                       "Language servers require bubblewrap, which is unavailable: " +
                           availability.reason);
    }
    if (spec.workspace.empty()) {
        throw ApiError(ErrorCode::SandboxUnavailable, "Sandbox requires a workspace");
    }

    std::error_code ec;
    const fs::path workspace = fs::canonical(spec.workspace, ec);
    if (ec || !fs::is_directory(workspace)) {
        throw ApiError(ErrorCode::SandboxUnavailable, "Sandbox workspace is invalid");
    }
    const std::string real_program = resolve_real_program(program);
    if (!is_trusted_system_program(real_program)) {
        throw ApiError(ErrorCode::SandboxUnavailable,
                       "Language server is outside trusted system prefixes: " + real_program);
    }
    prepare_cache_dir(spec.cache_dir);

    std::vector<std::string> writable;
    for (const auto& requested : spec.writable_paths) {
        const fs::path canonical = fs::canonical(requested, ec);
        if (ec) {
            throw ApiError(ErrorCode::SandboxUnavailable,
                           "Writable sandbox path does not exist: " + requested.string());
        }
        if (!fsutil::is_within(workspace, canonical)) {
            throw ApiError(ErrorCode::SandboxUnavailable,
                           "Writable sandbox path is outside the workspace: " +
                               canonical.string());
        }
        if (canonical == workspace && spec.workspace_read_only) {
            throw ApiError(ErrorCode::SandboxUnavailable,
                           "Cannot make the workspace writable while it is mounted read-only");
        }
        writable.push_back(canonical.string());
    }

    SandboxCommand command;
    command.sandboxed = true;
    command.cwd = workspace;
    command.env = clean_environment(spec.cache_dir, spec);
    command.inherited_fd = create_seccomp_memfd(seccomp);

    command.argv.push_back(availability.bwrap_path);
    const auto base = base_mount_arguments();
    command.argv.insert(command.argv.end(), base.begin(), base.end());

    const fs::path workspace_path(workspace);
    command.argv.insert(command.argv.end(),
                        {"--ro-bind", workspace_path.string(), workspace_path.string()});
    for (const auto& path : writable) {
        command.argv.insert(command.argv.end(), {"--bind", path, path});
    }
    command.argv.insert(command.argv.end(),
                        {"--bind", spec.cache_dir.string(), spec.cache_dir.string()});
    if (!spec.allow_network && availability.network_namespace) {
        command.argv.push_back("--unshare-net");
    }
    if (has_prefix(fs::path(real_program), "/opt")) {
        command.argv.insert(command.argv.end(), {"--ro-bind", "/opt", "/opt"});
    }

    command.argv.insert(command.argv.end(), {"--clearenv"});
    for (const auto& entry : command.env) {
        const std::size_t equals = entry.find('=');
        if (equals == std::string::npos) continue;
        command.argv.insert(command.argv.end(),
                            {"--setenv", entry.substr(0, equals), entry.substr(equals + 1)});
    }
    command.argv.insert(command.argv.end(),
                        {"--seccomp", "3", "--chdir", workspace_path.string(), "--",
                         real_program});
    command.argv.insert(command.argv.end(), arguments.begin(), arguments.end());

    logging::security_event("sandbox.spawn",
                            Json{{"program", real_program},
                                 {"workspace", workspace.string()},
                                 {"networkNamespace", availability.network_namespace},
                                 {"writablePaths", writable},
                                 {"execAllowed", spec.allow_exec}});
    return command;
}

SandboxCommand build_unsandboxed_dev_command(const SandboxSpec& spec,
                                             const SeccompProgram& seccomp,
                                             const std::string& program,
                                             const std::vector<std::string>& arguments) {
#if ANVIL_DEV_MODE
    (void)seccomp;
    const char* override_env = std::getenv("ANVIL_DEV_UNSAFE_LSP");
    if (override_env == nullptr || std::string(override_env) != "1") {
        throw ApiError(ErrorCode::SandboxUnavailable,
                       "Unsandboxed development spawn requires ANVIL_DEV_UNSAFE_LSP=1");
    }
    const std::string real_program = resolve_real_program(program);
    prepare_cache_dir(spec.cache_dir);

    SandboxCommand command;
    command.sandboxed = false;
    command.cwd = spec.workspace;
    command.env = clean_environment(spec.cache_dir, spec);
    command.argv.push_back(real_program);
    command.argv.insert(command.argv.end(), arguments.begin(), arguments.end());
    logging::security_event("sandbox.unsandboxed_dev_spawn",
                            Json{{"program", real_program},
                                 {"workspace", spec.workspace.string()},
                                 {"warning", "development override; no filesystem isolation"}});
    return command;
#else
    (void)spec;
    (void)seccomp;
    (void)program;
    (void)arguments;
    throw ApiError(ErrorCode::SandboxUnavailable,
                   "Unsandboxed language servers are disabled in release builds");
#endif
}

}  // namespace anvil::security
