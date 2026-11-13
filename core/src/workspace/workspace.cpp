#include "anvil/workspace/workspace.hpp"

#include <unistd.h>

#include <mutex>
#include <system_error>

#include "anvil/error.hpp"
#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/strings.hpp"

namespace anvil {
namespace fs = std::filesystem;

void Workspace::open(const fs::path& requested_root) {
    std::error_code ec;
    const fs::path canonical = fs::canonical(requested_root, ec);
    if (ec) {
        throw ApiError(ErrorCode::NotFound,
                       "Workspace does not exist: " + requested_root.string());
    }
    if (!fs::is_directory(canonical)) {
        throw ApiError(ErrorCode::Unsupported,
                       "Workspace is not a directory: " + canonical.string());
    }
    if (canonical == canonical.root_path()) {
        throw ApiError(ErrorCode::PermissionDenied,
                       "Refusing to use the filesystem root as a workspace");
    }
    if (::access(canonical.c_str(), R_OK | X_OK) != 0) {
        throw ApiError(ErrorCode::PermissionDenied,
                       "Workspace is not readable: " + canonical.string());
    }
    if (canonical.string().size() > 4096) {
        throw ApiError(ErrorCode::InvalidParams, "Workspace path is too long");
    }

    std::unique_lock lock(mutex_);
    root_ = canonical;
    // A short hash-like key avoids leaking the absolute path into filenames.
    state_key_ = std::to_string(strings::fnv1a64(root_.string())) + "-" +
                 std::to_string(root_.string().size());
    logging::info("workspace opened: " + root_.string());
    logging::security_event("workspace.open", Json{{"root", root_.string()}});
}

void Workspace::close() {
    std::unique_lock lock(mutex_);
    if (root_.empty()) return;
    logging::security_event("workspace.close", Json{{"root", root_.string()}});
    root_.clear();
    state_key_.clear();
}

bool Workspace::is_open() const {
    std::shared_lock lock(mutex_);
    return !root_.empty();
}

std::string Workspace::root_string() const {
    std::shared_lock lock(mutex_);
    return root_.string();
}

std::string Workspace::name() const {
    std::shared_lock lock(mutex_);
    if (root_.empty()) return {};
    const std::string filename = root_.filename().string();
    return filename.empty() ? root_.string() : filename;
}

std::string Workspace::state_key() const {
    std::shared_lock lock(mutex_);
    return state_key_;
}

fs::path Workspace::resolve(const std::string& relative_path) const {
    std::shared_lock lock(mutex_);
    if (root_.empty()) {
        throw ApiError(ErrorCode::WorkspaceNotOpen, "No workspace is open");
    }
    if (relative_path.empty()) return root_;

    std::error_code ec;
    const fs::path candidate = fs::canonical(root_ / fs::path(relative_path), ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            throw ApiError(ErrorCode::NotFound,
                           "Path not found: " + relative_path,
                           Json{{"path", relative_path}});
        }
        throw ApiError(ErrorCode::PermissionDenied,
                       "Cannot resolve path: " + relative_path + ": " + ec.message(),
                       Json{{"path", relative_path}});
    }
    if (!fsutil::is_within(root_, candidate)) {
        logging::security_event("path.outside_workspace",
                                Json{{"path", relative_path}, {"resolved", candidate.string()}});
        throw ApiError(ErrorCode::PathOutsideWorkspace,
                       "Path is outside the workspace: " + relative_path,
                       Json{{"path", relative_path}});
    }
    return candidate;
}

fs::path Workspace::resolve_for_create(const std::string& relative_path) const {
    std::shared_lock lock(mutex_);
    if (root_.empty()) {
        throw ApiError(ErrorCode::WorkspaceNotOpen, "No workspace is open");
    }
    if (relative_path.empty()) {
        throw ApiError(ErrorCode::InvalidParams, "A file or directory path is required");
    }

    std::error_code ec;
    const fs::path candidate = fs::weakly_canonical(root_ / fs::path(relative_path), ec);
    if (ec) {
        throw ApiError(ErrorCode::PermissionDenied,
                       "Cannot resolve path: " + relative_path + ": " + ec.message());
    }
    if (!fsutil::is_within(root_, candidate)) {
        logging::security_event("path.outside_workspace",
                                Json{{"path", relative_path}, {"resolved", candidate.string()}});
        throw ApiError(ErrorCode::PathOutsideWorkspace,
                       "Path is outside the workspace: " + relative_path);
    }
    const fs::path parent = candidate.parent_path();
    if (!fs::is_directory(parent)) {
        throw ApiError(ErrorCode::NotFound,
                       "Parent directory does not exist: " + relative_path);
    }
    return candidate;
}

fs::path Workspace::resolve_nofollow(const std::string& relative_path) const {
    std::shared_lock lock(mutex_);
    if (root_.empty()) {
        throw ApiError(ErrorCode::WorkspaceNotOpen, "No workspace is open");
    }
    if (relative_path.empty()) return root_;

    std::error_code ec;
    const fs::path parent = fs::canonical(root_ / fs::path(relative_path).parent_path(), ec);
    if (ec) {
        throw ApiError(ErrorCode::NotFound,
                       "Parent path not found: " + relative_path);
    }
    if (!fsutil::is_within(root_, parent)) {
        logging::security_event("path.outside_workspace",
                                Json{{"path", relative_path}, {"resolved", parent.string()}});
        throw ApiError(ErrorCode::PathOutsideWorkspace,
                       "Path is outside the workspace: " + relative_path);
    }
    return parent / fs::path(relative_path).filename();
}

std::string Workspace::relative(const fs::path& absolute_path) const {
    std::shared_lock lock(mutex_);
    if (root_.empty()) {
        throw ApiError(ErrorCode::WorkspaceNotOpen, "No workspace is open");
    }
    const auto relative_path = fsutil::relative_to(root_, absolute_path);
    if (!relative_path.has_value()) {
        throw ApiError(ErrorCode::PathOutsideWorkspace,
                       "Path is outside the workspace: " + absolute_path.string());
    }
    return *relative_path;
}

}  // namespace anvil
