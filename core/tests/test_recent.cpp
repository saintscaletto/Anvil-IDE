#include <gtest/gtest.h>


#include "anvil/recent.hpp"
#include "test_util.hpp"

namespace {

TEST(Recent, PersistsAndDeduplicates) {
    anvil::test::TempDir temp;
    const auto config = temp.path() / "config";
    anvil::test::write_file(temp.path() / "one" / "keep", "x");
    anvil::test::write_file(temp.path() / "two" / "keep", "x");
    anvil::RecentProjects recent;
    recent.load(config);
    recent.add(temp.path() / "one", "one");
    recent.add(temp.path() / "two", "two");
    recent.add(temp.path() / "one", "one-again");
    EXPECT_EQ(recent.list().size(), 2U);
    EXPECT_EQ(recent.list()[0]["name"], "one-again");
    recent.save();

    anvil::RecentProjects reloaded;
    reloaded.load(config);
    EXPECT_EQ(reloaded.list().size(), 2U);
    EXPECT_TRUE(reloaded.remove((temp.path() / "one").string()));
    EXPECT_EQ(reloaded.list().size(), 1U);
}

TEST(WorkspaceState, SavesAndLoadsValidatedState) {
    anvil::test::TempDir temp;
    anvil::WorkspaceStateStore store(temp.path() / "config");
    store.save("abc123", {"src/main.cpp", "include/engine.h"}, "src/main.cpp");
    const auto state = store.load("abc123");
    EXPECT_EQ(state["tabs"].size(), 2U);
    EXPECT_EQ(state["active"], "src/main.cpp");
    EXPECT_EQ(store.load("invalid/key")["tabs"].size(), 0U);
    EXPECT_EQ(store.load("missing")["tabs"].size(), 0U);
}

TEST(WorkspaceState, RejectsInconsistentState) {
    anvil::test::TempDir temp;
    anvil::WorkspaceStateStore store(temp.path() / "config");
    anvil::test::write_file(temp.path() / "config" / "workspace" / "abc.json",
                            R"({"tabs":["a"],"active":"b"})");
    const auto state = store.load("abc");
    EXPECT_EQ(state["tabs"].size(), 0U);
    EXPECT_EQ(state["active"], "");
}

}  // namespace
