#include "anvil/environment.hpp"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <utility>

#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/strings.hpp"

namespace anvil::environment {
namespace {

constexpr std::size_t kMaxDatabaseBytes = 8U * 1024U * 1024U;
constexpr std::size_t kMaxEntriesPerDatabase = 20'000;
constexpr std::size_t kMaxDiscoveredDatabases = 16;
constexpr std::size_t kMaxIncludeRoots = 32;
constexpr std::size_t kMaxQueryDrivers = 4;
constexpr std::size_t kMaxInspectedDirectories = 256;

// Directory names that must never be exposed to a language server, whether
// they are named by project data or by a settings file. These hold
// credentials, keys, and browser sessions.
constexpr const char* kSensitiveComponents[] = {
    ".ssh",     ".gnupg",  ".aws",          ".azure",    ".kube",   ".config",
    ".password-store",     ".pki",          ".mozilla",  ".thunderbird",
    ".docker",  ".cache",  ".npm",          ".git-credentials",     ".netrc",
    ".npmrc",   ".pypirc", ".vault-token",  "keyrings",  "credentials",
    "secrets",  "private-keys",
};

// System trees that a configured include path may not expose. /usr, /opt,
// /usr/local, /srv, /data and $HOME are handled separately.
constexpr const char* kDeniedSystemRoots[] = {"/etc", "/root", "/proc", "/sys",
                                              "/dev", "/run", "/boot"};

std::string trim_copy(std::string_view value) {
    const std::size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) return {};
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(begin, end - begin + 1));
}

bool is_control_free(const std::string& value) {
    for (const unsigned char c : value) {
        if (c < 0x20 || c == 0x7f) return false;
    }
    return true;
}

std::optional<fs::path> canonical_directory(const fs::path& candidate) {
    std::error_code ec;
    const fs::path canonical = fs::canonical(candidate, ec);
    if (ec || !fs::is_directory(canonical, ec)) return std::nullopt;
    return canonical;
}

bool has_prefix(const fs::path& path, const fs::path& prefix) {
    auto path_it = path.begin();
    auto prefix_it = prefix.begin();
    for (; prefix_it != prefix.end(); ++prefix_it, ++path_it) {
        if (path_it == path.end() || *path_it != *prefix_it) return false;
    }
    return true;
}

bool under_any(const fs::path& path, const char* const* roots, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        if (has_prefix(path, roots[index])) return true;
    }
    return false;
}

std::optional<fs::path> home_directory() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') return std::nullopt;
    return canonical_directory(home);
}

std::string lowercase(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool is_compiler_name(const std::string& name) {
    // gcc, g++, cc, c++, clang, clang++ with an optional version suffix.
    for (const char* base : {"gcc", "g++", "cc", "c++", "clang", "clang++"}) {
        if (name == base) return true;
        if (strings::starts_with(name, std::string(base) + "-")) {
            const std::string suffix = name.substr(std::string(base).size() + 1);
            if (!suffix.empty() &&
                std::all_of(suffix.begin(), suffix.end(),
                            [](unsigned char c) { return std::isdigit(c) || c == '.'; })) {
                return true;
            }
        }
    }
    return false;
}

bool looks_like_include_flag(const std::string& argument) {
    return argument == "-I" || argument == "-isystem" || argument == "-iquote" ||
           argument == "-idirafter" || argument == "--include-directory";
}

bool is_include_flag_prefix(const std::string& argument, std::string_view* remainder) {
    for (const char* prefix : {"-isystem", "-iquote", "-idirafter", "-I"}) {
        const std::size_t length = std::string_view(prefix).size();
        if (argument.size() > length && argument.compare(0, length, prefix) == 0) {
            *remainder = std::string_view(argument.data() + length, argument.size() - length);
            return true;
        }
    }
    constexpr std::string_view kLong = "--include-directory=";
    if (argument.size() > kLong.size() && argument.compare(0, kLong.size(), kLong) == 0) {
        *remainder =
            std::string_view(argument.data() + kLong.size(), argument.size() - kLong.size());
        return true;
    }
    return false;
}

void append_unique(std::vector<std::string>& values, std::set<std::string>& seen,
                   const std::string& value, std::size_t max_values) {
    if (values.size() >= max_values) return;
    if (!seen.insert(value).second) return;
    values.push_back(value);
}

fs::path resolve_against(const fs::path& base, std::string_view raw) {
    const fs::path candidate(trim_copy(raw));
    if (candidate.is_absolute()) return candidate;
    return base / candidate;
}

}  // namespace

