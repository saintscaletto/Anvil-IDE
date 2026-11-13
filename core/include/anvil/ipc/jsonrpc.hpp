#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "anvil/error.hpp"

namespace anvil::ipc {

inline constexpr std::size_t kMaxJsonLineBytes = 32U * 1024U * 1024U;
inline constexpr std::size_t kMaxJsonDepth = 64;

struct Request {
    Json id;
    std::string method;
    Json params = Json::object();
    bool is_notification = false;
    int api_version = 0;

    [[nodiscard]] std::string id_key() const;
};

struct Response {
    Json id;
    Json result;
    bool has_error = false;
    Json error;

    [[nodiscard]] bool is_error() const noexcept { return has_error; }
};

// Rejects deeply nested input before handing it to the JSON parser, which has
// no depth limit of its own. Brackets inside strings are ignored.
bool json_depth_within(std::string_view text, std::size_t max_depth);

// Parses one newline-delimited JSON-RPC request. Throws ApiError with
// ParseError/InvalidRequest. Oversized input must be rejected by the caller
// before calling this function.
Request parse_request(std::string_view line);

// Parses an LSP response object. Returns nullopt for notifications and other
// non-response messages.
std::optional<Response> parse_response(const Json& value);

Json make_result(const Json& id, Json result);
Json make_error(const Json& id, ErrorCode code, std::string_view message, Json data = Json());
Json make_error(const Json& id, int code, std::string_view message, Json data = Json());
Json make_notification(std::string_view method, Json params);

std::string serialize_line(const Json& value);

}  // namespace anvil::ipc
