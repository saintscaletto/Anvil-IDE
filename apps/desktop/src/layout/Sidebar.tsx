import { useUiStore } from "../stores/ui";
import { AiPanel } from "../features/AiPanel";
import { Explorer } from "../features/Explorer";
import { ScmPanel } from "../features/ScmPanel";
import { SearchPanel } from "../features/SearchPanel";
import { useI18n, type MessageKey } from "../i18n";

const activityLabels: Record<string, MessageKey> = {
  explorer: "activity.explorer",
  search: "activity.search",
  scm: "activity.scm",
  ai: "activity.ai",
};

export function Sidebar() {
  const { t } = useI18n();
  const activity = useUiStore((state) => state.activity);
  return (
    <aside
      className="sidebar"
      aria-label={t("sidebar.label", { activity: t(activityLabels[activity]) })}
    >
      {activity === "explorer" ? <Explorer /> : null}
      {activity === "search" ? <SearchPanel /> : null}
      {activity === "scm" ? <ScmPanel /> : null}
      {activity === "ai" ? <AiPanel /> : null}
    </aside>
  );
}
