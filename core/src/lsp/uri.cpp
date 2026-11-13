#include "anvil/lsp/uri.hpp"

#include <system_error>

#include "anvil/util/strings.hpp"

namespace anvil::lsp {

std::string path_to_uri(const Workspace& workspace, const std::string& relative_path) {
    const auto absolute = workspace.resolve(relative_path);
    return strings::path_to_file_uri(absolute.string());
}

std::optional<std::string> uri_to_workspace_path(const Workspace& workspace,
                                                 std::string_view uri) {
    const auto absolute = strings::file_uri_to_path(uri);
    if (!absolute.has_value()) return std::nullopt;
    try {
        return workspace.relative(*absolute);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace anvil::lsp
