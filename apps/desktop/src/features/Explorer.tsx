import { useEffect, useMemo, useRef, useState } from "react";
import { openFolderDialog } from "../actions";
import {
  ChevronDownIcon,
  ChevronRightIcon,
  CollapseIcon,
  EditIcon,
  FileIcon,
  FolderIcon,
  FolderOpenIcon,
  NewFileIcon,
  NewFolderIcon,
  RefreshIcon,
  TrashIcon,
} from "../components/Icons";
import { dirname, basename } from "../lib/paths";
import { useEditorStore } from "../stores/editor";
import { useSettingsStore } from "../stores/settings";
import { useUiStore } from "../stores/ui";
import { useWorkspaceStore } from "../stores/workspace";
import type { DirEntry } from "../ipc/types";
import { useI18n } from "../i18n";

interface Row {
  entry: DirEntry;
  depth: number;
}

interface ContextMenuState {
  x: number;
  y: number;
  entry: DirEntry;
}

export function Explorer() {
  const { t } = useI18n();
  const root = useWorkspaceStore((state) => state.root);
  const entries = useWorkspaceStore((state) => state.entries);
  const expanded = useWorkspaceStore((state) => state.expanded);
  const loading = useWorkspaceStore((state) => state.loading);
  const selected = useWorkspaceStore((state) => state.selected);
  const toggleDir = useWorkspaceStore((state) => state.toggleDir);
  const loadDir = useWorkspaceStore((state) => state.loadDir);
  const select = useWorkspaceStore((state) => state.select);
  const collapseAll = useWorkspaceStore((state) => state.collapseAll);
  const hideGit = useSettingsStore((state) => state.values["explorer.hideGit"]);
  const [contextMenu, setContextMenu] = useState<ContextMenuState | null>(null);
  const treeRef = useRef<HTMLDivElement | null>(null);

  const rows = useMemo(() => {
    const result: Row[] = [];
    const walk = (directory: string, depth: number) => {
      const list = entries[directory];
      if (!list) return;
      for (const entry of list) {
        if (hideGit && entry.name === ".git") continue;
        result.push({ entry, depth });
        if (entry.isDir && expanded[entry.path]) walk(entry.path, depth + 1);
      }
    };
    walk("", 0);
    return result;
  }, [entries, expanded, hideGit]);

  useEffect(() => {
    if (!contextMenu) return;
    const close = () => setContextMenu(null);
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") close();
    };
    document.addEventListener("mousedown", close);
    document.addEventListener("keydown", onKey);
    return () => {
      document.removeEventListener("mousedown", close);
      document.removeEventListener("keydown", onKey);
    };
  }, [contextMenu]);

  const openFile = (path: string) => {
    select(path);
    void useEditorStore.getState().openFile(path);
  };

  const activateRow = (entry: DirEntry) => {
    if (entry.isDir) {
      select(entry.path);
      void toggleDir(entry.path);
    } else {
      openFile(entry.path);
    }
  };

  const onKeyDown = (event: React.KeyboardEvent<HTMLDivElement>) => {
    if (rows.length === 0) return;
    const currentIndex = rows.findIndex((row) => row.entry.path === selected);
    if (event.key === "ArrowDown" || event.key === "ArrowUp") {
      event.preventDefault();
      const delta = event.key === "ArrowDown" ? 1 : -1;
      const next = currentIndex < 0 ? 0 : Math.min(rows.length - 1, Math.max(0, currentIndex + delta));
      select(rows[next]!.entry.path);
      return;
    }
    if (event.key === "Home" || event.key === "End") {
      event.preventDefault();
      select((event.key === "Home" ? rows[0] : rows[rows.length - 1])!.entry.path);
      return;
    }
    if (currentIndex < 0) return;
    const entry = rows[currentIndex]!.entry;
    if (event.key === "ArrowRight" && entry.isDir) {
      event.preventDefault();
      if (!expanded[entry.path]) void toggleDir(entry.path);
      return;
    }
    if (event.key === "ArrowLeft") {
      event.preventDefault();
      if (entry.isDir && expanded[entry.path]) {
        void toggleDir(entry.path);
      } else {
        const parent = dirname(entry.path);
        if (parent) select(parent);
        else select(null);
      }
      return;
    }
    if (event.key === "Enter") {
      event.preventDefault();
      activateRow(entry);
    }
  };

  const openNewPrompt = (entry: DirEntry | null, kind: "file" | "directory") => {
    setContextMenu(null);
    const ui = useUiStore.getState();
    const directory = entry === null ? "" : entry.isDir ? entry.path : dirname(entry.path);
    ui.showPrompt({
      title: kind === "file" ? t("dialog.newFileTitle") : t("dialog.newFolderTitle"),
      label: directory
        ? t("explorer.createInDirectory", { directory })
        : t("explorer.createInRoot"),
      confirmLabel: t("common.create"),
      onSubmit: async (name) => {
        const path = await useWorkspaceStore.getState().createEntry(directory, name, kind);
        if (path && kind === "file") await useEditorStore.getState().openFile(path);
      },
    });
  };

  if (!root) {
    return (
      <div className="sidebar-panel">
        <div className="sidebar-header">{t("explorer.title")}</div>
        <div className="sidebar-empty">
          <p>{t("explorer.notOpened")}</p>
          <button type="button" className="primary-button" onClick={() => void openFolderDialog()}>
            {t("command.openFolder")}
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="sidebar-panel">
      <div className="sidebar-header">
        <span>{t("explorer.title")}</span>
        <div className="sidebar-actions">
          <button
            type="button"
            className="icon-button"
            title={t("command.newFile")}
            aria-label={t("command.newFile")}
            onClick={() => openNewPrompt(null, "file")}
          >
            <NewFileIcon size={14} />
          </button>
          <button
            type="button"
            className="icon-button"
            title={t("command.newFolder")}
            aria-label={t("command.newFolder")}
            onClick={() => openNewPrompt(null, "directory")}
          >
            <NewFolderIcon size={14} />
          </button>
          <button
            type="button"
            className="icon-button"
            title={t("command.refreshExplorer")}
            aria-label={t("command.refreshExplorer")}
            onClick={() => void useWorkspaceStore.getState().refreshAll()}
          >
            <RefreshIcon size={14} />
          </button>
          <button
            type="button"
            className="icon-button"
            title={t("command.collapseFolders")}
            aria-label={t("command.collapseFolders")}
            onClick={collapseAll}
          >
            <CollapseIcon size={14} />
          </button>
        </div>
      </div>
      <div className="explorer-root" title={root}>
        {basename(root)}
      </div>
      <div
        className="file-tree"
        role="tree"
        aria-label={t("explorer.title")}
        tabIndex={0}
        ref={treeRef}
        onKeyDown={onKeyDown}
      >
        {rows.length === 0 && !loading[""] ? (
          <div className="tree-empty">{t("explorer.emptyFolder")}</div>
        ) : null}
        {rows.map(({ entry, depth }) => {
          const isOpen = expanded[entry.path] === true;
          const isLoading = loading[entry.path] === true;
          return (
            <div
              key={entry.path}
              role="treeitem"
              aria-level={depth + 1}
              aria-selected={selected === entry.path}
              aria-expanded={entry.isDir ? isOpen : undefined}
              className={`tree-item${selected === entry.path ? " selected" : ""}`}
              style={{ paddingLeft: 10 + depth * 14 }}
              title={entry.path}
              onClick={() => activateRow(entry)}
              onContextMenu={(event) => {
                event.preventDefault();
                event.stopPropagation();
                select(entry.path);
                setContextMenu({ x: event.clientX, y: event.clientY, entry });
              }}
            >
              <span className="tree-chevron">
                {entry.isDir ? (
                  isOpen ? (
                    <ChevronDownIcon size={12} />
                  ) : (
                    <ChevronRightIcon size={12} />
                  )
                ) : null}
              </span>
              <span className={`tree-icon ${entry.isDir ? "dir" : "file"}`}>
                {entry.isDir ? (
                  isOpen ? (
                    <FolderOpenIcon size={15} />
                  ) : (
                    <FolderIcon size={15} />
                  )
                ) : fileBadge(entry.name) ? (
                  <span className="file-badge">{fileBadge(entry.name)}</span>
                ) : (
                  <FileIcon size={14} />
                )}
              </span>
              <span className="tree-name">{entry.name}</span>
              {entry.isSymlink ? <span className="tree-link" title={t("explorer.symlink")}>↗</span> : null}
              {isLoading ? <span className="tree-loading" aria-label={t("explorer.loading")} /> : null}
            </div>
          );
        })}
      </div>
      {contextMenu ? (
        <div
          className="context-menu"
          style={{ left: contextMenu.x, top: contextMenu.y }}
          role="menu"
          onMouseDown={(event) => event.stopPropagation()}
        >
          <button
            type="button"
            role="menuitem"
            onClick={() => openNewPrompt(contextMenu.entry, "file")}
          >
            <NewFileIcon size={13} /> {t("command.newFile")}
          </button>
          <button
            type="button"
            role="menuitem"
            onClick={() => openNewPrompt(contextMenu.entry, "directory")}
          >
            <NewFolderIcon size={13} /> {t("command.newFolder")}
          </button>
          <div className="menu-separator" />
          <button
            type="button"
            role="menuitem"
            onClick={() => {
              setContextMenu(null);
              useUiStore.getState().showPrompt({
                title: t("dialog.renameTitle"),
                label: t("dialog.renameLabel"),
                initialValue: basename(contextMenu.entry.path),
                confirmLabel: t("common.rename"),
                onSubmit: async (name) => {
                  const newPath = await useWorkspaceStore
                    .getState()
                    .renameEntry(contextMenu.entry.path, name);
                  if (newPath) {
                    await useEditorStore.getState().handleRenamed(contextMenu.entry.path, newPath);
                  }
                },
              });
            }}
          >
            <EditIcon size={13} /> {t("common.rename")}
          </button>
          <button
            type="button"
            role="menuitem"
            onClick={() => {
              setContextMenu(null);
              const current = contextMenu.entry;
              useUiStore.getState().showConfirm({
                title: t("dialog.deleteTitle"),
                message: t("explorer.deleteMessage", { name: current.name }),
                confirmLabel: t("common.delete"),
                danger: true,
                onConfirm: async () => {
                  const workspace = useWorkspaceStore.getState();
                  const deleted = await workspace.deleteEntry(current.path, false);
                  if (!deleted && current.isDir) {
                    useUiStore.getState().showConfirm({
                      title: t("explorer.deleteRecursiveTitle"),
                      message: t("explorer.deleteRecursiveMessage", { name: current.name }),
                      confirmLabel: t("explorer.deleteRecursiveConfirm"),
                      danger: true,
                      onConfirm: async () => {
                        await workspace.deleteEntry(current.path, true);
                      },
                    });
                  }
                },
              });
            }}
          >
            <TrashIcon size={13} /> {t("common.delete")}
          </button>
          <button
            type="button"
            role="menuitem"
            onClick={() => {
              setContextMenu(null);
              const directory = contextMenu.entry.isDir
                ? contextMenu.entry.path
                : dirname(contextMenu.entry.path);
              void loadDir(directory, true);
            }}
          >
            <RefreshIcon size={13} /> {t("common.refresh")}
          </button>
        </div>
      ) : null}
    </div>
  );
}

function fileBadge(name: string): string | null {
  const lower = name.toLowerCase();
  if (lower.endsWith(".cpp") || lower.endsWith(".cc") || lower.endsWith(".cxx")) return "C++";
  if (lower.endsWith(".c")) return "C";
  if (lower.endsWith(".h") || lower.endsWith(".hpp") || lower.endsWith(".hh")) return "H";
  if (lower.endsWith(".rs")) return "Rs";
  if (lower.endsWith(".py")) return "Py";
  if (lower.endsWith(".json")) return "{}";
  if (lower.endsWith(".md")) return "M↓";
  if (lower.endsWith(".cmake") || lower === "cmakelists.txt") return "CM";
  return null;
}
