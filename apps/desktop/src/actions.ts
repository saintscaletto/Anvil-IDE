import { api } from "./ipc/api";
import { AnvilError } from "./ipc/bridge";
import { getActiveEditor } from "./editor/instance";
import { getModel } from "./editor/models";
import { resetDocVersion } from "./editor/docVersions";
import { resetTextSync } from "./editor/textSync";
import { basename, dirname } from "./lib/paths";
import { useCoreStore } from "./stores/core";
import { useEditorStore } from "./stores/editor";
import { useLspStore } from "./stores/lsp";
import { useSettingsStore } from "./stores/settings";
import { useUiStore } from "./stores/ui";
import { useWorkspaceStore } from "./stores/workspace";

export function errorMessage(error: unknown): string {
  if (error instanceof AnvilError || error instanceof Error) return error.message;
  return "Unexpected Anvil error";
}

export async function openWorkspacePath(
  path: string,
  force = false,
  afterOpen?: () => Promise<void>,
): Promise<boolean> {
  const ui = useUiStore.getState();
  const workspace = useWorkspaceStore.getState();
  const editor = useEditorStore.getState();
  const settings = useSettingsStore.getState();
  const lsp = useLspStore.getState();

  if (!force && editor.tabs.some((tab) => tab.dirty) && workspace.root !== path) {
    ui.showConfirm({
      title: "Unsaved changes",
      message: `Opening ${basename(path)} will discard unsaved editors in the current folder.`,
      confirmLabel: "Open anyway",
      danger: true,
      onConfirm: async () => {
        await openWorkspacePath(path, true, afterOpen);
      },
    });
    return false;
  }

  const result = await workspace.openWorkspace(path);
  if (!result) return false;
  settings.setFrom(result.settings);
  lsp.reset();
  editor.reset();
  await editor.restoreState(result.state.tabs, result.state.active);
  if (!result.watch) {
    ui.toast("warning", "File watching is unavailable; refresh the explorer manually.");
  }
  if (afterOpen) await afterOpen();
  return true;
}

export async function openFolderDialog(): Promise<void> {
  const chosen = await api.shellOpenFolder();
  if (chosen.cancelled || !chosen.path) return;
  await openWorkspacePath(chosen.path);
}

export async function openFileDialog(): Promise<void> {
  const chosen = await api.shellOpenFile();
  if (chosen.cancelled || !chosen.path) return;
  const filePath = chosen.path;
  const root = useWorkspaceStore.getState().root;
  if (root && filePath.startsWith(`${root}/`)) {
    await useEditorStore.getState().openFile(filePath.slice(root.length + 1));
    return;
  }
  const directory = dirname(filePath);
  await openWorkspacePath(directory, false, async () => {
    await useEditorStore.getState().openFile(basename(filePath));
  });
}

export async function saveActive(): Promise<void> {
  const editor = useEditorStore.getState();
  if (!editor.activePath) {
    useUiStore.getState().toast("info", "There is no active editor to save.");
    return;
  }
  await editor.saveFile(editor.activePath);
}

export async function saveAll(): Promise<void> {
  const editor = useEditorStore.getState();
  if (editor.tabs.length === 0) {
    useUiStore.getState().toast("info", "There are no open editors to save.");
    return;
  }
  await editor.saveAll();
}

export async function closeActive(): Promise<void> {
  const editor = useEditorStore.getState();
  if (!editor.activePath) return;
  await editor.closeFile(editor.activePath);
}

export async function closeFolder(): Promise<void> {
  const editor = useEditorStore.getState();
  const workspace = useWorkspaceStore.getState();
  const ui = useUiStore.getState();
  const closeNow = async () => {
    editor.reset();
    useLspStore.getState().reset();
    await workspace.closeWorkspace();
    ui.toast("info", "Folder closed");
  };
  if (editor.tabs.some((tab) => tab.dirty)) {
    ui.showConfirm({
      title: "Unsaved changes",
      message: "This folder has unsaved editors. Close it without saving?",
      confirmLabel: "Close folder",
      danger: true,
      onConfirm: closeNow,
    });
    return;
  }
  await closeNow();
}

