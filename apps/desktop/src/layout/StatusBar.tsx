import { openSecurity, showActivity, showPanel } from "../actions";
import {
  BranchIcon,
  ErrorIcon,
  InfoIcon,
  LockIcon,
  WarningIcon,
} from "../components/Icons";
import { useCoreStore } from "../stores/core";
import { useEditorStore } from "../stores/editor";
import { useLspStore, problemCounts } from "../stores/lsp";
import { useSettingsStore } from "../stores/settings";
import { useUiStore } from "../stores/ui";
import { useWorkspaceStore } from "../stores/workspace";

function languageLabel(language: string | undefined): string {
  switch (language) {
    case "cpp":
      return "C++";
    case "c":
      return "C";
    case "python":
      return "Python";
    case "rust":
      return "Rust";
    case "json":
      return "JSON";
    case "markdown":
      return "Markdown";
    case "yaml":
      return "YAML";
    case "javascript":
      return "JavaScript";
    case "typescript":
      return "TypeScript";
    case "shell":
      return "Shell Script";
    case "cmake":
      return "CMake";
    default:
      return "Plain Text";
  }
}

export function StatusBar() {
  const git = useWorkspaceStore((state) => state.git);
  const cursor = useEditorStore((state) => state.cursor);
  const eol = useEditorStore((state) => state.eol);
  const activePath = useEditorStore((state) => state.activePath);
  const tabs = useEditorStore((state) => state.tabs);
  const tabSize = useSettingsStore((state) => state.values["editor.tabSize"]);
  const diagnostics = useLspStore((state) => state.diagnostics);
  const lspStatus = useLspStore((state) => state.statuses["cpp"]);
  const progress = useLspStore((state) => state.progress);
  const sandbox = useCoreStore((state) => state.security?.sandbox ?? state.info?.security.sandbox);
  const status = useCoreStore((state) => state.status);
  const showPanelTab = useUiStore((state) => state.showPanel);
  const counts = problemCounts(diagnostics);
  const activeTab = tabs.find((tab) => tab.path === activePath);
  const branch = git?.repository ? git.branch || "detached" : "";

  const lspState = lspStatus?.state ?? "idle";
  const lspLabel =
    lspState === "running"
      ? "clangd"
      : lspState === "starting"
        ? "clangd starting"
        : lspState === "error"
          ? "clangd error"
          : "clangd idle";
  const indexing = progress?.percentage !== undefined ? ` · indexing ${Math.round(progress.percentage)}%` : "";

  return (
    <footer className="statusbar" aria-label="Status bar">
      <button
        type="button"
        className="status-item"
        title={git?.repository ? `Git branch: ${branch}` : "No Git repository"}
        onClick={() => showActivity("scm")}
      >
        <BranchIcon size={13} />
        {branch || "no repository"}
      </button>
      <button
        type="button"
        className="status-item"
        title="Show problems"
        onClick={() => showPanel("problems")}
      >
        <ErrorIcon size={12} />
        {counts.errors}
        <WarningIcon size={12} />
        {counts.warnings}
      </button>
      <div className="status-spacer" />
      {status !== "ready" ? (
        <span className={`status-item status-${status}`} title="Anvil core status">
          core {status}
        </span>
      ) : null}
      <button
        type="button"
        className={`status-item lsp-status ${lspState}`}
        title={
          lspStatus?.message
            ? lspStatus.message
            : lspStatus
              ? `${lspStatus.server || "clangd"} ${lspStatus.version || ""}`
              : "C++ language server is idle"
        }
        onClick={() => {
          if (lspState === "error" && lspStatus?.message) {
            useUiStore.getState().toast("warning", lspStatus.message);
          }
          showPanelTab("output");
        }}
      >
        <span className="status-dot" />
        {lspLabel}
        {indexing}
      </button>
      <button
        type="button"
        className="status-item"
        title={
          sandbox
            ? sandbox.available
              ? "Language servers are sandboxed with bubblewrap and seccomp"
              : `Sandbox unavailable: ${sandbox.reason}`
            : "Security status unknown"
        }
        onClick={openSecurity}
      >
        <LockIcon size={12} />
        {sandbox?.available ? "sandboxed" : "sandbox off"}
      </button>
      <span className="status-item" title="Cursor position">
        Ln {cursor.line}, Col {cursor.column}
      </span>
      <span className="status-item" title="Indentation">
        Spaces: {tabSize}
      </span>
      <span className="status-item" title="End-of-line sequence">
        {eol}
      </span>
      <span className="status-item" title="Encoding">
        UTF-8
      </span>
      <span className="status-item" title="Language mode">
        {activeTab ? languageLabel(activeTab.language) : "Anvil"}
      </span>
      <button
        type="button"
        className="status-item status-info"
        title="About Anvil"
        onClick={() => useUiStore.getState().setAboutOpen(true)}
      >
        <InfoIcon size={12} />
      </button>
    </footer>
  );
}
