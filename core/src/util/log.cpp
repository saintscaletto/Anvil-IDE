#include "anvil/util/log.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <regex>
#include <string>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include "anvil/util/strings.hpp"
#include "anvil/version.hpp"

namespace anvil::logging {
namespace {

constexpr std::size_t kMaxLogBytes = 10U * 1024U * 1024U;
constexpr int kMaxLogFiles = 5;
constexpr std::size_t kMaxDetailsBytes = 16U * 1024U;

std::mutex g_security_mutex;
std::filesystem::path g_config_dir;
std::filesystem::path g_security_path;
bool g_initialized = false;

const std::regex& authorization_regex() {
    static const std::regex re(R"((authorization\s*:\s*)([^\r\n]+))",
                               std::regex::icase | std::regex::optimize);
    return re;
}

const std::regex& key_value_regex() {
    static const std::regex re(
        R"(((?:api[_-]?key|access[_-]?token|refresh[_-]?token|token|password|passwd|secret)\s*[=:]\s*)(["']?)([^"'\s,;]+))",
        std::regex::icase | std::regex::optimize);
    return re;
}

const std::regex& bearer_regex() {
    static const std::regex re(R"((bearer\s+)[A-Za-z0-9._~+/\-]+=*)",
                               std::regex::icase | std::regex::optimize);
    return re;
}

std::string iso8601_now() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds).count();
    const std::time_t time = std::chrono::system_clock::to_time_t(seconds);
    std::tm utc{};
    gmtime_r(&time, &utc);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min,
                  utc.tm_sec, static_cast<long long>(millis));
    return buffer;
}

bool is_sensitive_key(std::string_view key) {
    const std::string lower = strings::to_lower(key);
    return lower == "token" || lower == "access_token" || lower == "refresh_token" ||
           lower == "password" || lower == "passwd" || lower == "secret" ||
           lower == "api_key" || lower == "apikey" || lower == "authorization" ||
           lower == "cookie" || lower == "session" || lower == "private_key";
}

Json redact_json(Json value) {
    if (value.is_string()) {
        return redact(value.get<std::string>());
    }
    if (value.is_array()) {
        for (auto& item : value) item = redact_json(std::move(item));
        return value;
    }
    if (value.is_object()) {
        for (auto& [key, item] : value.items()) {
            if (is_sensitive_key(key)) {
                item = "[REDACTED]";
            } else if (key == "content" || key == "text") {
                item = "[CONTENT REDACTED]";
            } else {
                item = redact_json(std::move(item));
            }
        }
        return value;
    }
    return value;
}

void rotate_security_log() {
    std::error_code ec;
    const auto size = std::filesystem::file_size(g_security_path, ec);
    if (ec || size < kMaxLogBytes) return;

    for (int index = kMaxLogFiles - 1; index >= 1; --index) {
        const auto from = g_security_path.string() + "." + std::to_string(index);
        const auto to = g_security_path.string() + "." + std::to_string(index + 1);
        std::filesystem::rename(from, to, ec);
        ec.clear();
    }
    std::filesystem::rename(g_security_path, g_security_path.string() + ".1", ec);
}

void append_security_line(const std::string& line) {
    std::lock_guard lock(g_security_mutex);
    if (!g_initialized) return;
    rotate_security_log();
    const int fd = ::open(g_security_path.c_str(),
                          O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return;
    const char* data = line.data();
    std::size_t remaining = line.size();
    while (remaining > 0) {
        const ssize_t written = ::write(fd, data, remaining);
        if (written <= 0) break;
        data += written;
        remaining -= static_cast<std::size_t>(written);
    }
    ::close(fd);
}

}  // namespace

void initialize(const std::filesystem::path& config_dir, bool dev_mode) {
    g_config_dir = config_dir;
    std::error_code ec;
    std::filesystem::create_directories(config_dir / "logs", ec);
    std::filesystem::permissions(config_dir / "logs",
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    g_security_path = config_dir / "logs" / "security.log";

    try {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            (config_dir / "logs" / "anvil-core.log").string(), kMaxLogBytes, kMaxLogFiles);
        auto stderr_sink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
        stderr_sink->set_level(spdlog::level::warn);

        std::vector<spdlog::sink_ptr> sinks{file_sink, stderr_sink};
        auto logger = std::make_shared<spdlog::logger>("anvil", sinks.begin(), sinks.end());
        logger->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%l] [%t] %v");
        logger->set_level(dev_mode ? spdlog::level::debug : spdlog::level::info);
        logger->flush_on(spdlog::level::warn);
        spdlog::set_default_logger(logger);
        spdlog::flush_every(std::chrono::seconds(2));
        g_initialized = true;
    } catch (const spdlog::spdlog_ex& ex) {
        std::fprintf(stderr, "anvil-core: logging initialisation failed: %s\n", ex.what());
        g_initialized = false;
    }

    ::chmod(g_security_path.c_str(), 0600);
    security_event("core.logging_initialized",
                   Json{{"devMode", dev_mode},
                        {"coreVersion", std::string(anvil::kVersion)}});
}

void shutdown() {
    if (g_initialized) {
        spdlog::shutdown();
    }
    g_initialized = false;
}

void debug(std::string_view message) {
    if (g_initialized) spdlog::debug("[{}]", message);
}

void info(std::string_view message) {
    if (g_initialized) spdlog::info("[{}]", message);
}

void warn(std::string_view message) {
    if (g_initialized) spdlog::warn("[{}]", message);
}

void error(std::string_view message) {
    if (g_initialized) spdlog::error("[{}]", message);
}

std::string redact(std::string_view text) {
    std::string value(text);
    value = std::regex_replace(value, authorization_regex(), "$1[REDACTED]");
    value = std::regex_replace(value, bearer_regex(), "$1[REDACTED]");
    value = std::regex_replace(value, key_value_regex(), "$1$2[REDACTED]");
    return value;
}

void security_event(std::string_view event, const Json& details) {
    if (!g_initialized) return;
    Json safe = redact_json(details);
    std::string serialized = safe.dump();
    if (serialized.size() > kMaxDetailsBytes) {
        serialized = R"({"truncated":true})";
    }
    Json line{{"ts", iso8601_now()},
              {"event", std::string(event)},
              {"pid", static_cast<long long>(::getpid())},
              {"details", Json::parse(serialized, nullptr, false)}};
    if (line["details"].is_discarded()) {
        line["details"] = Json::object();
    }
    append_security_line(line.dump() + "\n");
}

std::filesystem::path log_directory() { return g_config_dir / "logs"; }

std::filesystem::path security_log_path() { return g_security_path; }

}  // namespace anvil::logging
