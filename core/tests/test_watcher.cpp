#include <gtest/gtest.h>

#include <mutex>
#include <string>

#include "anvil/workspace/watcher.hpp"
#include "test_util.hpp"

namespace {

TEST(Watcher, ReportsCreatedFile) {
    anvil::test::TempDir temp;
    std::filesystem::create_directories(temp.path() / "src");
    anvil::test::EventGate gate;
    std::mutex mutex;
    anvil::Json captured;

    anvil::Watcher watcher;
    watcher.start(temp.path(), [&](anvil::Json event) {
        {
            std::lock_guard lock(mutex);
            captured = std::move(event);
        }
        gate.set();
    });

    anvil::test::write_file(temp.path() / "src" / "hello.txt", "hello\n");
    ASSERT_TRUE(gate.wait(std::chrono::seconds(6)));

    std::lock_guard lock(mutex);
    ASSERT_TRUE(captured.is_object());
    EXPECT_EQ(captured.value("type", ""), "changed");
    bool saw_file = false;
    for (const auto& change : captured["changes"]) {
        if (change["path"] == "src/hello.txt") saw_file = true;
    }
    EXPECT_TRUE(saw_file);
    watcher.stop();
}

}  // namespace
