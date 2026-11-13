import { useEffect, useRef, useState } from "react";
import { commands, type Command } from "../commands";
import { openRunConfig } from "../actions";
import { useWorkspaceStore } from "../stores/workspace";
import { BugIcon, PlayIcon } from "../components/Icons";

interface MenuDefinition {
  label: string;
  categories: string[];
}

const menus: MenuDefinition[] = [
  { label: "File", categories: ["File"] },
  { label: "Edit", categories: ["Edit"] },
  { label: "View", categories: ["View", "Explorer"] },
  { label: "Go", categories: ["Go"] },
  { label: "Run", categories: ["Run"] },
  { label: "Help", categories: ["Anvil"] },
];

export function TitleBar() {
  const name = useWorkspaceStore((state) => state.name);
  const root = useWorkspaceStore((state) => state.root);
  const [openMenu, setOpenMenu] = useState<string | null>(null);
  const containerRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    if (openMenu === null) return;
    const onPointerDown = (event: MouseEvent) => {
      if (!containerRef.current?.contains(event.target as Node)) setOpenMenu(null);
    };
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") setOpenMenu(null);
    };
    document.addEventListener("mousedown", onPointerDown);
    document.addEventListener("keydown", onKeyDown);
    return () => {
      document.removeEventListener("mousedown", onPointerDown);
      document.removeEventListener("keydown", onKeyDown);
    };
  }, [openMenu]);

  const runCommand = (command: Command) => {
    setOpenMenu(null);
    void command.run();
  };

  return (
    <header className="titlebar" ref={containerRef}>
      <div className="titlebar-brand" aria-label="Anvil IDE">
        <span className="brand-mark">A</span>
        <span className="brand-name">Anvil</span>
      </div>
      <div className="titlebar-project" title={root ?? "No folder open"}>
        {name || "No Folder"}
      </div>
      <nav className="titlebar-menu" aria-label="Application menu">
        {menus.map((menu) => {
          const items = commands.filter((command) => menu.categories.includes(command.category));
          const isOpen = openMenu === menu.label;
          return (
            <div className="menu-root" key={menu.label}>
              <button
                type="button"
                className={`menu-button${isOpen ? " open" : ""}`}
                aria-haspopup="menu"
                aria-expanded={isOpen}
                onClick={() => setOpenMenu(isOpen ? null : menu.label)}
              >
                {menu.label}
              </button>
              {isOpen ? (
                <div className="menu-dropdown" role="menu">
                  {items.map((command) => (
                    <button
                      type="button"
                      role="menuitem"
                      className="menu-item"
                      key={command.id}
                      onClick={() => runCommand(command)}
                    >
                      <span className="menu-item-title">{command.title}</span>
                      {command.keybinding ? <kbd>{command.keybinding}</kbd> : null}
                    </button>
                  ))}
                </div>
              ) : null}
            </div>
          );
        })}
      </nav>
      <div className="titlebar-spacer" />
      <div className="titlebar-actions">
        <button
          type="button"
          className="run-button"
          title="Run configuration (build integration arrives in Phase 3)"
          onClick={openRunConfig}
        >
          <PlayIcon size={12} />
          Run
        </button>
        <button
          type="button"
          className="run-button"
          title="Debug (DAP integration arrives in Phase 3)"
          onClick={openRunConfig}
        >
          <BugIcon size={12} />
          Debug
        </button>
      </div>
    </header>
  );
}
