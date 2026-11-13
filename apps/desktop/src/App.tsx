import { useEffect, useRef, useState } from "react";
import { boot, nativeBridgeAvailable } from "./bootstrap";
import { bridge } from "./ipc/bridge";
import { api } from "./ipc/api";
import type {
  FsChange,
  LspDiagnostic,
  LspDiagnosticsPayload,
  LspStatusPayload,
} from "./ipc/types";
import { applyDiagnostics } from "./editor/markers";
import { applyMonacoTheme } from "./editor/theme";
import { getActiveEditor } from "./editor/instance";
import { applyAppTheme, watchSystemTheme } from "./theme/apply";
import { applyFonts } from "./theme/fonts";
import { ActivityBar } from "./layout/ActivityBar";
import { CommandPalette } from "./layout/CommandPalette";
import { EditorArea } from "./layout/EditorArea";
import { Modals } from "./layout/Modals";
import { QuickOpen } from "./layout/QuickOpen";
import { RightPanel } from "./layout/RightPanel";
import { Sidebar } from "./layout/Sidebar";
import { StatusBar } from "./layout/StatusBar";
import { TitleBar } from "./layout/TitleBar";
import { Toasts } from "./layout/Toasts";
import { useCoreStore } from "./stores/core";
import { useEditorStore } from "./stores/editor";
import { useLspStore } from "./stores/lsp";
import { useSettingsStore } from "./stores/settings";
import { useUiStore } from "./stores/ui";
import { useWorkspaceStore } from "./stores/workspace";
import { useShortcuts } from "./hooks/useShortcuts";
import { ShieldIcon, WarningIcon } from "./components/Icons";
import { requestCloseApp, restartCore } from "./actions";
import {
  I18nProvider,
  resolveLocale,
  setRuntimeLocale,
  t,
  translate,
} from "./i18n";

function asRecord(value: unknown): Record<string, unknown> | null {
  return typeof value === "object" && value !== null && !Array.isArray(value)
    ? (value as Record<string, unknown>)
    : null;
}

function messageOf(error: unknown): string {
  return error instanceof Error ? error.message : t("app.couldNotStart");
}

