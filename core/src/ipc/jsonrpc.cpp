#include "anvil/ipc/jsonrpc.hpp"

#include <cctype>
#include <cstdint>

#include "anvil/util/strings.hpp"

namespace anvil::ipc {

bool json_depth_within(std::string_view text, std::size_t max_depth) {
    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (const char c : text) {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '{' || c == '[') {
            ++depth;
            if (depth > max_depth) return false;
        } else if (c == '}' || c == ']') {
            if (depth == 0) return false;
            --depth;
        }
    }
    return depth == 0 && !in_string && !escaped;
}

std::string Request::id_key() const {
    if (id.is_number_integer()) {
        return "n:" + std::to_string(id.get<std::int64_t>());
    }
    if (id.is_string()) {
        return "s:" + id.get<std::string>();
    }
    return "n:0";
}

Request parse_request(std::string_view line) {
    if (line.size() > kMaxJsonLineBytes) {
        throw ApiError(ErrorCode::PayloadTooLarge, "JSON-RPC message exceeds the size limit");
    }
    if (!json_depth_within(line, kMaxJsonDepth)) {
        throw ApiError(ErrorCode::ParseError, "JSON nesting depth or structure is invalid");
    }

    const Json value = Json::parse(line.begin(), line.end(), nullptr, false);
    if (value.is_discarded()) {
        throw ApiError(ErrorCode::ParseError, "Malformed JSON");
    }
    if (!value.is_object()) {
        throw ApiError(ErrorCode::InvalidRequest, "JSON-RPC message must be an object");
    }

    auto version = value.find("jsonrpc");
    if (version == value.end() || !version->is_string() || *version != "2.0") {
        throw ApiError(ErrorCode::InvalidRequest, "Missing or unsupported jsonrpc version");
    }

    auto method = value.find("method");
    if (method == value.end() || !method->is_string()) {
        throw ApiError(ErrorCode::InvalidRequest, "Missing method");
    }
    const std::string method_name = method->get<std::string>();
    if (method_name.empty() || method_name.size() > 128) {
        throw ApiError(ErrorCode::InvalidRequest, "Invalid method name");
    }

    Request request;
    request.method = method_name;
    auto id = value.find("id");
    if (id == value.end()) {
        request.is_notification = true;
        request.id = Json();
    } else {
        if (!id->is_number_integer() && !id->is_string()) {
            throw ApiError(ErrorCode::InvalidRequest, "id must be an integer or a string");
        }
        request.id = *id;
    }

    auto api_version = value.find("apiVersion");
    if (api_version != value.end()) {
        if (!api_version->is_number_integer()) {
            throw ApiError(ErrorCode::InvalidRequest, "apiVersion must be an integer");
        }
        const auto number = api_version->get<std::int64_t>();
        if (number < 0 || number > 1'000'000) {
            throw ApiError(ErrorCode::InvalidRequest, "apiVersion is out of range");
        }
        request.api_version = static_cast<int>(number);
    }

    auto params = value.find("params");
    if (params != value.end()) {
        if (!params->is_object()) {
            throw ApiError(ErrorCode::InvalidRequest, "params must be an object");
        }
        request.params = *params;
    }
    return request;
}

std::optional<Response> parse_response(const Json& value) {
    if (!value.is_object()) return std::nullopt;
    auto id = value.find("id");
    if (id == value.end()) return std::nullopt;
    Response response;
    response.id = *id;
    auto result = value.find("result");
    auto error = value.find("error");
    if (error != value.end()) {
        response.has_error = true;
        response.error = *error;
    } else if (result != value.end()) {
        response.result = *result;
    } else {
        return std::nullopt;
    }
    return response;
}

Json make_result(const Json& id, Json result) {
    Json message{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
    return message;
}

Json make_error(const Json& id, ErrorCode code, std::string_view message, Json data) {
    return make_error(id, static_cast<int>(code), message, std::move(data));
}

Json make_error(const Json& id, int code, std::string_view message, Json data) {
    Json error{{"code", code}, {"message", std::string(message)}};
    if (!data.is_null()) error["data"] = std::move(data);
    return Json{{"jsonrpc", "2.0"}, {"id", id}, {"error", std::move(error)}};
}

Json make_notification(std::string_view method, Json params) {
    return Json{{"jsonrpc", "2.0"}, {"method", std::string(method)}, {"params", std::move(params)}};
}

std::string serialize_line(const Json& value) { return value.dump() + "\n"; }

}  // namespace anvil::ipc
