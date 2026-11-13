#pragma once

#include <sys/types.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "anvil/error.hpp"

namespace anvil::fsutil {

namespace fs = std::filesystem;

struct FileStat {
    std::uint64_t size = 0;
    std::int64_t mtime_ms = 0;
    mode_t mode = 0;
    bool is_regular = false;
    bool is_directory = false;
    bool is_symlink = false;
};

struct DirEntry {
    std::string name;
    bool is_directory = false;
    bool is_symlink = false;
    std::uint64_t size = 0;
    std::int64_t mtime_ms = 0;
};

FileStat stat_path(const fs::path& path, bool follow_symlinks = true);

// Reads a regular UTF-8 text file. Rejects NUL bytes, invalid UTF-8, files over
// max_bytes and non-regular files. Throws ApiError with a stable code.
std::string read_text_file(const fs::path& path, std::size_t max_bytes);

// Writes atomically: temp file in the destination directory, fsync, rename,
// fsync of the directory. When expected_mtime_ms is supplied the write is
// rejected if the on-disk file changed since it was read.
void write_text_file_atomic(const fs::path& path, const std::string& content,
                            std::optional<std::int64_t> expected_mtime_ms);

std::vector<DirEntry> list_directory(const fs::path& path, std::size_t max_entries,
                                     bool* truncated);

void ensure_directory(const fs::path& path, mode_t mode);
void ensure_private_directory(const fs::path& path);

std::optional<Json> try_read_json_file(const fs::path& path, std::size_t max_bytes,
                                       std::string* error_message);

void write_json_file_atomic(const fs::path& path, const Json& value);

bool is_within(const fs::path& root, const fs::path& candidate);
std::optional<std::string> relative_to(const fs::path& root, const fs::path& candidate);

}  // namespace anvil::fsutil
