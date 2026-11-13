#include <gtest/gtest.h>

#include <unistd.h>

#include <string>

#include "anvil/util/process.hpp"

namespace {

TEST(LineReader, ReadsBufferedLines) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    const std::string payload = "alpha\nbeta\n";
    ASSERT_EQ(::write(fds[1], payload.data(), payload.size()),
              static_cast<ssize_t>(payload.size()));
    ::close(fds[1]);
    anvil::FdLineReader reader(fds[0]);
    std::string line;
    bool too_large = false;
    std::string error;
    ASSERT_TRUE(reader.read_line(&line, 1024, &too_large, &error));
    EXPECT_EQ(line, "alpha");
    ASSERT_TRUE(reader.read_line(&line, 1024, &too_large, &error));
    EXPECT_EQ(line, "beta");
    EXPECT_FALSE(reader.read_line(&line, 1024, &too_large, &error));
    ::close(fds[0]);
}

TEST(LineReader, ReturnsPartialFinalLineAndEnforcesLimit) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    const std::string payload = "partial";
    ASSERT_EQ(::write(fds[1], payload.data(), payload.size()),
              static_cast<ssize_t>(payload.size()));
    ::close(fds[1]);
    anvil::FdLineReader reader(fds[0]);
    std::string line;
    bool too_large = false;
    std::string error;
    ASSERT_TRUE(reader.read_line(&line, 1024, &too_large, &error));
    EXPECT_EQ(line, "partial");
    ::close(fds[0]);

    int second[2];
    ASSERT_EQ(::pipe(second), 0);
    const std::string long_line(100, 'x');
    ::write(second[1], long_line.data(), long_line.size());
    ::close(second[1]);
    anvil::FdLineReader limited(second[0]);
    EXPECT_FALSE(limited.read_line(&line, 10, &too_large, &error));
    EXPECT_TRUE(too_large);
    ::close(second[0]);
}

}  // namespace
