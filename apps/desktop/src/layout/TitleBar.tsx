import { useEffect, useRef, useState } from "react";
import { commands, type Command } from "../commands";
import { openRunConfig } from "../actions";
import type { MessageKey } from "../i18n";
import { useI18n } from "../i18n";
import { useWorkspaceStore } from "../stores/workspace";
import { BugIcon, PlayIcon } from "../components/Icons";

interface MenuDefinition {
  id: string;
  labelKey: MessageKey;
  categories: string[];
}

const menus: MenuDefinition[] = [
  { id: "file", labelKey: "menu.file", categories: ["File"] },
  { id: "edit", labelKey: "menu.edit", categories: ["Edit"] },
  { id: "view", labelKey: "menu.view", categories: ["View", "Explorer"] },
  { id: "go", labelKey: "menu.go", categories: ["Go"] },
  { id: "run", labelKey: "menu.run", categories: ["Run"] },
  { id: "help", labelKey: "menu.help", categories: ["Anvil"] },
];

export function TitleBar() {
  const { t } = useI18n();
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
      <div className="titlebar-brand" aria-label={t("app.product")}>
        <span className="brand-mark">A</span>
        <span className="brand-name">Anvil</span>
      </div>
      <div className="titlebar-project" title={root ?? t("titlebar.noFolderOpen")}>
        {name || t("titlebar.noFolder")}
      </div>
      <nav className="titlebar-menu" aria-label={t("titlebar.applicationMenu")}>
        {menus.map((menu) => {
          const items = commands.filter((command) => menu.categories.includes(command.category));
          const isOpen = openMenu === menu.id;
          return (
            <div className="menu-root" key={menu.id}>
              <button
                type="button"
                className={`menu-button${isOpen ? " open" : ""}`}
                aria-haspopup="menu"
                aria-expanded={isOpen}
                onClick={() => setOpenMenu(isOpen ? null : menu.id)}
              >
                {t(menu.labelKey)}
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
                      <span className="menu-item-title">{t(command.titleKey)}</span>
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
          title={t("titlebar.runTitle")}
          onClick={openRunConfig}
        >
          <PlayIcon size={12} />
          {t("common.run")}
        </button>
        <button
          type="button"
          className="run-button"
          title={t("titlebar.debugTitle")}
          onClick={openRunConfig}
        >
          <BugIcon size={12} />
          {t("common.debug")}
        </button>
      </div>
    </header>
  );
}
