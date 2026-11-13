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
import { useI18n, type MessageKey } from "../i18n";

function languageLabel(
  language: string | undefined,
  t: (key: MessageKey) => string,
): string {
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
      return t("language.shellScript");
    case "cmake":
      return "CMake";
    default:
      return t("language.plainText");
  }
}

export function StatusBar() {
  const { t } = useI18n();
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
        ? t("status.clangdStarting")
        : lspState === "error"
          ? t("status.clangdError")
          : t("status.clangdIdle");
  const indexing =
    progress?.percentage !== undefined
      ? t("status.indexing", { percent: Math.round(progress.percentage) })
      : "";

  return (
    <footer className="statusbar" aria-label={t("status.label")}>
      <button
        type="button"
        className="status-item"
        title={
          git?.repository
            ? t("status.gitBranch", { branch })
            : t("status.noRepository")
        }
        onClick={() => showActivity("scm")}
      >
        <BranchIcon size={13} />
        {branch || t("status.noRepositoryShort")}
      </button>
      <button
        type="button"
        className="status-item"
        title={t("status.showProblems")}
        onClick={() => showPanel("problems")}
      >
        <ErrorIcon size={12} />
        {counts.errors}
        <WarningIcon size={12} />
        {counts.warnings}
      </button>
      <div className="status-spacer" />
      {status !== "ready" ? (
        <span className={`status-item status-${status}`} title={t("status.coreStatus")}>
          {t("status.core", { status })}
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
              : t("status.lspIdle")
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
              ? t("status.sandboxOk")
              : t("status.sandboxUnavailable", { reason: sandbox.reason })
            : t("status.securityUnknown")
        }
        onClick={openSecurity}
      >
        <LockIcon size={12} />
        {sandbox?.available ? t("status.sandboxed") : t("status.sandboxOff")}
      </button>
      <span className="status-item" title={t("status.cursorTitle")}>
        {t("status.cursor", { line: cursor.line, column: cursor.column })}
      </span>
      <span className="status-item" title={t("status.indentationTitle")}>
        {t("status.spaces", { count: tabSize })}
      </span>
      <span className="status-item" title={t("status.eolTitle")}>
        {eol}
      </span>
      <span className="status-item" title={t("status.encodingTitle")}>
        UTF-8
      </span>
      <span className="status-item" title={t("status.languageTitle")}>
        {activeTab ? languageLabel(activeTab.language, t) : "Anvil"}
      </span>
      <button
        type="button"
        className="status-item status-info"
        title={t("status.about")}
        onClick={() => useUiStore.getState().setAboutOpen(true)}
      >
        <InfoIcon size={12} />
      </button>
    </footer>
  );
}
