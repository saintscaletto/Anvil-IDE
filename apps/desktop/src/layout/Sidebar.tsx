import { useUiStore } from "../stores/ui";
import { AiPanel } from "../features/AiPanel";
import { Explorer } from "../features/Explorer";
import { ScmPanel } from "../features/ScmPanel";
import { SearchPanel } from "../features/SearchPanel";

export function Sidebar() {
  const activity = useUiStore((state) => state.activity);
  return (
    <aside className="sidebar" aria-label={`${activity} sidebar`}>
      {activity === "explorer" ? <Explorer /> : null}
      {activity === "search" ? <SearchPanel /> : null}
      {activity === "scm" ? <ScmPanel /> : null}
      {activity === "ai" ? <AiPanel /> : null}
    </aside>
  );
}
