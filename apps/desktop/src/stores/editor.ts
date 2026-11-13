import { create } from "zustand";
import { api } from "../ipc/api";
import { AnvilError } from "../ipc/bridge";
import type { FsChange } from "../ipc/types";
import { basename } from "../lib/paths";
import {
  applyDiagnostics,
} from "../editor/markers";
import {
  beginProgrammaticChange,
  disposeAllModels,
  disposeModel,
  endProgrammaticChange,
  getModel,
  getOrCreateModel,
} from "../editor/models";
import { forgetDocVersion, resetDocVersion } from "../editor/docVersions";
import { forgetTextSync, resetTextSync } from "../editor/textSync";
import { useLspStore } from "./lsp";
import { useUiStore } from "./ui";
import { t } from "../i18n";
import { useWorkspaceStore } from "./workspace";

export interface EditorTab {
  path: string;
  name: string;
  language: string;
  mtime: number;
  dirty: boolean;
  externalChange: boolean;
  deletedOnDisk: boolean;
}

export interface RevealTarget {
  path: string;
  line: number;
  character: number;
}

interface OpenFileOptions {
  line?: number;
  character?: number;
  reveal?: boolean;
  activate?: boolean;
}

interface EditorState {
  tabs: EditorTab[];
  activePath: string | null;
  cursor: { line: number; column: number };
  eol: "LF" | "CRLF";
  revealTarget: RevealTarget | null;
  openFile: (path: string, options?: OpenFileOptions) => Promise<boolean>;
  activate: (path: string) => void;
  closeFile: (path: string, force?: boolean) => Promise<boolean>;
  saveFile: (path: string, force?: boolean) => Promise<boolean>;
  saveAll: () => Promise<boolean>;
  reloadFromDisk: (path: string, silent?: boolean) => Promise<void>;
  markDirty: (path: string) => void;
  markSaved: (path: string, mtime: number) => void;
  setExternalChange: (path: string, external: boolean) => void;
  onFsChanged: (changes: FsChange[]) => void;
  handleRenamed: (oldPath: string, newPath: string) => Promise<void>;
  restoreState: (tabs: string[], active: string) => Promise<void>;
  requestReveal: (path: string, line: number, character: number) => void;
  clearReveal: () => void;
  setCursor: (line: number, column: number) => void;
  setEol: (eol: "LF" | "CRLF") => void;
  reset: () => void;
}

let saveStateTimer = 0;

function errorText(error: unknown): string {
  if (error instanceof AnvilError || error instanceof Error) return error.message;
  return t("error.editor");
}

function activeTabOf(state: EditorState): EditorTab | undefined {
  return state.tabs.find((tab) => tab.path === state.activePath);
}

