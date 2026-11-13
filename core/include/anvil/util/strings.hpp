#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace anvil::strings {

std::string to_lower(std::string_view value);
std::string trim(std::string_view value);
bool starts_with(std::string_view value, std::string_view prefix);
bool ends_with(std::string_view value, std::string_view suffix);
std::vector<std::string> split(std::string_view value, char separator);
std::string join(const std::vector<std::string>& parts, std::string_view separator);

// True when the byte sequence is well-formed UTF-8 (no overlong forms, no
// surrogates, nothing above U+10FFFF). Files that fail this check are never
// sent through JSON.
bool is_valid_utf8(std::string_view value);

// LSP file URIs use percent-encoded UTF-8 paths. These helpers are deliberately
// strict: a malformed escape or an authority other than an empty host/localhost
// is rejected rather than guessed at.
std::string path_to_file_uri(const std::string& absolute_path);
std::optional<std::string> file_uri_to_path(std::string_view uri);

std::string random_hex(std::size_t bytes);
std::string hex_encode(const unsigned char* data, std::size_t size);
std::uint64_t fnv1a64(std::string_view value);

}  // namespace anvil::strings
