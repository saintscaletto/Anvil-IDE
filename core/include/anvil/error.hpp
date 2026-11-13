#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace anvil {

using Json = nlohmann::json;

// JSON-RPC 2.0 reserved codes plus Anvil-specific codes. The numeric values are
// part of the frontend contract; do not renumber them.
enum class ErrorCode : int {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    WorkspaceNotOpen = -32001,
    PathOutsideWorkspace = -32002,
    NotFound = -32003,
    AlreadyExists = -32004,
    PermissionDenied = -32005,
    PayloadTooLarge = -32006,
    Unsupported = -32007,
    Conflict = -32008,
    SandboxUnavailable = -32009,
    LspUnavailable = -32010,
    Cancelled = -32011,
    InvalidUtf8 = -32012,
    ConfirmationRequired = -32013,
    KeyringUnavailable = -32014,
    Overloaded = -32015,
};

class ApiError : public std::runtime_error {
public:
    ApiError(ErrorCode code, std::string message, Json data = Json())
        : std::runtime_error(std::move(message)), code_(code), data_(std::move(data)) {}

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const Json& data() const noexcept { return data_; }

private:
    ErrorCode code_;
    Json data_;
};

inline ApiError invalid_params(std::string message) {
    return ApiError(ErrorCode::InvalidParams, std::move(message));
}

inline ApiError internal_error(std::string message) {
    return ApiError(ErrorCode::InternalError, std::move(message));
}

}  // namespace anvil
