#include "anvil/git_branch.hpp"

#include <filesystem>
#include <system_error>

#include "anvil/util/fs_util.hpp"
#include "anvil/util/log.hpp"
#include "anvil/util/strings.hpp"

namespace anvil {
namespace fs = std::filesystem;

Json read_git_branch(const Workspace& workspace) {
    try {
        const fs::path git_entry = workspace.resolve(".git");
        std::error_code ec;
        if (fs::is_regular_file(git_entry, ec)) {
            const std::string marker = fsutil::read_text_file(git_entry, 4096);
            if (strings::starts_with(marker, "gitdir:")) {
                return Json{{"repository", true},
                            {"worktree", true},
                            {"branch", ""},
                            {"detached", false},
                            {"reason", "linked worktree is not inspected"}};
            }
            return Json{{"repository", false}};
        }
        if (!fs::is_directory(git_entry, ec)) return Json{{"repository", false}};

        const std::string head = fsutil::read_text_file(workspace.resolve(".git/HEAD"), 4096);
        const std::string trimmed = strings::trim(head);
        if (strings::starts_with(trimmed, "ref: refs/heads/")) {
            const std::string branch = trimmed.substr(std::string("ref: refs/heads/").size());
            if (branch.empty() || branch.size() > 512 ||
                branch.find('\0') != std::string::npos ||
                branch.find_first_of("\r\n") != std::string::npos) {
                return Json{{"repository", true}, {"branch", ""}, {"detached", false}};
            }
            return Json{{"repository", true},
                        {"worktree", false},
                        {"branch", branch},
                        {"detached", false}};
        }
        if (trimmed.size() >= 7 && trimmed.size() <= 64) {
            return Json{{"repository", true},
                        {"worktree", false},
                        {"branch", trimmed.substr(0, 12)},
                        {"detached", true}};
        }
        return Json{{"repository", true}, {"branch", ""}, {"detached", false}};
    } catch (const std::exception& error) {
        logging::debug(std::string("git branch lookup failed: ") + error.what());
        return Json{{"repository", false}};
    }
}

}  // namespace anvil