bool is_sensitive_path(const fs::path& path) {
    for (const auto& component : path) {
        const std::string name = lowercase(component.string());
        if (name.empty() || name == "/" || name == ".") continue;
        for (const char* sensitive : kSensitiveComponents) {
            if (name == sensitive) return true;
        }
    }
    return false;
}

bool is_valid_include_path(std::string_view value, std::string* reason) {
    if (value.empty() || value.size() > 1024) {
        if (reason != nullptr) *reason = "include path must be 1..1024 bytes";
        return false;
    }
    if (value.front() != '/') {
        if (reason != nullptr) *reason = "include path must be absolute";
        return false;
    }
    if (value == "/") {
        if (reason != nullptr) *reason = "include path must not be the filesystem root";
        return false;
    }
    if (!is_control_free(std::string(value))) {
        if (reason != nullptr) *reason = "include path contains control characters";
        return false;
    }
    if (!strings::is_valid_utf8(value)) {
        if (reason != nullptr) *reason = "include path is not valid UTF-8";
        return false;
    }
    return true;
}

bool is_valid_query_driver(std::string_view value, std::string* reason) {
    if (value.empty()) return true;
    if (!is_valid_include_path(value, reason)) return false;
    const fs::path path{std::string(value)};
    if (!is_compiler_name(path.filename().string())) {
        if (reason != nullptr) *reason = "query driver must be a compiler executable";
        return false;
    }
    if (!canonical_directory(path.parent_path()).has_value()) {
        if (reason != nullptr) *reason = "query driver directory does not exist";
        return false;
    }
    return true;
}

IncludePolicy classify_include_root(const fs::path& candidate, const fs::path& workspace,
                                    const fs::path& cache_dir) {
    const auto canonical = canonical_directory(candidate);
    if (!canonical.has_value()) return IncludePolicy::skip;
    if (is_sensitive_path(*canonical)) {
        // Never mountable, so never offered for approval either.
        logging::security_event("environment.sensitive_include_ignored",
                                Json{{"path", canonical->string()}});
        return IncludePolicy::skip;
    }
    if (!workspace.empty() && fsutil::is_within(workspace, *canonical)) {
        return IncludePolicy::skip;  // the workspace is already mounted read-only
    }
    if (!cache_dir.empty() && fsutil::is_within(cache_dir, *canonical)) {
        return IncludePolicy::skip;  // private cache, already writable
    }
    if (has_prefix(*canonical, "/usr") && !has_prefix(*canonical, "/usr/local")) {
        return IncludePolicy::skip;  // /usr is mounted as a whole
    }
    if (has_prefix(*canonical, "/usr/local") || has_prefix(*canonical, "/opt")) {
        return IncludePolicy::automatic;
    }
    const auto home = home_directory();
    if (home.has_value() && has_prefix(*canonical, *home)) {
        return *canonical == *home ? IncludePolicy::pending : IncludePolicy::automatic;
    }
    return IncludePolicy::pending;
}

bool is_mountable_include_root(const fs::path& candidate, bool configured,
                               const fs::path& workspace, const fs::path& cache_dir) {
    const auto canonical = canonical_directory(candidate);
    if (!canonical.has_value()) return false;
    if (is_sensitive_path(*canonical)) {
        logging::security_event("sandbox.include_root_denied",
                                Json{{"path", canonical->string()},
                                     {"reason", "sensitive directory"}});
        return false;
    }
    if (!workspace.empty() && fsutil::is_within(workspace, *canonical)) return false;
    if (!cache_dir.empty() && fsutil::is_within(cache_dir, *canonical)) return false;
    if (!configured) {
        if (has_prefix(*canonical, "/usr") && !has_prefix(*canonical, "/usr/local")) {
            return true;  // already mounted
        }
        if (has_prefix(*canonical, "/usr/local") || has_prefix(*canonical, "/opt")) {
            return true;
        }
        const auto home = home_directory();
        return home.has_value() && has_prefix(*canonical, *home) && *canonical != *home;
    }
    if (under_any(*canonical, kDeniedSystemRoots,
                  sizeof(kDeniedSystemRoots) / sizeof(kDeniedSystemRoots[0]))) {
        logging::security_event("sandbox.include_root_denied",
                                Json{{"path", canonical->string()},
                                     {"reason", "reserved system tree"}});
        return false;
    }
    return true;
}

