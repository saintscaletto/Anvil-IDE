#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "anvil/ipc/jsonrpc.hpp"
#include "anvil/util/process.hpp"
#include "test_util.hpp"

namespace {

using anvil::ChildProcess;
using anvil::SpawnRequest;

class CoreHarness {
public:
    explicit CoreHarness(const std::filesystem::path& temp) {
        SpawnRequest request;
        request.argv = {ANVIL_CORE_PATH, "--stdio"};
        request.env = {
            "PATH=/usr/bin:/bin",
            "LANG=C.UTF-8",
            "HOME=" + (temp / "home").string(),
            "ANVIL_CONFIG_DIR=" + (temp / "config").string(),
            "ANVIL_CACHE_DIR=" + (temp / "cache").string(),
        };
        request.cwd = temp;
        request.stdin_pipe = true;
        request.capture_stdout = true;
        request.capture_stderr = true;
        std::string error;
        auto child = ChildProcess::spawn(request, &error);
        if (!child.has_value()) {
            throw std::runtime_error("cannot spawn anvil-core: " + error);
        }
        child_ = std::move(*child);
        reader_ = std::thread([this] { read_loop(); });
    }

    ~CoreHarness() { stop(); }

    bool send_raw(const std::string& line) {
        return child_.write_stdin(line + "\n");
    }

    std::optional<anvil::Json> wait_for(int id) {
        std::unique_lock lock(mutex_);
        if (!condition_.wait_for(lock, std::chrono::seconds(20),
                                 [&] { return responses_.count(id) != 0 || eof_; })) {
            return std::nullopt;
        }
        const auto it = responses_.find(id);
        if (it == responses_.end()) return std::nullopt;
        anvil::Json response = it->second;
        responses_.erase(it);
        return response;
    }

    std::optional<anvil::Json> request(int id, const std::string& method,
                                       const anvil::Json& params) {
        const anvil::Json message{{"jsonrpc", "2.0"},
                                  {"id", id},
                                  {"method", method},
                                  {"params", params},
                                  {"apiVersion", 1}};
        if (!send_raw(anvil::ipc::serialize_line(message))) {
            return std::nullopt;
        }
        return wait_for(id);
    }

