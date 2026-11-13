import { create } from "zustand";
import { api } from "../ipc/api";
import { AnvilError } from "../ipc/bridge";
import type {
  DirEntry,
  FsChange,
  GitInfo,
  RecentProject,
  SearchResult,
  WorkspaceOpenResult,
} from "../ipc/types";
import { basename, dirname, isValidEntryName, joinPath } from "../lib/paths";
import { useUiStore } from "./ui";

interface WorkspaceState {
  root: string | null;
  name: string;
  recent: RecentProject[];
  git: GitInfo | null;
  entries: Record<string, DirEntry[]>;
  expanded: Record<string, boolean>;
  loading: Record<string, boolean>;
  selected: string | null;
  watcherAvailable: boolean;
  openWorkspace: (path: string) => Promise<WorkspaceOpenResult | null>;
  closeWorkspace: () => Promise<void>;
  loadDir: (path: string, force?: boolean) => Promise<void>;
  toggleDir: (path: string) => Promise<void>;
  refreshAll: () => Promise<void>;
  refreshGit: () => Promise<void>;
  select: (path: string | null) => void;
  collapseAll: () => void;
  onFsChanged: (changes: FsChange[]) => void;
  createEntry: (directory: string, name: string, kind: "file" | "directory") => Promise<string | null>;
  renameEntry: (path: string, newName: string) => Promise<string | null>;
  deleteEntry: (path: string, recursive: boolean) => Promise<boolean>;
  searchFiles: (query: string, limit?: number) => Promise<SearchResult[]>;
  setRecent: (recent: RecentProject[]) => void;
  setWatcherAvailable: (available: boolean) => void;
  reset: () => void;
}

const pendingRefresh = new Set<string>();
let refreshTimer = 0;

function errorText(error: unknown): string {
  if (error instanceof AnvilError || error instanceof Error) return error.message;
  return "Unexpected filesystem error";
}

function removeSubtree(
  entries: Record<string, DirEntry[]>,
  expanded: Record<string, boolean>,
  path: string,
): { entries: Record<string, DirEntry[]>; expanded: Record<string, boolean> } {
  const nextEntries = { ...entries };
  const nextExpanded = { ...expanded };
  for (const key of Object.keys(nextEntries)) {
    if (key === path || key.startsWith(`${path}/`)) delete nextEntries[key];
  }
  for (const key of Object.keys(nextExpanded)) {
    if (key === path || key.startsWith(`${path}/`)) delete nextExpanded[key];
  }
  return { entries: nextEntries, expanded: nextExpanded };
}