export const useEditorStore = create<EditorState>((set, get) => ({
  tabs: [],
  activePath: null,
  cursor: { line: 1, column: 1 },
  eol: "LF",
  revealTarget: null,

  openFile: async (path, options = {}) => {
    const state = get();
    const existing = state.tabs.find((tab) => tab.path === path);
    if (existing) {
      if (options.activate !== false) set({ activePath: path });
      if (options.reveal !== false && options.line !== undefined) {
        get().requestReveal(path, options.line, options.character ?? 1);
      }
      scheduleSaveState();
      return true;
    }
    const root = useWorkspaceStore.getState().root;
    if (root === null) {
      useUiStore.getState().toast("error", t("action.openFolderBeforeFile"));
      return false;
    }
    let result;
    try {
      result = await api.readFile(path);
    } catch (error) {
      useUiStore.getState().toast("error", errorText(error));
      return false;
    }

    getOrCreateModel(path, root, result.content, result.language);
    const tab: EditorTab = {
      path,
      name: basename(path),
      language: result.language,
      mtime: result.mtime,
      dirty: false,
      externalChange: false,
      deletedOnDisk: false,
    };
    set((current) => ({
      tabs: [...current.tabs, tab],
      activePath: options.activate === false ? current.activePath : path,
    }));
    resetDocVersion(path, 1);
    resetTextSync(path, result.content);
    const diagnostics = useLspStore.getState().diagnostics[path];
    if (diagnostics) applyDiagnostics(path, diagnostics);
    if (options.reveal !== false && options.line !== undefined) {
      get().requestReveal(path, options.line, options.character ?? 1);
    }
    if (options.activate !== false) {
      const activeModel = getModel(path);
      if (activeModel) set({ eol: activeModel.getEOL() === "\r\n" ? "CRLF" : "LF" });
    }

    if (result.language === "cpp" || result.language === "c") {
      void api.lspDidOpen(path, result.language, result.content, 1).catch((error: unknown) => {
        useLspStore.getState().handleFailure(error);
      });
    }
    scheduleSaveState();
    return true;
  },

  activate: (path) => {
    if (!get().tabs.some((tab) => tab.path === path)) return;
    const model = getModel(path);
    set({
      activePath: path,
      eol: model ? (model.getEOL() === "\r\n" ? "CRLF" : "LF") : get().eol,
    });
    scheduleSaveState();
  },

  closeFile: async (path, force = false) => {
    const state = get();
    const tab = state.tabs.find((item) => item.path === path);
    if (!tab) return true;
    if (tab.dirty && !force) {
      useUiStore.getState().showConfirm({
        title: t("dialog.unsavedTitle"),
        message: t("dialog.saveChangesBeforeClosing", { name: tab.name }),
        confirmLabel: t("dialog.closeWithoutSaving"),
        danger: true,
        onConfirm: () => {
          void get().closeFile(path, true);
        },
      });
      return false;
    }
    const index = state.tabs.findIndex((item) => item.path === path);
    const tabs = state.tabs.filter((item) => item.path !== path);
    let activePath = state.activePath;
    if (activePath === path) {
      activePath = tabs[Math.min(index, tabs.length - 1)]?.path ?? null;
    }
    set({ tabs, activePath });
    disposeModel(path);
    forgetDocVersion(path);
    forgetTextSync(path);
    void api.lspDidClose(path).catch(() => undefined);
    scheduleSaveState();
    return true;
  },

  saveFile: async (path, force = false) => {
    const state = get();
    const tab = state.tabs.find((item) => item.path === path);
    const model = getModel(path);
    if (!tab || !model) return false;
    if (!tab.dirty && !force) return true;
    try {
      const result = await api.writeFile(path, model.getValue(), tab.mtime, force);
      set((current) => ({
        tabs: current.tabs.map((item) =>
          item.path === path
            ? {
                ...item,
                mtime: result.mtime,
                dirty: false,
                externalChange: false,
                deletedOnDisk: false,
              }
            : item,
        ),
      }));
      void api.lspDidSave(path).catch(() => undefined);
      return true;
    } catch (error) {
      if (error instanceof AnvilError && error.code === -32008) {
        get().setExternalChange(path, true);
        useUiStore
          .getState()
          .toast("warning", t("action.fileChangedOnDisk"));
      } else {
        useUiStore.getState().toast("error", errorText(error));
      }
      return false;
    }
  },

  saveAll: async () => {
    const dirty = get().tabs.filter((tab) => tab.dirty);
    let ok = true;
    for (const tab of dirty) {
      const saved = await get().saveFile(tab.path);
      ok = saved && ok;
    }
    return ok;
  },

  reloadFromDisk: async (path, silent = false) => {
    const tab = get().tabs.find((item) => item.path === path);
    const model = getModel(path);
    if (!tab || !model) return;
    try {
      const result = await api.readFile(path);
      beginProgrammaticChange();
      try {
        const viewState = model.getEOL();
        model.setValue(result.content);
        if (result.content.includes("\r\n") && viewState !== "\r\n") {
          model.pushEOL(1);
        }
      } finally {
        endProgrammaticChange();
      }
      set((current) => ({
        tabs: current.tabs.map((item) =>
          item.path === path
            ? {
                ...item,
                mtime: result.mtime,
                dirty: false,
                externalChange: false,
                deletedOnDisk: false,
                language: result.language,
              }
            : item,
        ),
      }));
      resetDocVersion(path, 1);
      resetTextSync(path, result.content);
      void api.lspDidClose(path).catch(() => undefined);
      void api.lspDidOpen(path, result.language, result.content, 1).catch((error: unknown) => {
        useLspStore.getState().handleFailure(error);
      });
      const diagnostics = useLspStore.getState().diagnostics[path];
      if (diagnostics) applyDiagnostics(path, diagnostics);
      if (!silent) useUiStore.getState().toast("info", `Reloaded ${tab.name} from disk`);
    } catch (error) {
      useUiStore.getState().toast("error", errorText(error));
    }
  },

  markDirty: (path) =>
    set((state) => ({
      tabs: state.tabs.map((tab) => (tab.path === path ? { ...tab, dirty: true } : tab)),
    })),

  markSaved: (path, mtime) =>
    set((state) => ({
      tabs: state.tabs.map((tab) =>
        tab.path === path ? { ...tab, mtime, dirty: false, externalChange: false } : tab,
      ),
    })),

  setExternalChange: (path, external) =>
    set((state) => ({
      tabs: state.tabs.map((tab) =>
        tab.path === path ? { ...tab, externalChange: external } : tab,
      ),
    })),

  onFsChanged: (changes) => {
    void (async () => {
      for (const change of changes) {
        const tab = get().tabs.find((item) => item.path === change.path);
        if (!tab) continue;
        if (change.kind === "deleted") {
          set((state) => ({
            tabs: state.tabs.map((item) =>
              item.path === change.path
                ? { ...item, externalChange: true, deletedOnDisk: true }
                : item,
            ),
          }));
          useUiStore.getState().toast("warning", `${tab.name} was deleted on disk`);
          continue;
        }
        if (change.kind === "renamed") {
          get().setExternalChange(change.path, true);
          continue;
        }
        try {
          const stat = await api.stat(change.path);
          if (stat.mtime === tab.mtime) continue;
          if (tab.dirty) {
            get().setExternalChange(change.path, true);
          } else {
            await get().reloadFromDisk(change.path, true);
          }
        } catch {
          // A transient stat failure is not enough to disturb the editor.
        }
      }
    })();
  },

  handleRenamed: async (oldPath, newPath) => {
    const tab = get().tabs.find((item) => item.path === oldPath);
    if (!tab) return;
    await get().closeFile(oldPath, true);
    await get().openFile(newPath);
  },

  restoreState: async (tabs, active) => {
    const unique = Array.from(new Set(tabs)).slice(0, 20);
    for (const path of unique) {
      await get().openFile(path, { activate: false });
    }
    if (active && get().tabs.some((tab) => tab.path === active)) {
      get().activate(active);
    } else if (get().tabs.length > 0) {
      get().activate(get().tabs[0]!.path);
    }
  },

  requestReveal: (path, line, character) => set({ revealTarget: { path, line, character } }),
  clearReveal: () => set({ revealTarget: null }),
  setCursor: (line, column) => set({ cursor: { line, column } }),
  setEol: (eol) => set({ eol }),

  reset: () => {
    disposeAllModels();
    set({
      tabs: [],
      activePath: null,
      cursor: { line: 1, column: 1 },
      eol: "LF",
      revealTarget: null,
    });
  },
}));

export function activeEditorTab(): EditorTab | undefined {
  return activeTabOf(useEditorStore.getState());
}

function scheduleSaveState(): void {
  if (saveStateTimer !== 0) window.clearTimeout(saveStateTimer);
  saveStateTimer = window.setTimeout(() => {
    saveStateTimer = 0;
    const state = useEditorStore.getState();
    if (useWorkspaceStore.getState().root === null) return;
    void api
      .saveWorkspaceState(
        state.tabs.map((tab) => tab.path),
        state.activePath ?? "",
      )
      .catch(() => undefined);
  }, 450);
}