    void stop() {
        if (!child_.valid()) return;
        child_.close_stdin();
        for (int i = 0; i < 100; ++i) {
            if (child_.try_wait().has_value()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        child_.terminate();
        if (reader_.joinable()) reader_.join();
    }

private:
    void read_loop() {
        anvil::FdLineReader reader(child_.stdout_fd());
        std::string line;
        bool too_large = false;
        std::string error;
        while (reader.read_line(&line, 32U * 1024U * 1024U, &too_large, &error)) {
            const anvil::Json message = anvil::Json::parse(line, nullptr, false);
            if (message.is_discarded() || !message.is_object()) continue;
            if (message.contains("method")) continue;
            if (!message.contains("id") || !message["id"].is_number_integer()) continue;
            {
                std::lock_guard lock(mutex_);
                responses_[message["id"].get<int>()] = message;
            }
            condition_.notify_all();
        }
        {
            std::lock_guard lock(mutex_);
            eof_ = true;
        }
        condition_.notify_all();
    }

    ChildProcess child_;
    std::thread reader_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::map<int, anvil::Json> responses_;
    bool eof_ = false;
};

TEST(Server, FullJsonRpcLifecycle) {
    anvil::test::TempDir temp;
    anvil::test::write_file(temp.path() / "workspace" / "src" / "main.cpp",
                            "int main() { return 0; }\n");
    CoreHarness core(temp.path());

    auto info = core.request(1, "system/info", anvil::Json::object());
    ASSERT_TRUE(info.has_value());
    EXPECT_FALSE((*info).contains("error"));
    EXPECT_EQ((*info)["result"]["product"], "Anvil IDE");
    EXPECT_EQ((*info)["result"]["apiVersion"], 1);
    EXPECT_FALSE((*info)["result"]["workspace"]["open"].get<bool>());

    auto opened = core.request(2, "workspace/open",
                               anvil::Json{{"path", (temp.path() / "workspace").string()}});
    ASSERT_TRUE(opened.has_value());
    ASSERT_FALSE((*opened).contains("error"));
    EXPECT_EQ((*opened)["result"]["name"], "workspace");
    EXPECT_TRUE((*opened)["result"]["watch"].get<bool>());

    auto read = core.request(3, "fs/read", anvil::Json{{"path", "src/main.cpp"}});
    ASSERT_TRUE(read.has_value());
    ASSERT_FALSE((*read).contains("error"));
    const std::string original = (*read)["result"]["content"];
    EXPECT_EQ(original, "int main() { return 0; }\n");
    const auto mtime = (*read)["result"]["mtime"].get<int64_t>();

    auto written = core.request(4, "fs/write",
                                anvil::Json{{"path", "src/main.cpp"},
                                            {"content", "int main() { return 1; }\n"},
                                            {"expectedMtime", mtime}});
    ASSERT_TRUE(written.has_value());
    ASSERT_FALSE((*written).contains("error"));
    EXPECT_GE((*written)["result"]["mtime"].get<int64_t>(), mtime);

    auto listing = core.request(5, "fs/list", anvil::Json{{"path", ""}});
    ASSERT_TRUE(listing.has_value());
    bool found_src = false;
    for (const auto& entry : (*listing)["result"]["entries"]) {
        if (entry["name"] == "src") found_src = true;
    }
    EXPECT_TRUE(found_src);

    auto search = core.request(6, "fs/searchFiles", anvil::Json{{"query", "main"}});
    ASSERT_TRUE(search.has_value());
    ASSERT_FALSE((*search).contains("error"));
    EXPECT_GE((*search)["result"]["results"].size(), 1U);

    auto traversal = core.request(7, "fs/read", anvil::Json{{"path", "../etc/passwd"}});
    ASSERT_TRUE(traversal.has_value());
    ASSERT_TRUE((*traversal).contains("error"));
    EXPECT_EQ((*traversal)["error"]["code"], -32602);

    auto absolute = core.request(8, "fs/read", anvil::Json{{"path", "/etc/passwd"}});
    ASSERT_TRUE(absolute.has_value());
    ASSERT_TRUE((*absolute).contains("error"));
    EXPECT_EQ((*absolute)["error"]["code"], -32602);

    auto bad_setting = core.request(9, "settings/set",
                                    anvil::Json{{"key", "rm.rf"}, {"value", true}});
    ASSERT_TRUE(bad_setting.has_value());
    ASSERT_TRUE((*bad_setting).contains("error"));
    EXPECT_EQ((*bad_setting)["error"]["code"], -32602);

    auto branch = core.request(10, "git/branch", anvil::Json::object());
    ASSERT_TRUE(branch.has_value());
    ASSERT_FALSE((*branch).contains("error"));
    EXPECT_FALSE((*branch)["result"]["repository"].get<bool>());

    auto lsp_status = core.request(11, "lsp/status", anvil::Json::object());
    ASSERT_TRUE(lsp_status.has_value());
    ASSERT_FALSE((*lsp_status).contains("error"));
    EXPECT_TRUE((*lsp_status)["result"]["servers"].is_array());

    auto security = core.request(12, "security/status", anvil::Json::object());
    ASSERT_TRUE(security.has_value());
    ASSERT_FALSE((*security).contains("error"));
    EXPECT_TRUE((*security)["result"]["policy"]["workspaceScoped"].get<bool>());

    auto shutdown = core.request(13, "system/shutdown", anvil::Json::object());
    ASSERT_TRUE(shutdown.has_value());
    EXPECT_TRUE((*shutdown)["result"]["ok"].get<bool>());
    core.stop();
}

TEST(Server, RejectsMissingApiVersion) {
    anvil::test::TempDir temp;
    CoreHarness core(temp.path());
    ASSERT_TRUE(core.send_raw(
        R"({"jsonrpc":"2.0","id":50,"method":"system/info","params":{}})"));
    const auto response = core.wait_for(50);
    ASSERT_TRUE(response.has_value());
    ASSERT_TRUE((*response).contains("error"));
    EXPECT_EQ((*response)["error"]["code"], -32600);
    core.stop();
}

}  // namespace
