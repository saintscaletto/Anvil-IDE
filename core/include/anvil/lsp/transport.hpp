#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>

#include "anvil/error.hpp"

namespace anvil::lsp {

// LSP base protocol framing over a pipe: Content-Length headers, CRLF, JSON
// body. Reads are blocking and intended for a dedicated reader thread.
class Transport {
public:
    Transport(int read_fd, int write_fd);
    ~Transport();

    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    bool read_message(Json* message, std::string* error_message);
    bool write_message(const Json& message);
    void close_read();
    void close_write();

    [[nodiscard]] bool read_closed() const noexcept { return read_fd_ < 0; }

private:
    bool fill_buffer(std::string* error_message);

    int read_fd_;
    int write_fd_;
    std::string buffer_;
    std::mutex write_mutex_;
};

}  // namespace anvil::lsp
