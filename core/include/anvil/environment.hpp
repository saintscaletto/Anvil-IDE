#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace anvil::environment {

namespace fs = std::filesystem;

// A single compile_commands.json entry, normalised to absolute paths. The
// original arguments are preserved verbatim; only directory and file are
// rewritten.
struct CompileCommandEntry {
    std::string directory;
    std::string file;
    std::vector<std::string> arguments;
};

// How an include root discovered in project data may be used.
enum class IncludePolicy {
    skip,       // already mounted, or not a directory
    automatic,  // trusted prefix or a safe user dependency root
    pending,    // needs explicit approval (settings or the UI banner)
};

struct Resolution {
    // Directory passed to clangd via --compile-commands-dir. Empty when the
    // project has no compile_commands.json anywhere.
    fs::path compile_commands_dir;
    std::size_t entry_count = 0;
    // Include roots mounted read-only without user action.
    std::vector<std::string> automatic_include_roots;
    // Include roots explicitly configured in settings (explicit consent).
    std::vector<std::string> configured_include_roots;
    // Include roots referenced by compile_commands that are neither safe to
    // mount automatically nor configured. The UI offers to approve them.
    std::vector<std::string> pending_include_roots;
    // Compiler executables referenced by compile_commands that are safe to pass
    // to clangd --query-driver (system compilers only).
    std::vector<std::string> query_drivers;
};

// Discovers compile_commands.json files (project root, common build layouts,
// and colcon's build/<pkg>/ layout), merges them into one cache copy, and
// classifies include roots and compilers. Pure data transformation: no process
// is executed and no project file is written.
//
// output_dir must be a writable directory that is bind-mounted into the
// sandbox; the merged database is written there so clangd can read it.
Resolution resolve(const fs::path& workspace_root, const fs::path& output_dir,
                   const std::vector<std::string>& configured_include_paths);

// Classifies a directory referenced by project data.
IncludePolicy classify_include_root(const fs::path& candidate, const fs::path& workspace,
                                    const fs::path& cache_dir);

// True when a path is (or is inside) a directory that must never be exposed to
// a language server: credential stores, browser profiles, keyrings.
bool is_sensitive_path(const fs::path& path);

// Sandbox-side re-validation. Automatic roots must satisfy the project-data
// policy; configured roots additionally allow other read-only data mounts
// (for example /srv or /data) but still reject secrets and system internals.
bool is_mountable_include_root(const fs::path& candidate, bool configured,
                               const fs::path& workspace, const fs::path& cache_dir);

// Syntax validation shared by Settings and the IPC validator. Does not touch
// the filesystem: a path may be recorded before the dependency is installed.
bool is_valid_include_path(std::string_view value, std::string* reason);
bool is_valid_query_driver(std::string_view value, std::string* reason);

// Exposed for tests.
std::vector<fs::path> discover_compile_commands(const fs::path& root);
std::vector<CompileCommandEntry> load_compile_commands(const fs::path& file);
std::vector<std::string> extract_include_roots(const std::vector<CompileCommandEntry>& entries);
std::vector<std::string> extract_query_drivers(const std::vector<CompileCommandEntry>& entries);
std::vector<std::string> split_command_line(std::string_view command);

}  // namespace anvil::environment
