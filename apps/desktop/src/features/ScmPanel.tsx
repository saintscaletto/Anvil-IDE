import { BranchIcon, RefreshIcon } from "../components/Icons";
import { useWorkspaceStore } from "../stores/workspace";

export function ScmPanel() {
  const root = useWorkspaceStore((state) => state.root);
  const git = useWorkspaceStore((state) => state.git);
  const refreshGit = useWorkspaceStore((state) => state.refreshGit);

  if (!root) {
    return (
      <div className="sidebar-panel">
        <div className="sidebar-header">Source Control</div>
        <div className="sidebar-empty">
          <p>Open a folder to inspect its repository.</p>
        </div>
      </div>
    );
  }

  return (
    <div className="sidebar-panel">
      <div className="sidebar-header">
        <span>Source Control</span>
        <div className="sidebar-actions">
          <button
            type="button"
            className="icon-button"
            title="Refresh branch"
            aria-label="Refresh branch"
            onClick={() => void refreshGit()}
          >
            <RefreshIcon size={14} />
          </button>
        </div>
      </div>
      <div className="scm-panel">
        {git?.repository ? (
          <>
            <div className="scm-branch">
              <BranchIcon size={15} />
              <span>{git.branch || "unknown"}</span>
              {git.detached ? <span className="muted">detached HEAD</span> : null}
            </div>
            <div className="sidebar-empty compact">
              <p>This release shows the current branch only.</p>
              <p className="muted">
                Staging, diffs, and commits are scheduled for Phase 4. Git hooks are never
                executed by Anvil.
              </p>
            </div>
          </>
        ) : (
          <div className="sidebar-empty">
            <p>No Git repository detected in this workspace.</p>
            <p className="muted">Source-control integration is scheduled for Phase 4.</p>
          </div>
        )}
      </div>
    </div>
  );
}