std::vector<std::string> split_command_line(std::string_view command) {
    std::vector<std::string> tokens;
    std::string current;
    bool single_quote = false;
    bool double_quote = false;
    bool escaped = false;
    for (const char c : command) {
        if (escaped) {
            current.push_back(c);
            escaped = false;
            continue;
        }
        if (c == '\\' && !single_quote) {
            escaped = true;
            continue;
        }
        if (c == '\'' && !double_quote) {
            single_quote = !single_quote;
            continue;
        }
        if (c == '"' && !single_quote) {
            double_quote = !double_quote;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c)) != 0 && !single_quote && !double_quote) {
            if (!current.empty()) {
                tokens.push_back(std::move(current));
                current.clear();
            }
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty()) tokens.push_back(std::move(current));
    return tokens;
}

std::vector<CompileCommandEntry> load_compile_commands(const fs::path& file) {
    std::vector<CompileCommandEntry> entries;
    std::string error;
    const auto loaded = fsutil::try_read_json_file(file, kMaxDatabaseBytes, &error);
    if (!loaded.has_value() || !loaded->is_array()) {
        if (!error.empty()) {
            logging::warn("compile_commands.json ignored (" + file.string() + "): " + error);
        }
        return entries;
    }
    const fs::path base = file.parent_path();
    for (const auto& item : *loaded) {
        if (entries.size() >= kMaxEntriesPerDatabase) break;
        if (!item.is_object()) continue;
        if (!item.contains("directory") || !item["directory"].is_string()) continue;
        if (!item.contains("file") || !item["file"].is_string()) continue;

        std::vector<std::string> arguments;
        if (item.contains("arguments") && item["arguments"].is_array()) {
            for (const auto& argument : item["arguments"]) {
                if (!argument.is_string()) {
                    arguments.clear();
                    break;
                }
                arguments.push_back(argument.get<std::string>());
            }
        }
        if (arguments.empty()) {
            if (!item.contains("command") || !item["command"].is_string()) continue;
            arguments = split_command_line(item["command"].get<std::string>());
        }
        if (arguments.empty()) continue;

        const fs::path directory =
            fs::path(item["directory"].get<std::string>()).is_absolute()
                ? fs::path(item["directory"].get<std::string>())
                : base / item["directory"].get<std::string>();
        fs::path source(item["file"].get<std::string>());
        if (!source.is_absolute()) source = directory / source;

        CompileCommandEntry entry;
        entry.directory = directory.lexically_normal().string();
        entry.file = source.lexically_normal().string();
        entry.arguments = std::move(arguments);
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<fs::path> discover_compile_commands(const fs::path& root) {
    std::vector<fs::path> found;
    std::set<std::string> seen;
    std::size_t inspected = 0;

    auto add_if_present = [&](const fs::path& directory) {
        if (found.size() >= kMaxDiscoveredDatabases) return;
        if (inspected >= kMaxInspectedDirectories) return;
        ++inspected;
        std::error_code ec;
        const fs::path candidate = directory / "compile_commands.json";
        if (!fs::is_regular_file(candidate, ec)) return;
        if (!seen.insert(directory.string()).second) return;
        found.push_back(directory);
    };

    add_if_present(root);
    for (const char* relative : {"build", "out", ".build", "cmake-build-debug",
                                 "cmake-build-release", "cmake-build-relwithdebinfo",
                                 "build/Debug", "build/Release", "build/debug", "build/release"}) {
        add_if_present(root / relative);
    }

    // colcon puts one database in each build/<package> directory; unique CMake
    // projects use build-<name>. Both are one level below a known parent.
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (found.size() >= kMaxDiscoveredDatabases || inspected >= kMaxInspectedDirectories) break;
        if (!entry.is_directory(ec)) continue;
        const std::string name = entry.path().filename().string();
        const bool colcon_parent = name == "build" || name == "out" || name == ".build";
        const bool cmake_variant = strings::starts_with(name, "cmake-build-") ||
                                   strings::starts_with(name, "build-");
        if (!colcon_parent && !cmake_variant) continue;
        add_if_present(entry.path());
        for (const auto& child : fs::directory_iterator(entry.path(), ec)) {
            if (found.size() >= kMaxDiscoveredDatabases ||
                inspected >= kMaxInspectedDirectories) {
                break;
            }
            if (child.is_directory(ec)) add_if_present(child.path());
        }
    }
    return found;
}

std::vector<std::string> extract_include_roots(
    const std::vector<CompileCommandEntry>& entries) {
    std::vector<std::string> roots;
    std::set<std::string> seen;
    for (const auto& entry : entries) {
        const fs::path base(entry.directory);
        for (std::size_t index = 0; index < entry.arguments.size(); ++index) {
            const std::string& argument = entry.arguments[index];
            std::string raw;
            if (looks_like_include_flag(argument)) {
                if (index + 1 >= entry.arguments.size()) continue;
                raw = entry.arguments[++index];
            } else {
                std::string_view remainder;
                if (!is_include_flag_prefix(argument, &remainder)) continue;
                raw = std::string(remainder);
                if (raw.empty()) continue;
            }
            const auto canonical = canonical_directory(resolve_against(base, raw));
            if (!canonical.has_value()) continue;
            append_unique(roots, seen, canonical->string(), kMaxIncludeRoots);
        }
    }
    return roots;
}

std::vector<std::string> extract_query_drivers(
    const std::vector<CompileCommandEntry>& entries) {
    std::vector<std::string> drivers;
    std::set<std::string> seen;
    for (const auto& entry : entries) {
        if (entry.arguments.empty()) continue;
        std::error_code ec;
        const fs::path candidate = fs::weakly_canonical(entry.arguments.front(), ec);
        if (ec || !fs::is_regular_file(candidate, ec)) continue;
        if (!is_compiler_name(candidate.filename().string())) continue;
        if (::access(candidate.c_str(), X_OK) != 0) continue;
        append_unique(drivers, seen, candidate.string(), kMaxQueryDrivers);
    }
    return drivers;
}

Resolution resolve(const fs::path& workspace_root, const fs::path& output_dir,
                   const std::vector<std::string>& configured_include_paths) {
    Resolution resolution;

    std::vector<CompileCommandEntry> merged_entries;
    std::set<std::string> merged_keys;
    for (const auto& directory : discover_compile_commands(workspace_root)) {
        for (auto& entry : load_compile_commands(directory / "compile_commands.json")) {
            const std::string key = entry.directory + '\n' + entry.file;
            if (!merged_keys.insert(key).second) continue;
            merged_entries.push_back(std::move(entry));
        }
    }
    resolution.entry_count = merged_entries.size();

    if (!merged_entries.empty() && !output_dir.empty()) {
        Json database = Json::array();
        for (const auto& entry : merged_entries) {
            Json item{{"directory", entry.directory}, {"file", entry.file}};
            item["arguments"] = entry.arguments;
            database.push_back(std::move(item));
        }
        try {
            fsutil::ensure_private_directory(output_dir);
            fsutil::write_json_file_atomic(output_dir / "compile_commands.json", database);
            resolution.compile_commands_dir = output_dir;
        } catch (const std::exception& error) {
            logging::warn(std::string("cannot write merged compile_commands.json: ") +
                          error.what());
        }
    }

    std::error_code ec;
    const fs::path root = fs::canonical(workspace_root, ec);
    const fs::path cache = output_dir.empty() ? fs::path() : fs::canonical(output_dir, ec);

    std::set<std::string> configured_seen;
    for (const auto& raw : configured_include_paths) {
        const auto canonical = canonical_directory(raw);
        if (!canonical.has_value()) {
            logging::warn("configured include path does not exist: " + raw);
            continue;
        }
        if (!is_mountable_include_root(*canonical, true, root, cache)) {
            logging::warn("configured include path is refused: " + raw);
            continue;
        }
        append_unique(resolution.configured_include_roots, configured_seen,
                      canonical->string(), kMaxIncludeRoots);
    }

    std::set<std::string> automatic_seen;
    for (const auto& raw : extract_include_roots(merged_entries)) {
        if (configured_seen.count(raw) != 0) continue;
        switch (classify_include_root(raw, root, cache)) {
            case IncludePolicy::automatic:
                append_unique(resolution.automatic_include_roots, automatic_seen, raw,
                              kMaxIncludeRoots);
                break;
            case IncludePolicy::pending:
                append_unique(resolution.pending_include_roots, automatic_seen, raw,
                              kMaxIncludeRoots);
                break;
            case IncludePolicy::skip:
                break;
        }
    }

    resolution.query_drivers = extract_query_drivers(merged_entries);
    return resolution;
}

}  // namespace anvil::environment
