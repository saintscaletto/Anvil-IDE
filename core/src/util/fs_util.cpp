#include "anvil/util/fs_util.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <system_error>

#include "anvil/util/log.hpp"
#include "anvil/util/strings.hpp"

namespace anvil::fsutil {
namespace {

constexpr std::size_t kReadChunk = 64U * 1024U;

std::int64_t to_mtime_ms(const struct stat& info) {
    return static_cast<std::int64_t>(info.st_mtim.tv_sec) * 1000 +
           static_cast<std::int64_t>(info.st_mtim.tv_nsec) / 1'000'000;
}

[[noreturn]] void throw_errno(ErrorCode code, const std::string& context,
                              const fs::path& path) {
    const int saved = errno;
    throw ApiError(code, context + ": " + path.string() + ": " + std::strerror(saved));
}

void write_all(int fd, const char* data, std::size_t size) {
    while (size > 0) {
        const ssize_t written = ::write(fd, data, size);
        if (written < 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "write");
        }
        if (written == 0) throw std::system_error(EIO, std::generic_category(), "write");
        data += written;
        size -= static_cast<std::size_t>(written);
    }
}

void fsync_directory(const fs::path& directory) {
    const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return;
    ::fsync(fd);
    ::close(fd);
}

}  // namespace

FileStat stat_path(const fs::path& path, bool follow_symlinks) {
    struct stat info {};
    const int result = follow_symlinks ? ::stat(path.c_str(), &info)
                                       : ::lstat(path.c_str(), &info);
    if (result != 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            throw ApiError(ErrorCode::NotFound, "Path not found: " + path.string());
        }
        throw_errno(ErrorCode::PermissionDenied, "Cannot stat path", path);
    }
    FileStat out;
    out.size = static_cast<std::uint64_t>(info.st_size);
    out.mtime_ms = to_mtime_ms(info);
    out.mode = info.st_mode;
    out.is_regular = S_ISREG(info.st_mode);
    out.is_directory = S_ISDIR(info.st_mode);
    out.is_symlink = S_ISLNK(info.st_mode);
    return out;
}

std::string read_text_file(const fs::path& path, std::size_t max_bytes) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        if (errno == ELOOP) {
            throw ApiError(ErrorCode::PermissionDenied, "Symlink rejected: " + path.string());
        }
        if (errno == ENOENT) {
            throw ApiError(ErrorCode::NotFound, "File not found: " + path.string());
        }
        throw_errno(ErrorCode::PermissionDenied, "Cannot open file", path);
    }

    struct stat info {};
    if (::fstat(fd, &info) != 0) {
        ::close(fd);
        throw_errno(ErrorCode::PermissionDenied, "Cannot stat file", path);
    }
    if (!S_ISREG(info.st_mode)) {
        ::close(fd);
        throw ApiError(ErrorCode::Unsupported, "Not a regular file: " + path.string());
    }
    if (info.st_size < 0 || static_cast<std::uint64_t>(info.st_size) > max_bytes) {
        ::close(fd);
        throw ApiError(ErrorCode::PayloadTooLarge,
                       "File exceeds the " + std::to_string(max_bytes) + " byte limit: " +
                           path.string());
    }

    std::string content;
    content.reserve(static_cast<std::size_t>(info.st_size));
    std::array<char, kReadChunk> buffer{};
    while (true) {
        const ssize_t count = ::read(fd, buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            throw_errno(ErrorCode::PermissionDenied, "Cannot read file", path);
        }
        if (count == 0) break;
        content.append(buffer.data(), static_cast<std::size_t>(count));
        if (content.size() > max_bytes) {
            ::close(fd);
            throw ApiError(ErrorCode::PayloadTooLarge,
                           "File exceeds the configured size limit: " + path.string());
        }
    }
    ::close(fd);

    if (content.find('\0') != std::string::npos) {
        throw ApiError(ErrorCode::Unsupported,
                       "Binary file (NUL byte) cannot be opened as text: " + path.string());
    }
    if (!strings::is_valid_utf8(content)) {
        throw ApiError(ErrorCode::InvalidUtf8,
                       "File is not valid UTF-8: " + path.string());
    }
    return content;
}