export const useWorkspaceStore = create<WorkspaceState>((set, get) => ({
  root: null,
  name: "",
  recent: [],
  git: null,
  entries: {},
  expanded: {},
  loading: {},
  selected: null,
  watcherAvailable: false,

  openWorkspace: async (path) => {
    try {
      const result = await api.openWorkspace(path);
      set({
        root: result.root,
        name: result.name,
        recent: result.recent,
        git: result.git,
        entries: {},
        expanded: {},
        loading: {},
        selected: null,
        watcherAvailable: result.watch,
      });
      await get().loadDir("", true);
      return result;
    } catch (error) {
      useUiStore.getState().toast("error", errorText(error));
      return null;
    }
  },

  closeWorkspace: async () => {
    try {
      await api.closeWorkspace();
    } catch (error) {
      useUiStore.getState().toast("error", errorText(error));
    }
    set({
      root: null,
      name: "",
      git: null,
      entries: {},
      expanded: {},
      loading: {},
      selected: null,
      watcherAvailable: false,
    });
  },

  loadDir: async (path, force = false) => {
    const state = get();
    if (state.root === null) return;
    if (state.loading[path]) return;
    if (!force && path in state.entries) return;
    set((current) => ({ loading: { ...current.loading, [path]: true } }));
    try {
      const listing = await api.listDir(path);
      set((current) => ({
        entries: { ...current.entries, [path]: listing.entries },
        loading: { ...current.loading, [path]: false },
      }));
      if (listing.truncated) {
        useUiStore.getState().toast("warning", `Directory is too large to list fully: ${path || "."}`);
      }
    } catch (error) {
      set((current) => {
        const loading = { ...current.loading };
        delete loading[path];
        return { loading };
      });
      if (error instanceof AnvilError && error.code === -32003) {
        set((current) => {
          const entries = { ...current.entries };
          delete entries[path];
          return { entries };
        });
      } else {
        useUiStore.getState().toast("error", errorText(error));
      }
    }
  },

  toggleDir: async (path) => {
    const state = get();
    if (state.expanded[path]) {
      set({ expanded: { ...state.expanded, [path]: false } });
      return;
    }
    set({ expanded: { ...state.expanded, [path]: true } });
    await get().loadDir(path);
  },

  refreshAll: async () => {
    const state = get();
    if (state.root === null) return;
    const dirs = ["", ...Object.keys(state.entries)].filter((dir) => dir in state.entries);
    await Promise.all(dirs.map((dir) => get().loadDir(dir, true)));
  },

  refreshGit: async () => {
    if (get().root === null) return;
    try {
      set({ git: await api.gitBranch() });
    } catch {
      // A non-Git workspace is not an error worth interrupting the user for.
    }
  },

  select: (selected) => set({ selected }),

  collapseAll: () => set({ expanded: {} }),

  onFsChanged: (changes) => {
    if (get().root === null || changes.length === 0) return;
    const state = get();
    let entries = state.entries;
    let expanded = state.expanded;
    let selected = state.selected;

    for (const change of changes) {
      if (change.kind === "deleted" || change.kind === "renamed") {
        const removed = removeSubtree(entries, expanded, change.path);
        entries = removed.entries;
        expanded = removed.expanded;
        if (selected === change.path || (selected !== null && selected.startsWith(`${change.path}/`))) {
          selected = null;
        }
      }
      const parent = dirname(change.path);
      if (parent in entries) pendingRefresh.add(parent);
      if (change.kind === "created" && change.path in entries) pendingRefresh.add(change.path);
    }
    set({ entries, expanded, selected });

    if (pendingRefresh.size > 0 && refreshTimer === 0) {
      refreshTimer = window.setTimeout(() => {
        refreshTimer = 0;
        const directories = [...pendingRefresh];
        pendingRefresh.clear();
        for (const directory of directories) {
          if (directory in get().entries) void get().loadDir(directory, true);
        }
      }, 120);
    }
  },

  createEntry: async (directory, name, kind) => {
    if (!isValidEntryName(name)) {
      useUiStore.getState().toast("error", "Invalid file or folder name");
      return null;
    }
    const path = joinPath(directory, name);
    try {
      await api.createEntry(path, kind);
      await get().loadDir(directory, true);
      return path;
    } catch (error) {
      useUiStore.getState().toast("error", errorText(error));
      return null;
    }
  },

  renameEntry: async (path, newName) => {
    if (!isValidEntryName(newName)) {
      useUiStore.getState().toast("error", "Invalid file or folder name");
      return null;
    }
    const parent = dirname(path);
    const newPath = joinPath(parent, newName);
    try {
      await api.renameEntry(path, newPath);
      set((current) => {
        const removed = removeSubtree(current.entries, current.expanded, path);
        return {
          entries: removed.entries,
          expanded: removed.expanded,
          selected: current.selected === path ? newPath : current.selected,
        };
      });
      await get().loadDir(parent, true);
      return newPath;
    } catch (error) {
      useUiStore.getState().toast("error", errorText(error));
      return null;
    }
  },

  deleteEntry: async (path, recursive) => {
    try {
      await api.deleteEntry(path, recursive);
      set((current) => {
        const removed = removeSubtree(current.entries, current.expanded, path);
        return {
          entries: removed.entries,
          expanded: removed.expanded,
          selected: current.selected === path ? null : current.selected,
        };
      });
      await get().loadDir(dirname(path), true);
      return true;
    } catch (error) {
      useUiStore.getState().toast("error", errorText(error));
      return false;
    }
  },

  searchFiles: async (query, limit = 200) => {
    const trimmed = query.trim();
    if (get().root === null || trimmed.length === 0) return [];
    try {
      const result = await api.searchFiles(trimmed, limit);
      return result.results;
    } catch (error) {
      useUiStore.getState().toast("error", errorText(error));
      return [];
    }
  },

  setRecent: (recent) => set({ recent }),
  setWatcherAvailable: (watcherAvailable) => set({ watcherAvailable }),
  reset: () =>
    set({
      root: null,
      name: "",
      git: null,
      entries: {},
      expanded: {},
      loading: {},
      selected: null,
      watcherAvailable: false,
    }),
}));

export function pathLabel(path: string): string {
  return basename(path) || path;
}