export async function requestCloseApp(): Promise<void> {
  const editor = useEditorStore.getState();
  const ui = useUiStore.getState();
  if (editor.tabs.some((tab) => tab.dirty)) {
    ui.setCloseConfirmOpen(true);
    return;
  }
  await api.shellClose();
}

export async function closeSavingAll(): Promise<boolean> {
  const saved = await useEditorStore.getState().saveAll();
  if (!saved) {
    useUiStore.getState().toast("error", "Some files could not be saved.");
    return false;
  }
  await api.shellClose();
  return true;
}

export async function closeDiscarding(): Promise<void> {
  await api.shellClose();
}

export async function newFile(directory = ""): Promise<void> {
  const workspace = useWorkspaceStore.getState();
  const ui = useUiStore.getState();
  if (!workspace.root) {
    ui.toast("info", "Open a folder first.");
    return;
  }
  ui.showPrompt({
    title: "New File",
    label: directory ? `Create a file in ${directory}/` : "Create a file in the workspace root",
    initialValue: "",
    confirmLabel: "Create",
    onSubmit: async (name) => {
      const path = await workspace.createEntry(directory, name, "file");
      if (path) await useEditorStore.getState().openFile(path);
    },
  });
}

export async function newFolder(directory = ""): Promise<void> {
  const workspace = useWorkspaceStore.getState();
  const ui = useUiStore.getState();
  if (!workspace.root) {
    ui.toast("info", "Open a folder first.");
    return;
  }
  ui.showPrompt({
    title: "New Folder",
    label: directory ? `Create a folder in ${directory}/` : "Create a folder in the workspace root",
    initialValue: "",
    confirmLabel: "Create",
    onSubmit: async (name) => {
      await workspace.createEntry(directory, name, "directory");
    },
  });
}

export async function renameEntry(path: string): Promise<void> {
  const workspace = useWorkspaceStore.getState();
  const ui = useUiStore.getState();
  ui.showPrompt({
    title: "Rename",
    label: "New name",
    initialValue: basename(path),
    confirmLabel: "Rename",
    onSubmit: async (name) => {
      if (name === basename(path)) return;
      const newPath = await workspace.renameEntry(path, name);
      if (newPath) {
        await useEditorStore.getState().handleRenamed(path, newPath);
      }
    },
  });
}

export function deleteEntry(path: string, isDirectory: boolean): void {
  const workspace = useWorkspaceStore.getState();
  const ui = useUiStore.getState();
  ui.showConfirm({
    title: "Delete",
    message: `Delete ${basename(path)}? This operation cannot be undone.`,
    confirmLabel: "Delete",
    danger: true,
    onConfirm: async () => {
      const deleted = await workspace.deleteEntry(path, false);
      if (!deleted && isDirectory) {
        ui.showConfirm({
          title: "Delete non-empty folder?",
          message: `${basename(path)} may not be empty. Delete it and everything inside?`,
          confirmLabel: "Delete recursively",
          danger: true,
          onConfirm: async () => {
            await workspace.deleteEntry(path, true);
          },
        });
      }
    },
  });
}

export function toggleSidebar(): void {
  useUiStore.getState().toggleSidebar();
}

export function togglePanel(): void {
  useUiStore.getState().togglePanel();
}

export function showPanel(tab: "problems" | "output" | "terminal" | "debug"): void {
  useUiStore.getState().showPanel(tab);
}

export function showActivity(activity: "explorer" | "search" | "scm" | "ai"): void {
  useUiStore.getState().setActivity(activity);
}

export function openSettings(): void {
  useUiStore.getState().setSettingsOpen(true);
}

export function openAbout(): void {
  useUiStore.getState().setAboutOpen(true);
}

export function openSecurity(): void {
  useUiStore.getState().setSecurityOpen(true);
}

