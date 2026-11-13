#include <gtest/gtest.h>

#include <unistd.h>

#include <string>

#include "anvil/lsp/transport.hpp"

namespace {

struct PipePair {
    int read_fd = -1;
    int write_fd = -1;
    PipePair() {
        int fds[2];
        if (::pipe(fds) == 0) {
            read_fd = fds[0];
            write_fd = fds[1];
        }
    }
    ~PipePair() {
        if (read_fd >= 0) ::close(read_fd);
        if (write_fd >= 0) ::close(write_fd);
    }
};

std::string frame(const std::string& body) {
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

void write_all(int fd, const std::string& data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written = ::write(fd, data.data() + offset, data.size() - offset);
        ASSERT_GT(written, 0);
        offset += static_cast<std::size_t>(written);
    }
}

TEST(LspTransport, ReadsFramedMessagesAcrossWrites) {
    PipePair input;
    PipePair output;
    anvil::lsp::Transport transport(input.read_fd, output.write_fd);
    input.read_fd = -1;
    output.write_fd = -1;

    const std::string payload = R"({"jsonrpc":"2.0","id":1,"result":{"ok":true}})";
    const std::string framed = frame(payload);
    const std::size_t split = framed.size() / 2;
    write_all(input.write_fd, framed.substr(0, split));
    write_all(input.write_fd, framed.substr(split));
    ::close(input.write_fd);
    input.write_fd = -1;

    anvil::Json message;
    std::string error;
    ASSERT_TRUE(transport.read_message(&message, &error)) << error;
    EXPECT_EQ(message["id"], 1);
    EXPECT_TRUE(message["result"]["ok"]);
}

TEST(LspTransport, WritesValidFrames) {
    PipePair input;
    PipePair output;
    anvil::lsp::Transport transport(input.read_fd, output.write_fd);
    input.read_fd = -1;
    output.write_fd = -1;
    ASSERT_TRUE(transport.write_message(
        anvil::Json{{"jsonrpc", "2.0"}, {"id", 2}, {"result", anvil::Json::object()}}));
    transport.close_write();

    std::string raw;
    char buffer[256];
    ssize_t count = 0;
    while ((count = ::read(output.read_fd, buffer, sizeof(buffer))) > 0) {
        raw.append(buffer, static_cast<std::size_t>(count));
    }
    EXPECT_NE(raw.find("Content-Length: "), std::string::npos);
    EXPECT_NE(raw.find("\r\n\r\n"), std::string::npos);
    EXPECT_NE(raw.find("\"id\":2"), std::string::npos);
}

TEST(LspTransport, RejectsMissingOrInvalidContentLength) {
    {
        PipePair input;
        PipePair output;
        anvil::lsp::Transport transport(input.read_fd, output.write_fd);
        input.read_fd = -1;
        output.write_fd = -1;
        const std::string body = "{\"jsonrpc\":\"2.0\"}\n\n";
        write_all(input.write_fd, body);
        ::close(input.write_fd);
        input.write_fd = -1;
        anvil::Json message;
        std::string error;
        EXPECT_FALSE(transport.read_message(&message, &error));
        EXPECT_FALSE(error.empty());
    }
    {
        PipePair input;
        PipePair output;
        anvil::lsp::Transport transport(input.read_fd, output.write_fd);
        input.read_fd = -1;
        output.write_fd = -1;
        write_all(input.write_fd, "Content-Length: nope\r\n\r\n{}");
        ::close(input.write_fd);
        input.write_fd = -1;
        anvil::Json message;
        std::string error;
        EXPECT_FALSE(transport.read_message(&message, &error));
    }
}

}  // namespace
