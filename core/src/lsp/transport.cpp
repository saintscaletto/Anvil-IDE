#include "anvil/lsp/transport.hpp"

#include <unistd.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <cstring>

#include "anvil/ipc/jsonrpc.hpp"
#include "anvil/util/strings.hpp"

namespace anvil::lsp {
namespace {

constexpr std::size_t kMaxHeaderBytes = 8U * 1024U;
constexpr std::size_t kMaxContentBytes = 32U * 1024U * 1024U;

std::string trim_cr(std::string value) {
    if (!value.empty() && value.back() == '\r') value.pop_back();
    return value;
}

}  // namespace

Transport::Transport(int read_fd, int write_fd) : read_fd_(read_fd), write_fd_(write_fd) {}

Transport::~Transport() {
    close_read();
    close_write();
}

void Transport::close_read() {
    if (read_fd_ >= 0) {
        ::close(read_fd_);
        read_fd_ = -1;
    }
}

void Transport::close_write() {
    if (write_fd_ >= 0) {
        ::close(write_fd_);
        write_fd_ = -1;
    }
}

bool Transport::fill_buffer(std::string* error_message) {
    std::array<char, 16U * 1024U> chunk{};
    const ssize_t count = ::read(read_fd_, chunk.data(), chunk.size());
    if (count < 0) {
        if (errno == EINTR) return fill_buffer(error_message);
        if (error_message != nullptr) *error_message = std::strerror(errno);
        return false;
    }
    if (count == 0) {
        if (error_message != nullptr) *error_message = "end of stream";
        return false;
    }
    buffer_.append(chunk.data(), static_cast<std::size_t>(count));
    if (buffer_.size() > kMaxHeaderBytes + kMaxContentBytes) {
        if (error_message != nullptr) *error_message = "LSP message exceeds the size limit";
        return false;
    }
    return true;
}

bool Transport::read_message(Json* message, std::string* error_message) {
    if (read_fd_ < 0) {
        if (error_message != nullptr) *error_message = "stream is closed";
        return false;
    }

    std::size_t header_end = std::string::npos;
    while (true) {
        header_end = buffer_.find("\r\n\r\n");
        if (header_end != std::string::npos) break;
        header_end = buffer_.find("\n\n");
        if (header_end != std::string::npos) break;
        if (buffer_.size() > kMaxHeaderBytes) {
            if (error_message != nullptr) *error_message = "LSP header exceeds the size limit";
            return false;
        }
        if (!fill_buffer(error_message)) return false;
    }

    const bool using_crlf = buffer_.compare(header_end, 4, "\r\n\r\n") == 0;
    const std::size_t delimiter_size = using_crlf ? 4 : 2;
    const std::string header_block = buffer_.substr(0, header_end);
    std::size_t content_length = 0;
    bool have_length = false;
    for (const auto& line : strings::split(header_block, '\n')) {
        const std::string header = trim_cr(line);
        const std::size_t colon = header.find(':');
        if (colon == std::string::npos) continue;
        const std::string name = strings::to_lower(strings::trim(header.substr(0, colon)));
        const std::string value = strings::trim(header.substr(colon + 1));
        if (name == "content-length") {
            const char* begin = value.data();
            const char* end = value.data() + value.size();
            const auto [pointer, ec] = std::from_chars(begin, end, content_length);
            if (ec != std::errc() || pointer != end) {
                if (error_message != nullptr) *error_message = "Invalid Content-Length";
                return false;
            }
            have_length = true;
        }
    }
    if (!have_length) {
        if (error_message != nullptr) *error_message = "Missing Content-Length";
        return false;
    }
    if (content_length > kMaxContentBytes) {
        if (error_message != nullptr) *error_message = "LSP body exceeds the size limit";
        return false;
    }

    const std::size_t body_start = header_end + delimiter_size;
    while (buffer_.size() < body_start + content_length) {
        if (!fill_buffer(error_message)) return false;
    }
    const std::string body = buffer_.substr(body_start, content_length);
    buffer_.erase(0, body_start + content_length);

    if (body.empty() || !ipc::json_depth_within(body, ipc::kMaxJsonDepth)) {
        if (error_message != nullptr) *error_message = "Invalid LSP JSON body";
        return false;
    }
    const Json parsed = Json::parse(body, nullptr, false);
    if (parsed.is_discarded()) {
        if (error_message != nullptr) *error_message = "Malformed LSP JSON body";
        return false;
    }
    *message = parsed;
    return true;
}

bool Transport::write_message(const Json& message) {
    if (write_fd_ < 0) return false;
    const std::string body = message.dump();
    if (body.size() > kMaxContentBytes) return false;
    const std::string header =
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    std::lock_guard lock(write_mutex_);
    const std::string payload = header + body;
    std::size_t written = 0;
    while (written < payload.size()) {
        const ssize_t count = ::write(write_fd_, payload.data() + written,
                                      payload.size() - written);
        if (count < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (count == 0) return false;
        written += static_cast<std::size_t>(count);
    }
    return true;
}

}  // namespace anvil::lsp
