#include <gtest/gtest.h>

#include <cstdlib>

#include <chrono>
#include <future>
#include <string>

#include "anvil/lsp/client.hpp"
#include "anvil/security/seccomp.hpp"
#include "test_util.hpp"

namespace {

TEST(LspClient, HandshakeDiagnosticsAndHover) {
    ::setenv("ANVIL_DEV_UNSAFE_LSP", "1", 1);
    anvil::test::TempDir temp;
    const auto workspace = temp.path() / "project";
    const auto cache = temp.path() / "cache";
    std::filesystem::create_directories(workspace);
    std::filesystem::create_directories(cache);
    anvil::test::write_file(workspace / "main.cpp", "int main() {}\n");

    anvil::lsp::ClientOptions options;
    options.program = ANVIL_FAKE_LSP_PATH;
    options.arguments = {};
    options.root_uri = "file://" + workspace.string();
    options.sandbox.workspace = workspace;
    options.sandbox.cache_dir = cache;
    options.seccomp_profile = anvil::security::SeccompProfile::LspDev;
    options.allow_unsandboxed_dev = true;

    std::string name;
    std::string version;
    auto client = anvil::lsp::Client::create(options, &name, &version);
    ASSERT_NE(client, nullptr);
    EXPECT_EQ(name, "fake-lsp");
    EXPECT_FALSE(version.empty());

    std::promise<anvil::Json> diagnostics_promise;
    auto diagnostics_future = diagnostics_promise.get_future();
    client->set_notification_handler([&](const std::string& method, const anvil::Json& params) {
        if (method == "textDocument/publishDiagnostics") {
            try {
                diagnostics_promise.set_value(params);
            } catch (const std::future_error&) {
            }
        }
    });

    const std::string uri = options.root_uri + "/main.cpp";
    client->notify("textDocument/didOpen",
                   anvil::Json{{"textDocument",
                                {{"uri", uri}, {"languageId", "cpp"}, {"version", 1},
                                 {"text", "int main() {}\n"}}}});
    ASSERT_EQ(diagnostics_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    const auto diagnostics = diagnostics_future.get();
    ASSERT_TRUE(diagnostics.contains("diagnostics"));
    ASSERT_EQ(diagnostics["diagnostics"].size(), 1U);
    EXPECT_EQ(diagnostics["diagnostics"][0]["message"], "fake diagnostic");

    const auto hover = client->request(
        "textDocument/hover",
        anvil::Json{{"textDocument", {{"uri", uri}}},
                    {"position", {{"line", 0}, {"character", 0}}}},
        nullptr, 5000);
    EXPECT_EQ(hover["contents"]["value"], "**fake hover**");
    client->shutdown();
    EXPECT_FALSE(client->running());
}

}  // namespace
