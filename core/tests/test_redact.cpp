#include <gtest/gtest.h>

#include <fstream>

#include "anvil/util/log.hpp"
#include "test_util.hpp"

namespace {

TEST(Redact, MasksSecrets) {
    const std::string input =
        "Authorization: Bearer abc.def.ghi\napi_key=super-secret\npassword: hunter2\n"
        "token=xyz\nnormal text";
    const std::string output = anvil::logging::redact(input);
    EXPECT_EQ(output.find("abc.def.ghi"), std::string::npos);
    EXPECT_EQ(output.find("super-secret"), std::string::npos);
    EXPECT_EQ(output.find("hunter2"), std::string::npos);
    EXPECT_EQ(output.find("xyz"), std::string::npos);
    EXPECT_NE(output.find("normal text"), std::string::npos);
    EXPECT_NE(output.find("[REDACTED]"), std::string::npos);
}

TEST(Redact, SecurityLogRedactsDetails) {
    anvil::test::TempDir temp;
    anvil::logging::initialize(temp.path(), true);
    anvil::logging::security_event(
        "test.event",
        anvil::Json{{"token", "super-secret-token"},
                    {"text", "source code"},
                    {"nested", {{"password", "hunter2"}}},
                    {"safe", "kept"}});
    anvil::logging::shutdown();
    std::ifstream input(anvil::logging::security_log_path());
    const std::string content((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
    EXPECT_EQ(content.find("super-secret-token"), std::string::npos);
    EXPECT_EQ(content.find("hunter2"), std::string::npos);
    EXPECT_EQ(content.find("source code"), std::string::npos);
    EXPECT_NE(content.find("kept"), std::string::npos);
}

}  // namespace
