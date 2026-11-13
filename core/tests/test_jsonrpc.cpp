#include <gtest/gtest.h>

#include "anvil/ipc/jsonrpc.hpp"

namespace {

using anvil::ApiError;
using anvil::ErrorCode;
using anvil::ipc::parse_request;

TEST(JsonRpc, ParsesRequestAndNotification) {
    const auto request = parse_request(
        R"({"jsonrpc":"2.0","id":7,"method":"fs/read","params":{"path":"a.cpp"},"apiVersion":1})");
    EXPECT_EQ(request.method, "fs/read");
    EXPECT_EQ(request.api_version, 1);
    EXPECT_FALSE(request.is_notification);
    EXPECT_EQ(request.id.get<int>(), 7);
    EXPECT_EQ(request.params["path"], "a.cpp");

    const auto notification = parse_request(
        R"({"jsonrpc":"2.0","method":"request/cancel","params":{"targetId":7},"apiVersion":1})");
    EXPECT_TRUE(notification.is_notification);
    EXPECT_EQ(notification.api_version, 1);
}

TEST(JsonRpc, RejectsMalformedMessages) {
    EXPECT_THROW(parse_request("{"), ApiError);
    EXPECT_THROW(parse_request("[]"), ApiError);
    EXPECT_THROW(parse_request(R"({"jsonrpc":"1.0","id":1,"method":"x"})"), ApiError);
    EXPECT_THROW(parse_request(R"({"jsonrpc":"2.0","id":1})"), ApiError);
    EXPECT_THROW(parse_request(R"({"jsonrpc":"2.0","id":1.5,"method":"x"})"), ApiError);
    EXPECT_THROW(parse_request(R"({"jsonrpc":"2.0","id":1,"method":"x","params":[]})"), ApiError);
    EXPECT_THROW(parse_request(R"({"jsonrpc":"2.0","id":1,"method":"x","apiVersion":"1"})"), ApiError);
}

TEST(JsonRpc, DepthLimit) {
    const std::string deep(80, '{');
    EXPECT_FALSE(anvil::ipc::json_depth_within(deep, 64));
    EXPECT_TRUE(anvil::ipc::json_depth_within("{\"text\":\"{{{{{{{{{{\"}", 4));
}

TEST(JsonRpc, ResponseHelpers) {
    const auto result = anvil::ipc::make_result(3, anvil::Json{{"ok", true}});
    EXPECT_EQ(result["id"], 3);
    EXPECT_EQ(result["result"]["ok"], true);
    const auto error = anvil::ipc::make_error(4, ErrorCode::PathOutsideWorkspace, "outside");
    EXPECT_EQ(error["error"]["code"], -32002);
    EXPECT_EQ(error["error"]["message"], "outside");
    const auto response = anvil::ipc::parse_response(error);
    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->is_error());
}

TEST(JsonRpc, SerializesOneLine) {
    const std::string line = anvil::ipc::serialize_line(
        anvil::Json{{"jsonrpc", "2.0"}, {"id", 1}, {"result", anvil::Json{{"text", "a\nb"}}}});
    EXPECT_EQ(line.back(), '\n');
    EXPECT_EQ(std::count(line.begin(), line.end(), '\n'), 1);
}

}  // namespace
