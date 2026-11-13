#include "anvil/util/strings.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <random>

namespace anvil::strings {
namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool is_unreserved(unsigned char c) {
    return std::isalnum(c) != 0 || c == '-' || c == '.' || c == '_' || c == '~';
}

}  // namespace

std::string to_lower(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

std::string trim(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) --end;
    return std::string(value.substr(begin, end - begin));
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
}

std::vector<std::string> split(std::string_view value, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= value.size(); ++i) {
        if (i == value.size() || value[i] == separator) {
            parts.emplace_back(value.substr(start, i - start));
            start = i + 1;
        }
    }
    return parts;
}

std::string join(const std::vector<std::string>& parts, std::string_view separator) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out.append(separator);
        out.append(parts[i]);
    }
    return out;
}

bool is_valid_utf8(std::string_view value) {
    std::size_t i = 0;
    while (i < value.size()) {
        const auto lead = static_cast<unsigned char>(value[i]);
        if (lead < 0x80) {
            ++i;
            continue;
        }

        std::size_t length = 0;
        std::uint32_t code_point = 0;
        if ((lead & 0xE0) == 0xC0) {
            length = 2;
            code_point = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            code_point = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            code_point = lead & 0x07;
        } else {
            return false;
        }
        if (i + length > value.size()) return false;
        for (std::size_t j = 1; j < length; ++j) {
            const auto byte = static_cast<unsigned char>(value[i + j]);
            if ((byte & 0xC0) != 0x80) return false;
            code_point = (code_point << 6) | (byte & 0x3F);
        }
        if ((length == 2 && code_point < 0x80) || (length == 3 && code_point < 0x800) ||
            (length == 4 && code_point < 0x10000)) {
            return false;  // overlong encoding
        }
        if (code_point > 0x10FFFF || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
            return false;
        }
        i += length;
    }
    return true;
}

std::string hex_encode(const unsigned char* data, std::size_t size) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(kHex[data[i] >> 4]);
        out.push_back(kHex[data[i] & 0x0F]);
    }
    return out;
}

std::string random_hex(std::size_t bytes) {
    std::vector<unsigned char> buffer(bytes);
    std::random_device device;
    for (auto& byte : buffer) {
        byte = static_cast<unsigned char>(device() & 0xFF);
    }
    return hex_encode(buffer.data(), buffer.size());
}

std::uint64_t fnv1a64(std::string_view value) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char c : value) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string path_to_file_uri(const std::string& absolute_path) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(absolute_path.size() + 16);
    for (const unsigned char c : absolute_path) {
        if (is_unreserved(c) || c == '/') {
            encoded.push_back(static_cast<char>(c));
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[c >> 4]);
            encoded.push_back(kHex[c & 0x0F]);
        }
    }
    if (encoded.empty() || encoded.front() != '/') {
        encoded.insert(encoded.begin(), '/');
    }
    return "file://" + encoded;
}

std::optional<std::string> file_uri_to_path(std::string_view uri) {
    constexpr std::string_view kScheme = "file://";
    if (!starts_with(uri, kScheme)) return std::nullopt;

    std::string_view rest = uri.substr(kScheme.size());
    if (!rest.empty() && rest.front() != '/') {
        // Only an empty authority or "localhost" is accepted.
        const std::size_t slash = rest.find('/');
        if (slash == std::string_view::npos) return std::nullopt;
        const std::string_view authority = rest.substr(0, slash);
        if (authority != "localhost") return std::nullopt;
        rest = rest.substr(slash);
    }

    std::string decoded;
    decoded.reserve(rest.size());
    for (std::size_t i = 0; i < rest.size(); ++i) {
        if (rest[i] == '%') {
            if (i + 2 >= rest.size()) return std::nullopt;
            const int high = hex_value(rest[i + 1]);
            const int low = hex_value(rest[i + 2]);
            if (high < 0 || low < 0) return std::nullopt;
            const char c = static_cast<char>((high << 4) | low);
            if (c == '\0') return std::nullopt;
            decoded.push_back(c);
            i += 2;
        } else {
            decoded.push_back(rest[i]);
        }
    }
    if (decoded.empty() || decoded.front() != '/') return std::nullopt;
    if (!is_valid_utf8(decoded)) return std::nullopt;
    return decoded;
}

}  // namespace anvil::strings
