import { openSecurity, openSettings, showActivity } from "../actions";
import { useCoreStore } from "../stores/core";
import { useUiStore, type Activity } from "../stores/ui";
import {
  BranchIcon,
  FilesIcon,
  SearchIcon,
  SettingsIcon,
  ShieldIcon,
  SparklesIcon,
} from "../components/Icons";

const activities: Array<{ id: Activity; label: string; icon: typeof FilesIcon }> = [
  { id: "explorer", label: "Explorer", icon: FilesIcon },
  { id: "search", label: "Search", icon: SearchIcon },
  { id: "scm", label: "Source Control", icon: BranchIcon },
  { id: "ai", label: "Anvil AI", icon: SparklesIcon },
];

export function ActivityBar() {
  const active = useUiStore((state) => state.activity);
  const sidebarVisible = useUiStore((state) => state.sidebarVisible);
  const sandbox = useCoreStore((state) => state.security?.sandbox ?? state.info?.security.sandbox);
  const sandboxOk = sandbox?.available === true;

  return (
    <aside className="activity-bar" aria-label="Activity bar">
      {activities.map((activity) => {
        const Icon = activity.icon;
        const selected = active === activity.id && sidebarVisible;
        return (
          <button
            type="button"
            key={activity.id}
            className={`activity-btn${selected ? " active" : ""}`}
            aria-label={activity.label}
            aria-pressed={selected}
            title={activity.label}
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
        aria-label="Security status"
        title={
          sandboxOk
            ? "Security: language servers run in a bubblewrap sandbox"
            : `Security: ${sandbox?.reason ?? "sandbox status unknown"}`
        }
        onClick={openSecurity}
      >
        <ShieldIcon size={18} />
        <span className="security-dot" />
      </button>
      <button
        type="button"
        className="activity-btn"
        aria-label="Settings"
        title="Settings"
        onClick={openSettings}
      >
        <SettingsIcon size={18} />
      </button>
    </aside>
  );
}
