import { BranchIcon, RefreshIcon } from "../components/Icons";
import { useWorkspaceStore } from "../stores/workspace";
import { useI18n } from "../i18n";

export function ScmPanel() {
  const { t } = useI18n();
  const root = useWorkspaceStore((state) => state.root);
  const git = useWorkspaceStore((state) => state.git);
  const refreshGit = useWorkspaceStore((state) => state.refreshGit);

  if (!root) {
    return (
      <div className="sidebar-panel">
        <div className="sidebar-header">{t("scm.title")}</div>
        <div className="sidebar-empty">
          <p>{t("scm.openFolder")}</p>
        </div>
      </div>
    );
  }

  return (
    <div className="sidebar-panel">
      <div className="sidebar-header">
        <span>{t("scm.title")}</span>
        <div className="sidebar-actions">
          <button
            type="button"
            className="icon-button"
            title={t("scm.refreshBranch")}
            aria-label={t("scm.refreshBranch")}
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
              <span>{git.branch || t("common.unknown")}</span>
              {git.detached ? <span className="muted">{t("scm.detachedHead")}</span> : null}
            </div>
            <div className="sidebar-empty compact">
              <p>{t("scm.branchOnly")}</p>
              <p className="muted">
                {t("scm.phase4Note")}
              </p>
            </div>
          </>
        ) : (
          <div className="sidebar-empty">
            <p>{t("scm.noRepo")}</p>
            <p className="muted">{t("scm.phase4")}</p>
          </div>
        )}
      </div>
    </div>
  );
}