void write_text_file_atomic(const fs::path& path, const std::string& content,
                            std::optional<std::int64_t> expected_mtime_ms) {
    if (content.size() > 16U * 1024U * 1024U) {
        throw ApiError(ErrorCode::PayloadTooLarge, "Content exceeds the 16 MiB limit");
    }
    if (content.find('\0') != std::string::npos || !strings::is_valid_utf8(content)) {
        throw ApiError(ErrorCode::InvalidUtf8, "Refusing to write non-UTF-8 content");
    }

    struct stat existing {};
    const bool exists = ::stat(path.c_str(), &existing) == 0;
    if (expected_mtime_ms.has_value()) {
        if (!exists) {
            throw ApiError(ErrorCode::Conflict,
                           "File was removed on disk; reload before saving: " + path.string());
        }
        const std::int64_t current = to_mtime_ms(existing);
        if (current != *expected_mtime_ms) {
            throw ApiError(ErrorCode::Conflict,
                           "File changed on disk; reload before saving: " + path.string(),
                           Json{{"currentMtime", current},
                                {"expectedMtime", *expected_mtime_ms}});
        }
    }
    if (exists && !S_ISREG(existing.st_mode)) {
        throw ApiError(ErrorCode::Unsupported, "Target is not a regular file: " + path.string());
    }

    const fs::path directory = path.parent_path();
    if (!fs::exists(directory)) {
        throw ApiError(ErrorCode::NotFound, "Parent directory does not exist: " +
                                                directory.string());
    }

    const std::string temp_name =
        "." + path.filename().string() + ".anvil-tmp-" + std::to_string(::getpid()) + "-" +
        strings::random_hex(6);
    const fs::path temp_path = directory / temp_name;

    const mode_t mode = exists ? (existing.st_mode & 0777) : 0644;
    const int fd = ::open(temp_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
    if (fd < 0) {
        throw_errno(ErrorCode::PermissionDenied, "Cannot create temporary file in", directory);
    }

    try {
        write_all(fd, content.data(), content.size());
        if (::fsync(fd) != 0) {
            throw std::system_error(errno, std::generic_category(), "fsync");
        }
        ::close(fd);

        if (expected_mtime_ms.has_value()) {
            struct stat current {};
            if (::stat(path.c_str(), &current) != 0 ||
                to_mtime_ms(current) != *expected_mtime_ms) {
                ::unlink(temp_path.c_str());
                throw ApiError(ErrorCode::Conflict,
                               "File changed on disk while saving: " + path.string());
            }
        }
        if (::rename(temp_path.c_str(), path.c_str()) != 0) {
            throw std::system_error(errno, std::generic_category(), "rename");
        }
        fsync_directory(directory);
    } catch (...) {
        ::unlink(temp_path.c_str());
        throw;
    }
}

std::vector<DirEntry> list_directory(const fs::path& path, std::size_t max_entries,
                                     bool* truncated) {
    std::vector<DirEntry> entries;
    std::error_code ec;
    fs::directory_iterator iterator(path, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        throw ApiError(ErrorCode::PermissionDenied,
                       "Cannot list directory: " + path.string() + ": " + ec.message());
    }

    for (const auto& entry : iterator) {
        if (entries.size() >= max_entries) {
            if (truncated != nullptr) *truncated = true;
            break;
        }
        std::error_code entry_ec;
        const auto link_status = entry.symlink_status(entry_ec);
        if (entry_ec) continue;
        DirEntry item;
        item.name = entry.path().filename().string();
        item.is_symlink = fs::is_symlink(link_status);
        item.is_directory = fs::is_directory(link_status);
        if (!item.is_symlink) {
            const auto file_status = entry.status(entry_ec);
            if (!entry_ec) item.is_directory = fs::is_directory(file_status);
            if (!entry_ec && file_status.type() == fs::file_type::regular) {
                struct stat info {};
                if (::lstat(entry.path().c_str(), &info) == 0) {
                    item.size = static_cast<std::uint64_t>(info.st_size);
                    item.mtime_ms = to_mtime_ms(info);
                }
            }
        }
        entries.push_back(std::move(item));
    }

    std::sort(entries.begin(), entries.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.is_directory != b.is_directory) return a.is_directory;
        return strings::to_lower(a.name) < strings::to_lower(b.name);
    });
    return entries;
}

void ensure_directory(const fs::path& path, mode_t mode) {
    std::error_code ec;
    const bool existed = fs::exists(path, ec);
    fs::create_directories(path, ec);
    if (ec) {
        throw ApiError(ErrorCode::PermissionDenied,
                       "Cannot create directory " + path.string() + ": " + ec.message());
    }
    if (!existed) ::chmod(path.c_str(), mode);
}

void ensure_private_directory(const fs::path& path) {
    ensure_directory(path, 0700);
}

std::optional<Json> try_read_json_file(const fs::path& path, std::size_t max_bytes,
                                       std::string* error_message) {
    std::string content;
    try {
        content = read_text_file(path, max_bytes);
    } catch (const ApiError& error) {
        if (error_message != nullptr) *error_message = error.what();
        return std::nullopt;
    }
    const Json value = Json::parse(content, nullptr, false);
    if (value.is_discarded()) {
        if (error_message != nullptr) *error_message = "Invalid JSON";
        return std::nullopt;
    }
    return value;
}

void write_json_file_atomic(const fs::path& path, const Json& value) {
    write_text_file_atomic(path, value.dump(2) + "\n", std::nullopt);
    ::chmod(path.c_str(), 0600);
}

bool is_within(const fs::path& root, const fs::path& candidate) {
    auto root_it = root.begin();
    auto candidate_it = candidate.begin();
    for (; root_it != root.end(); ++root_it, ++candidate_it) {
        if (candidate_it == candidate.end() || *root_it != *candidate_it) return false;
    }
    return true;
}

std::optional<std::string> relative_to(const fs::path& root, const fs::path& candidate) {
    if (!is_within(root, candidate)) return std::nullopt;
    std::error_code ec;
    const fs::path relative = fs::relative(candidate, root, ec);
    if (ec) return std::nullopt;
    for (const auto& component : relative) {
        if (component == "..") return std::nullopt;
    }
    std::string result = relative.generic_string();
    if (result == ".") result.clear();
    return result;
}

}  // namespace anvil::fsutil
