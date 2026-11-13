import { openSecurity, openSettings, showActivity } from "../actions";
import { useCoreStore } from "../stores/core";
import { useUiStore, type Activity } from "../stores/ui";
import type { MessageKey } from "../i18n";
import { useI18n } from "../i18n";
import {
  BranchIcon,
  FilesIcon,
  SearchIcon,
  SettingsIcon,
  ShieldIcon,
  SparklesIcon,
} from "../components/Icons";

const activities: Array<{ id: Activity; labelKey: MessageKey; icon: typeof FilesIcon }> = [
  { id: "explorer", labelKey: "activity.explorer", icon: FilesIcon },
  { id: "search", labelKey: "activity.search", icon: SearchIcon },
  { id: "scm", labelKey: "activity.scm", icon: BranchIcon },
  { id: "ai", labelKey: "activity.ai", icon: SparklesIcon },
];

export function ActivityBar() {
  const { t } = useI18n();
  const active = useUiStore((state) => state.activity);
  const sidebarVisible = useUiStore((state) => state.sidebarVisible);
  const sandbox = useCoreStore((state) => state.security?.sandbox ?? state.info?.security.sandbox);
  const sandboxOk = sandbox?.available === true;

  return (
    <aside className="activity-bar" aria-label={t("activity.barLabel")}>
      {activities.map((activity) => {
        const Icon = activity.icon;
        const label = t(activity.labelKey);
        const selected = active === activity.id && sidebarVisible;
        return (
          <button
            type="button"
            key={activity.id}
            className={`activity-btn${selected ? " active" : ""}`}
            aria-label={label}
            aria-pressed={selected}
            title={label}
            onClick={() => showActivity(activity.id)}
          >
            <Icon size={19} />
          </button>
        );
      })}
      <div className="activity-spacer" />
      <button
        type="button"
        className={`activity-btn security-btn${sandboxOk ? " secure" : " insecure"}`}
        aria-label={t("activity.securityStatus")}
        title={
          sandboxOk
            ? t("activity.securityOk")
            : t("activity.securityReason", { reason: sandbox?.reason ?? t("common.unknown") })
        }
        onClick={openSecurity}
      >
        <ShieldIcon size={18} />
        <span className="security-dot" />
      </button>
      <button
        type="button"
        className="activity-btn"
        aria-label={t("activity.settings")}
        title={t("activity.settings")}
        onClick={openSettings}
      >
        <SettingsIcon size={18} />
      </button>
    </aside>
  );
}
