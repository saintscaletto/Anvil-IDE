#pragma once

#include "anvil/error.hpp"
#include "anvil/workspace/workspace.hpp"

namespace anvil {

// Reads only .git/HEAD (and .git as a file marker) to provide a branch
// indicator. No Git plumbing is executed and linked worktrees are reported as
// such instead of following a gitdir outside the workspace.
Json read_git_branch(const Workspace& workspace);

}  // namespace anvil