export function App() {
  const [initializing, setInitializing] = useState(true);
  const [initOk, setInitOk] = useState(false);
  const started = useRef(false);
  const readySent = useRef(false);
  const sidebarVisible = useUiStore((state) => state.sidebarVisible);
  const panelVisible = useUiStore((state) => state.panelVisible);
  const coreStatus = useCoreStore((state) => state.status);
  const coreError = useCoreStore((state) => state.error);
  const workspaceName = useWorkspaceStore((state) => state.name);
  const tabs = useEditorStore((state) => state.tabs);
  const activePath = useEditorStore((state) => state.activePath);
  const activeTab = tabs.find((tab) => tab.path === activePath);
  const watcherAvailable = useWorkspaceStore((state) => state.watcherAvailable);
  const workspaceRoot = useWorkspaceStore((state) => state.root);
  const settingsValues = useSettingsStore((state) => state.values);
  const themePreference = settingsValues["ui.theme"];
  const locale = resolveLocale(settingsValues["ui.language"], boot.locale);
  useShortcuts();

  useEffect(() => {
    setRuntimeLocale(locale);
    document.documentElement.lang = locale;
  }, [locale]);

  useEffect(() => {
    const apply = () => {
      const resolved = applyAppTheme(themePreference);
      applyMonacoTheme(resolved);
    };
    apply();
    if (themePreference !== "auto") return;
    return watchSystemTheme(apply);
  }, [themePreference]);

  useEffect(() => {
    applyFonts(settingsValues);
  }, [settingsValues]);

  useEffect(() => {
    if (!nativeBridgeAvailable) {
      setInitializing(false);
      return;
    }
    if (started.current) return;
    started.current = true;
    let cancelled = false;
    const unsubscribers: Array<() => void> = [];

    unsubscribers.push(
      bridge.on("lsp:status", (params) => {
        const payload = asRecord(params) as unknown as LspStatusPayload | null;
        if (payload && typeof payload.languageId === "string") {
          useLspStore.getState().setStatus(payload);
        }
      }),
    );
    unsubscribers.push(
      bridge.on("lsp:diagnostics", (params) => {
        const payload = asRecord(params) as unknown as LspDiagnosticsPayload | null;
        if (!payload || typeof payload.path !== "string") return;
        const diagnostics: LspDiagnostic[] = Array.isArray(payload.diagnostics)
          ? payload.diagnostics
          : [];
        useLspStore.getState().setDiagnostics(payload.path, diagnostics);
        applyDiagnostics(payload.path, diagnostics);
      }),
    );
    unsubscribers.push(
      bridge.on("lsp:progress", (params) => {
        const payload = asRecord(params);
        const value = asRecord(payload?.["value"]);
        const kind = typeof value?.["kind"] === "string" ? value["kind"] : "";
        if (kind === "end") {
          useLspStore.getState().setProgress(null);
        } else {
          const percentage =
            typeof value?.["percentage"] === "number" ? value["percentage"] : undefined;
          const title = typeof value?.["title"] === "string" ? value["title"] : undefined;
          useLspStore.getState().setProgress({
            languageId: typeof payload?.["languageId"] === "string" ? payload["languageId"] : "cpp",
            ...(percentage === undefined ? {} : { percentage }),
            ...(title === undefined ? {} : { title }),
          });
        }
      }),
    );
    unsubscribers.push(
      bridge.on("lsp:message", (params) => {
        const payload = asRecord(params);
        if (!payload || typeof payload["message"] !== "string") return;
        const type = typeof payload["type"] === "number" ? payload["type"] : 3;
        useUiStore
          .getState()
          .toast(type === 1 ? "error" : type === 2 ? "warning" : "info", payload["message"]);
      }),
    );
    unsubscribers.push(
      bridge.on("lsp:log", (params) => {
        const payload = asRecord(params);
        if (payload && typeof payload["line"] === "string") {
          useCoreStore.getState().addLog("clangd", payload["line"]);
        }
      }),
    );
    unsubscribers.push(
      bridge.on("fs:changed", (params) => {
        const payload = asRecord(params);
        const changes = payload?.["changes"];
        if (!Array.isArray(changes)) return;
        useWorkspaceStore.getState().onFsChanged(changes as FsChange[]);
        useEditorStore.getState().onFsChanged(changes as FsChange[]);
      }),
    );
    unsubscribers.push(
      bridge.on("fs:rescan", () => {
        void useWorkspaceStore.getState().refreshAll();
        useUiStore.getState().toast("warning", t("app.watcherOverflow"));
      }),
    );
    unsubscribers.push(
      bridge.on("fs:watchUnavailable", (params) => {
        const payload = asRecord(params);
        useWorkspaceStore.getState().setWatcherAvailable(false);
        useUiStore
          .getState()
          .toast(
            "warning",
            typeof payload?.["reason"] === "string"
              ? payload["reason"]
              : t("app.watcherUnavailable"),
          );
      }),
    );
    unsubscribers.push(
      bridge.on("core:log", (params) => {
        const payload = asRecord(params);
        if (payload && typeof payload["line"] === "string") {
          useCoreStore.getState().addLog("core", payload["line"]);
        }
      }),
    );
    unsubscribers.push(
      bridge.on("core:error", (params) => {
        const payload = asRecord(params);
        const message =
          payload && typeof payload["message"] === "string"
            ? payload["message"]
            : t("app.coreReportedError");
        useCoreStore.getState().setStatus("error", message);
        useUiStore.getState().toast("error", message);
      }),
    );
    unsubscribers.push(
      bridge.on("core:exited", (params) => {
        const payload = asRecord(params);
        const detail =
          payload && typeof payload["code"] === "number"
            ? `exit ${payload["code"]}`
            : payload && typeof payload["signal"] === "number"
              ? `signal ${payload["signal"]}`
              : "unexpected exit";
        const stopped = t("app.coreStopped", { detail });
        useCoreStore.getState().setStatus("error", stopped);
        useUiStore.getState().toast("error", stopped);
      }),
    );
    unsubscribers.push(
      bridge.on("core:overloaded", () => {
        useUiStore.getState().toast("warning", t("app.coreBusy"));
      }),
    );
    unsubscribers.push(
      bridge.on("core:protocolError", (params) => {
        const payload = asRecord(params);
        useUiStore
          .getState()
          .toast("error", typeof payload?.["message"] === "string" ? payload["message"] : t("app.protocolError"));
      }),
    );
    unsubscribers.push(
      bridge.on("ui:processTerminated", () => {
        useUiStore.getState().toast("error", t("app.webProcessTerminated"));
      }),
    );
    unsubscribers.push(
      bridge.on("shell:closeRequested", () => {
        void requestCloseApp();
      }),
    );

    void (async () => {
      try {
        const info = await api.systemInfo();
        if (cancelled) return;
        useCoreStore.getState().setInfo(info);
        useSettingsStore.getState().setFrom(info.settings);
        useWorkspaceStore.getState().setRecent(info.recent);
        useCoreStore.getState().setStatus("ready");
        void api
          .securityStatus()
          .then((security) => {
            if (!cancelled) useCoreStore.getState().setSecurity(security);
          })
          .catch(() => {
            // The About/Security views fall back to the slimmer info payload.
          });

        if (boot.initialWorkspace) {
          const result = await useWorkspaceStore.getState().openWorkspace(boot.initialWorkspace);
          if (result) {
            useSettingsStore.getState().setFrom(result.settings);
            useLspStore.getState().reset();
            useEditorStore.getState().reset();
            await useEditorStore
              .getState()
              .restoreState(result.state.tabs, result.state.active);
            if (boot.e2eFile) {
              await useEditorStore.getState().openFile(boot.e2eFile);
            }
          }
        }
        if (cancelled) return;
        setInitOk(true);
        setInitializing(false);
      } catch (error) {
        if (cancelled) return;
        const message = messageOf(error);
        useCoreStore.getState().setStatus("error", message);
        useUiStore.getState().toast("error", message);
        setInitializing(false);
        bridge.notify("shell/ready", { ok: false, error: message });
      }
    })();

    return () => {
      cancelled = true;
      for (const unsubscribe of unsubscribers) unsubscribe();
    };
  }, []);

  useEffect(() => {
    if (!nativeBridgeAvailable || initializing || !initOk || readySent.current) return;
    readySent.current = true;
    const timer = window.setTimeout(() => {
      if (boot.e2e) {
        window.__anvilTest = {
          openFile: async (path: string) => {
            await useEditorStore.getState().openFile(path);
          },
          saveAll: () => useEditorStore.getState().saveAll(),
          hover: (line: number, character: number) => {
            const editor = getActiveEditor();
            if (!editor) return;
            editor.setPosition({ lineNumber: line, column: character });
            editor.focus();
            void editor.getAction("editor.action.showHover")?.run();
          },
          getState: () => ({
            tabs: useEditorStore.getState().tabs,
            activePath: useEditorStore.getState().activePath,
            root: useWorkspaceStore.getState().root,
          }),
        };
      }
      bridge.notify("shell/ready", { ok: true });
    }, 30);
    return () => window.clearTimeout(timer);
  }, [initializing, initOk]);

  const activeName = activeTab?.name ?? "";
  useEffect(() => {
    const product = translate(locale, "app.product");
    const title = activeName
      ? translate(locale, "app.documentTitle.tab", {
          file: activeName,
          workspace: workspaceName || "Anvil",
        })
      : workspaceName
        ? translate(locale, "app.documentTitle.workspace", { workspace: workspaceName })
        : product;
    void api.shellSetTitle(title).catch(() => undefined);
  }, [activeName, workspaceName, locale]);

  if (!nativeBridgeAvailable) {
    return (
      <div className="native-required">
        <div className="welcome-logo">
          Anvil<span>IDE</span>
        </div>
        <h1>{translate(locale, "app.nativeRequiredTitle")}</h1>
        <p>{translate(locale, "app.nativeRequiredBody")}</p>
      </div>
    );
  }

  if (initializing) {
    return (
      <div className="boot-screen">
        <div className="welcome-logo">
          Anvil<span>IDE</span>
        </div>
        <div className="boot-status">
          <span className="boot-spinner" />
          {translate(locale, "app.startingCore")}
        </div>
        {coreStatus === "error" && coreError ? <div className="boot-error">{coreError}</div> : null}
      </div>
    );
  }

  return (
    <I18nProvider locale={locale}>
      <div
        className={`app${sidebarVisible ? "" : " sidebar-collapsed"}${panelVisible ? "" : " panel-collapsed"}`}
      >
        <TitleBar />
        <ActivityBar />
        <Sidebar />
        <EditorArea />
        <RightPanel />
        <StatusBar />
      </div>
      {coreStatus === "error" ? (
        <div className="core-error-banner" role="alert">
          <WarningIcon size={15} />
          <span>{coreError ?? translate(locale, "app.coreUnavailable")}</span>
          <button type="button" className="banner-action" onClick={() => void restartCore()}>
            {translate(locale, "app.restartCore")}
          </button>
          <button
            type="button"
            className="banner-action"
            onClick={() => useUiStore.getState().showPanel("output")}
          >
            {translate(locale, "app.showOutput")}
          </button>
        </div>
      ) : null}
      {!watcherAvailable && workspaceRoot ? (
        <div className="watcher-banner" role="status">
          <ShieldIcon size={13} />
          {translate(locale, "app.watcherBanner")}
        </div>
      ) : null}
      <CommandPalette />
      <QuickOpen />
      <Modals />
      <Toasts />
    </I18nProvider>
  );
}
