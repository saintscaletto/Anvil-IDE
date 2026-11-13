#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "anvil/workspace/workspace.hpp"

namespace anvil::lsp {

// Converts a workspace-relative path into a percent-encoded file:// URI.
std::string path_to_uri(const Workspace& workspace, const std::string& relative_path);

// Returns nullopt for non-file URIs, malformed URIs and paths outside the
// workspace. Diagnostics for system headers are intentionally ignored.
std::optional<std::string> uri_to_workspace_path(const Workspace& workspace,
                                                 std::string_view uri);

}  // namespace anvil::lsp
