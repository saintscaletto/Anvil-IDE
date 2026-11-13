#pragma once

#include <filesystem>
#include <shared_mutex>
#include <string>

namespace anvil {

// Owns the single trusted workspace root. Every IPC path is resolved through
// this class; symlinks are canonicalised before the containment check so a
// link inside the project cannot escape it.
class Workspace {
public:
    Workspace() = default;

    void open(const std::filesystem::path& requested_root);
    void close();

    [[nodiscard]] bool is_open() const;
    [[nodiscard]] std::string root_string() const;
    [[nodiscard]] std::string name() const;
    [[nodiscard]] std::string state_key() const;

    // Throws NotFound when the path does not exist and PathOutsideWorkspace
    // when it escapes the root (including through a symlink).
    std::filesystem::path resolve(const std::string& relative_path) const;

    // Resolves a path that may not exist yet. The parent must exist and stay
    // inside the workspace.
    std::filesystem::path resolve_for_create(const std::string& relative_path) const;

    // Resolves the parent and appends the final component without following a
    // symlink at the final position. Used by rename/delete/stat so operating on
    // a symlink affects the link, not its target.
    std::filesystem::path resolve_nofollow(const std::string& relative_path) const;

    // Throws PathOutsideWorkspace when the absolute path is not contained.
    std::string relative(const std::filesystem::path& absolute_path) const;

private:
    mutable std::shared_mutex mutex_;
    std::filesystem::path root_;
    std::string state_key_;
};

}  // namespace anvil
