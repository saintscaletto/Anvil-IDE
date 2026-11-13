#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "anvil/error.hpp"

namespace anvil::logging {

// Prepares rotating application logs and the append-only JSONL security log.
// Must be called once before any other logging function.
void initialize(const std::filesystem::path& config_dir, bool dev_mode);
void shutdown();

void debug(std::string_view message);
void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);

// Writes an audit event to <config>/logs/security.log. Details are redacted and
// truncated; this function never throws.
void security_event(std::string_view event, const Json& details = Json::object());

// Masks Authorization headers, bearer tokens and key=value style secrets.
std::string redact(std::string_view text);

std::filesystem::path log_directory();
std::filesystem::path security_log_path();

}  // namespace anvil::logging