export function openRunConfig(): void {
  useUiStore.getState().setRunConfigOpen(true);
}

export function openCommandPalette(): void {
  useUiStore.getState().setPaletteOpen(true);
}

export function openQuickOpen(): void {
  useUiStore.getState().setQuickOpenOpen(true);
}

export function toggleActivityPanel(): void {
  useUiStore.getState().togglePanel();
}

export function focusProblems(): void {
  useUiStore.getState().showPanel("problems");
}

export function focusOutput(): void {
  useUiStore.getState().showPanel("output");
}

export function findInFile(): void {
  const editor = getActiveEditor();
  void editor?.getAction("actions.find")?.run();
}

export function goToDefinition(): void {
  const editor = getActiveEditor();
  void editor?.getAction("editor.action.revealDefinition")?.run();
}

export async function findReferences(): Promise<void> {
  const editor = getActiveEditor();
  const model = editor?.getModel();
  if (!editor || !model) {
    useUiStore.getState().toast("info", "Open a C++ file to find references.");
    return;
  }
  const path = useEditorStore.getState().activePath;
  const position = editor.getPosition();
  if (!path || !position) return;
  try {
    const result = await api.lspReferences(path, {
      line: position.lineNumber - 1,
      character: position.column - 1,
    });
    if (result.locations.length === 0) {
      useUiStore.getState().toast("info", "No references found.");
      return;
    }
    useUiStore.getState().setReferences({ filePath: path, locations: result.locations });
  } catch (error) {
    useLspStore.getState().handleFailure(error);
  }
}

export async function restartLanguageServer(): Promise<void> {
  const editorState = useEditorStore.getState();
  try {
    await api.lspRestart("cpp");
    useLspStore.getState().reset();
    for (const tab of editorState.tabs) {
      if (tab.language !== "cpp" && tab.language !== "c") continue;
      const model = getModel(tab.path);
      if (!model) continue;
      const text = model.getValue();
      resetDocVersion(tab.path, 1);
      resetTextSync(tab.path, text);
      try {
        await api.lspDidOpen(tab.path, tab.language, text, 1);
      } catch (error) {
        useLspStore.getState().handleFailure(error);
        break;
      }
    }
    useUiStore.getState().toast("success", "Language server restarted.");
  } catch (error) {
    useUiStore.getState().toast("error", errorMessage(error));
  }
}

export async function removeRecent(path: string): Promise<void> {
  try {
    await api.removeRecent(path);
    const recent = await api.recentList();
    useWorkspaceStore.getState().setRecent(recent.recent);
  } catch (error) {
    useUiStore.getState().toast("error", errorMessage(error));
  }
}

export async function refreshExplorer(): Promise<void> {
  await useWorkspaceStore.getState().refreshAll();
  await useWorkspaceStore.getState().refreshGit();
}

export function collapseExplorer(): void {
  useWorkspaceStore.getState().collapseAll();
}

export function clearOutput(): void {
  useCoreStore.getState().clearLogs();
}

export async function reloadActiveFromDisk(): Promise<void> {
  const path = useEditorStore.getState().activePath;
  if (path) await useEditorStore.getState().reloadFromDisk(path);
}

export async function overwriteActive(): Promise<void> {
  const path = useEditorStore.getState().activePath;
  if (path) await useEditorStore.getState().saveFile(path, true);
}

export async function restartCore(): Promise<void> {
  const ui = useUiStore.getState();
  const editor = useEditorStore.getState();
  const restart = async () => {
    ui.toast("warning", "Restarting the Anvil core. Open your folder again afterwards.");
    await api.shellRestartCore();
    window.location.reload();
  };
  if (editor.tabs.some((tab) => tab.dirty)) {
    ui.showConfirm({
      title: "Restart Anvil core?",
      message: "Unsaved editors will be lost when the core restarts.",
      confirmLabel: "Restart and discard changes",
      danger: true,
      onConfirm: restart,
    });
    return;
  }
  await restart();
}
