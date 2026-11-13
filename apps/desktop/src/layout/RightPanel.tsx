import { useEffect, useMemo, useRef } from "react";
import { clearOutput } from "../actions";
import {
  CheckIcon,
  CloseIcon,
  ErrorIcon,
  OutputIcon,
  ProblemsIcon,
  TerminalIcon,
  BugIcon,
  WarningIcon,
} from "../components/Icons";
import { useCoreStore } from "../stores/core";
import { useEditorStore } from "../stores/editor";
import { problemCounts, useLspStore } from "../stores/lsp";
import { useUiStore, type PanelTab } from "../stores/ui";
import { basename } from "../lib/paths";
import type { MessageKey } from "../i18n";
import { useI18n } from "../i18n";

const tabs: Array<{ id: PanelTab; labelKey: MessageKey; icon: typeof ProblemsIcon }> = [
  { id: "problems", labelKey: "panel.problems", icon: ProblemsIcon },
  { id: "output", labelKey: "panel.output", icon: OutputIcon },
  { id: "terminal", labelKey: "panel.terminal", icon: TerminalIcon },
  { id: "debug", labelKey: "panel.debug", icon: BugIcon },
];

export function RightPanel() {
  const { locale, t } = useI18n();
  const activeTab = useUiStore((state) => state.panelTab);
  const diagnostics = useLspStore((state) => state.diagnostics);
  const logs = useCoreStore((state) => state.logs);
  const counts = problemCounts(diagnostics);
  const outputRef = useRef<HTMLDivElement | null>(null);
  const setPanelTab = useUiStore((state) => state.showPanel);
  const togglePanel = useUiStore((state) => state.togglePanel);

  const problems = useMemo(() => {
    const files = Object.entries(diagnostics)
      .filter(([, list]) => list.length > 0)
      .sort(([a], [b]) => a.localeCompare(b));
    return files;
  }, [diagnostics]);

  useEffect(() => {
    if (activeTab === "output" && outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [activeTab, logs]);

  return (
    <aside className="panel" aria-label={t("panel.label")}>
      <div className="panel-tabs" role="tablist">
        {tabs.map((tab) => {
          const Icon = tab.icon;
          return (
            <button
              type="button"
              key={tab.id}
              role="tab"
              aria-selected={activeTab === tab.id}
              className={`panel-tab${activeTab === tab.id ? " active" : ""}`}
              onClick={() => setPanelTab(tab.id)}
            >
              <Icon size={13} />
              {t(tab.labelKey)}
              {tab.id === "problems" && counts.errors + counts.warnings > 0 ? (
                <span className="panel-badge">{counts.errors + counts.warnings}</span>
              ) : null}
            </button>
          );
        })}
        <div className="panel-tab-spacer" />
        <button
          type="button"
          className="icon-button panel-close"
          title={t("panel.hide")}
          aria-label={t("panel.hide")}
          onClick={togglePanel}
        >
          <CloseIcon size={12} />
        </button>
      </div>
      <div className="panel-content active" role="tabpanel">
        {activeTab === "problems" ? (
          <div className="problems-panel">
            {problems.length === 0 ? (
              <div className="panel-empty">
                <CheckIcon size={22} />
                <p>{t("panel.noProblemsTitle")}</p>
                <span className="muted">{t("panel.noProblemsNote")}</span>
              </div>
            ) : (
              problems.map(([path, list]) => (
                <div className="problem-group" key={path}>
                  <div className="problem-file" title={path}>
                    <span>{basename(path)}</span>
                    <span className="problem-file-path">{path}</span>
                    <span className="problem-file-count">{list.length}</span>
                  </div>
                  {list.map((diagnostic, index) => {
                    const severity = diagnostic.severity ?? 3;
                    return (
                      <button
                        type="button"
                        className={`problem-item severity-${severity}`}
                        key={`${path}-${index}`}
                        onClick={() => {
                          void useEditorStore.getState().openFile(path, {
                            line: diagnostic.range.start.line + 1,
                            character: diagnostic.range.start.character + 1,
                          });
                        }}
                      >
                        <span className="problem-severity">
                          {severity === 1 ? (
                            <ErrorIcon size={13} />
                          ) : severity === 2 ? (
                            <WarningIcon size={13} />
                          ) : (
                            <ProblemsIcon size={13} />
                          )}
                        </span>
                        <span className="problem-message">{diagnostic.message}</span>
                        <span className="problem-loc">
                          {diagnostic.range.start.line + 1}:{diagnostic.range.start.character + 1}
                        </span>
                      </button>
                    );
                  })}
                </div>
              ))
            )}
          </div>
        ) : null}
        {activeTab === "output" ? (
          <div className="output-panel" ref={outputRef}>
            <div className="output-toolbar">
              <span>{t("panel.outputChannel")}</span>
              <button type="button" className="text-button" onClick={clearOutput}>
                {t("common.clear")}
              </button>
            </div>
            {logs.length === 0 ? (
              <div className="panel-empty">
                <OutputIcon size={22} />
                <p>{t("panel.noOutput")}</p>
                <span className="muted">{t("panel.outputNote")}</span>
              </div>
            ) : (
              <div className="output-lines" aria-live="polite">
                {logs.map((line) => (
                  <div className="output-line" key={line.id}>
                    <span className="output-time">
                      {new Date(line.time).toLocaleTimeString(
                        locale === "ru" ? "ru-RU" : "en-US",
                        {
                          hour: "2-digit",
                          minute: "2-digit",
                          second: "2-digit",
                        },
                      )}
                    </span>
                    <span className={`output-source ${line.source}`}>{line.source}</span>
                    <span className="output-text">{line.text}</span>
                  </div>
                ))}
              </div>
            )}
          </div>
        ) : null}
        {activeTab === "terminal" ? (
          <div className="panel-empty wide">
            <TerminalIcon size={24} />
            <p>{t("panel.terminalPhase3")}</p>
            <span className="muted">{t("panel.terminalNote")}</span>
          </div>
        ) : null}
        {activeTab === "debug" ? (
          <div className="panel-empty wide">
            <BugIcon size={24} />
            <p>{t("panel.debugPhase3")}</p>
            <span className="muted">{t("panel.debugNote")}</span>
          </div>
        ) : null}
      </div>
    </aside>
  );
}
