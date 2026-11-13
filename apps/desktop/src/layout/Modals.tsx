import { useEffect, useState, type ReactNode } from "react";
import { Modal } from "../components/Modal";
import { CheckIcon, ShieldIcon, WarningIcon } from "../components/Icons";
import { isValidEntryName } from "../lib/paths";
import { useCoreStore } from "../stores/core";
import { useEditorStore } from "../stores/editor";
import { useSettingsStore } from "../stores/settings";
import { useUiStore } from "../stores/ui";
import { api } from "../ipc/api";
import {
  closeDiscarding,
  closeSavingAll,
  restartLanguageServer,
} from "../actions";

function SettingsModal() {
  const open = useUiStore((state) => state.settingsOpen);
  const setOpen = useUiStore((state) => state.setSettingsOpen);
  const values = useSettingsStore((state) => state.values);
  const setValue = useSettingsStore((state) => state.setValue);
  const info = useCoreStore((state) => state.info);
  const security = useCoreStore((state) => state.security);
  if (!open) return null;

  return (
    <Modal
      title="Settings"
      onClose={() => setOpen(false)}
      footer={
        <button type="button" className="primary-button" onClick={() => setOpen(false)}>
          Done
        </button>
      }
    >
      <section className="settings-section">
        <h3>Editor</h3>
        <label className="setting-row">
          <span>Font size</span>
          <input
            type="number"
            min={8}
            max={32}
            value={values["editor.fontSize"]}
            onChange={(event) => {
              const value = Math.max(8, Math.min(32, Number(event.target.value) || 14));
              void setValue("editor.fontSize", value);
            }}
          />
        </label>
        <label className="setting-row">
          <span>Tab size</span>
          <input
            type="number"
            min={1}
            max={8}
            value={values["editor.tabSize"]}
            onChange={(event) => {
              const value = Math.max(1, Math.min(8, Number(event.target.value) || 4));
              void setValue("editor.tabSize", value);
            }}
          />
        </label>
        <label className="setting-row checkbox">
          <span>Word wrap</span>
          <input
            type="checkbox"
            checked={values["editor.wordWrap"]}
            onChange={(event) => void setValue("editor.wordWrap", event.target.checked)}
          />
        </label>
        <label className="setting-row checkbox">
          <span>Minimap</span>
          <input
            type="checkbox"
            checked={values["editor.minimap"]}
            onChange={(event) => void setValue("editor.minimap", event.target.checked)}
          />
        </label>
        <label className="setting-row checkbox">
          <span>Hide .git in Explorer</span>
          <input
            type="checkbox"
            checked={values["explorer.hideGit"]}
            onChange={(event) => void setValue("explorer.hideGit", event.target.checked)}
          />
        </label>
        <div className="setting-note">
          Format-on-save and other language features arrive in Phase 2. Formatting is never
          applied silently in this release.
        </div>
      </section>

      <section className="settings-section">
        <h3>C/C++ language server</h3>
        <label className="setting-row column">
          <span>clangd path</span>
          <input
            key={values["lsp.clangdPath"]}
            type="text"
            defaultValue={values["lsp.clangdPath"]}
            placeholder="clangd or /usr/bin/clangd-18"
            onBlur={(event) => {
              const value = event.target.value.trim();
              if (value && value !== values["lsp.clangdPath"]) {
                void setValue("lsp.clangdPath", value);
              }
            }}
          />
        </label>
        <label className="setting-row column">
          <span>Extra clangd arguments (one per line)</span>
          <textarea
            key={values["lsp.clangdArgs"].join("\n")}
            defaultValue={values["lsp.clangdArgs"].join("\n")}
            rows={3}
            placeholder="--background-index"
            onBlur={(event) => {
              const args = event.target.value
                .split("\n")
                .map((line) => line.trim())
                .filter(Boolean);
              if (args.join("\n") !== values["lsp.clangdArgs"].join("\n")) {
                void setValue("lsp.clangdArgs", args);
              }
            }}
          />
        </label>
        <label className="setting-row checkbox">
          <span>Read project .clangd configuration</span>
          <input
            type="checkbox"
            checked={values["lsp.enableProjectConfig"]}
            onChange={(event) => void setValue("lsp.enableProjectConfig", event.target.checked)}
          />
        </label>
        <div className="setting-note">
          Project configuration is disabled by default because .clangd is untrusted project
          input. Compile commands are always treated as data, never executed.
        </div>
        <div className="settings-inline">
          <button
            type="button"
            className="secondary-button"
            onClick={() => void restartLanguageServer()}
          >
            Restart language server
          </button>
        </div>
      </section>

      <section className="settings-section">
        <h3>Security</h3>
        <div className="settings-security">
          <ShieldIcon size={20} />
          <div>
            <strong>
              {security?.sandbox.available ?? info?.security.sandbox.available
                ? "Sandbox available"
                : "Sandbox unavailable"}
            </strong>
            <p>
              {security?.sandbox.reason ?? info?.security.sandbox.reason ?? "Probing bubblewrap…"}
            </p>
          </div>
        </div>
        <div className="setting-note">
          Unsandboxed language servers are available only in explicit developer builds. There is
          no user-facing switch in release builds.
        </div>
        <dl className="settings-paths">
          <div>
            <dt>Settings file</dt>
            <dd>{info ? `${info.configDir}/settings.json` : "unknown"}</dd>
          </div>
          <div>
            <dt>Security log</dt>
            <dd>{security?.logs.security ?? "unknown"}</dd>
          </div>
        </dl>
      </section>
    </Modal>
  );
}

function AboutModal() {
  const open = useUiStore((state) => state.aboutOpen);
  const setOpen = useUiStore((state) => state.setAboutOpen);
  const info = useCoreStore((state) => state.info);
  const security = useCoreStore((state) => state.security);
  if (!open) return null;
  return (
    <Modal
      title="About Anvil"
      onClose={() => setOpen(false)}
      footer={
        <>
          <button
            type="button"
            className="secondary-button"
            onClick={() => {
              setOpen(false);
              useUiStore.getState().setSecurityOpen(true);
            }}
          >
            Security Status
          </button>
          <button type="button" className="primary-button" onClick={() => setOpen(false)}>
            Close
          </button>
        </>
      }
    >
      <div className="about-hero">
        <div className="welcome-logo small">
          Anvil<span>IDE</span>
        </div>
        <p>Forge C++ at full speed.</p>
        <span className="version-chip">v{info?.version ?? "0.0.1"} · experimental</span>
      </div>
      <p className="about-text">
        A C++-first IDE with a native C++ core, clangd-powered intelligence, and a Monaco
        frontend inside a system WebView. No bundled Chromium, no Electron, no telemetry.
      </p>
      <dl className="about-grid">
        <div>
          <dt>Product</dt>
          <dd>{info?.product ?? "Anvil IDE"}</dd>
        </div>
        <div>
          <dt>Core</dt>
          <dd>{info ? `${info.core} ${info.version} (API ${info.apiVersion})` : "anvil-core"}</dd>
        </div>
        <div>
          <dt>Platform</dt>
          <dd>{info?.platform ?? "linux"}</dd>
        </div>
        <div>
          <dt>License</dt>
          <dd>Apache-2.0</dd>
        </div>
        <div>
          <dt>Config</dt>
          <dd>{info?.configDir ?? "~/.config/anvil"}</dd>
        </div>
        <div>
          <dt>Cache</dt>
          <dd>{info?.cacheDir ?? "~/.cache/anvil"}</dd>
        </div>
        <div>
          <dt>Security log</dt>
          <dd>{security?.logs.security ?? "not loaded"}</dd>
        </div>
        <div>
          <dt>Build</dt>
          <dd>{info?.devMode ? "development" : "release"}</dd>
        </div>
      </dl>
      <p className="about-fine">
        C++20 core · WebKitGTK shell · Monaco Editor · clangd · bubblewrap + seccomp
      </p>
    </Modal>
  );
}

function SecurityModal() {
  const open = useUiStore((state) => state.securityOpen);
  const setOpen = useUiStore((state) => state.setSecurityOpen);
  const info = useCoreStore((state) => state.info);
  const security = useCoreStore((state) => state.security);
  if (!open) return null;
  const sandbox = security?.sandbox ?? info?.security.sandbox;
  return (
    <Modal
      title="Security Status"
      onClose={() => setOpen(false)}
      footer={
        <button type="button" className="primary-button" onClick={() => setOpen(false)}>
          Close
        </button>
      }
      wide
    >
      {sandbox?.available ? (
        <div className="security-banner ok">
          <ShieldIcon size={20} />
          <div>
            <strong>Language servers are isolated</strong>
            <p>{sandbox.reason}</p>
          </div>
        </div>
      ) : (
        <div className="security-banner warning">
          <WarningIcon size={20} />
          <div>
            <strong>Sandbox is unavailable on this system</strong>
            <p>{sandbox?.reason ?? "bubblewrap could not be verified."}</p>
            <p>
              Language-server startup is blocked unless an explicit development override is
              active. Release builds never fall back silently.
            </p>
          </div>
        </div>
      )}
      <dl className="about-grid">
        <div>
          <dt>bubblewrap</dt>
          <dd>{sandbox?.bwrap || "not found"}</dd>
        </div>
        <div>
          <dt>Network namespace</dt>
          <dd>{sandbox?.networkNamespace ? "isolated" : "seccomp network deny"}</dd>
        </div>
        <div>
          <dt>Workspace root</dt>
          <dd>{security?.workspace.root || info?.workspace.root || "none"}</dd>
        </div>
        <div>
          <dt>Audit log</dt>
          <dd>{security?.logs.security ?? "not loaded"}</dd>
        </div>
      </dl>
      <ul className="security-list">
        <li>
          <CheckIcon size={13} /> Filesystem access is scoped to the open workspace root.
        </li>
        <li>
          <CheckIcon size={13} /> Processes are spawned with argv arrays; Anvil never invokes a
          shell.
        </li>
        <li>
          <CheckIcon size={13} /> Language servers run read-only against the workspace with a
          private cache.
        </li>
        <li>
          <CheckIcon size={13} /> No telemetry, no remote content, no AI provider enabled.
        </li>
        <li>
          <CheckIcon size={13} /> Secrets are never written to settings.json or logs.
        </li>
      </ul>
      <p className="setting-note">
        Residual risks: a compromised WebKit or clangd binary could affect this session; project
        build scripts are not executed in 0.0.1. Read the threat model in the installed
        documentation for details.
      </p>
    </Modal>
  );
}

function RunConfigModal() {
  const open = useUiStore((state) => state.runConfigOpen);
  const setOpen = useUiStore((state) => state.setRunConfigOpen);
  if (!open) return null;
  return (
    <Modal
      title="Run Configuration"
      onClose={() => setOpen(false)}
      footer={
        <>
          <button type="button" className="secondary-button" onClick={() => setOpen(false)}>
            Cancel
          </button>
          <button type="button" className="primary-button" disabled title="Phase 3">
            Start
          </button>
        </>
      }
      wide
    >
      <div className="roadmap-banner">
        <strong>Build and run integration arrives in Phase 3.</strong>
        <p>
          This preview shows the planned configuration surface. Anvil does not execute project
          build scripts in 0.0.1, so the Start button is disabled.
        </p>
      </div>
      <div className="form-grid">
        <label>
          <span>Target</span>
          <input disabled placeholder="Not configured" />
        </label>
        <label>
          <span>Configuration</span>
          <input disabled placeholder="Not configured" />
        </label>
        <label>
          <span>Working directory</span>
          <input disabled placeholder="Not configured" />
        </label>
        <label>
          <span>Program arguments</span>
          <input disabled placeholder="Not configured" />
        </label>
        <label className="full">
          <span>Environment</span>
          <textarea disabled rows={2} placeholder="Not configured" />
        </label>
      </div>
      <p className="setting-note">
        When enabled, builds will run only after an explicit confirmation that names the exact
        command, inside bubblewrap, with network access denied and write access limited to the
        build directory.
      </p>
    </Modal>
  );
}

function ReferencesModal() {
  const references = useUiStore((state) => state.references);
  const setReferences = useUiStore((state) => state.setReferences);
  if (!references) return null;
  const groups = new Map<string, typeof references.locations>();
  for (const location of references.locations) {
    const list = groups.get(location.path) ?? [];
    list.push(location);
    groups.set(location.path, list);
  }
  return (
    <Modal
      title={`References (${references.locations.length})`}
      onClose={() => setReferences(null)}
      footer={
        <button type="button" className="primary-button" onClick={() => setReferences(null)}>
          Close
        </button>
      }
      wide
    >
      {Array.from(groups.entries()).map(([path, locations]) => (
        <div className="reference-group" key={path}>
          <div className="reference-file">{path}</div>
          {locations.map((location, index) => (
            <button
              type="button"
              className="reference-row"
              key={`${path}-${index}`}
              onClick={() => {
                setReferences(null);
                void useEditorStore.getState().openFile(path, {
                  line: location.range.start.line + 1,
                  character: location.range.start.character + 1,
                });
              }}
            >
              <span className="reference-loc">
                {location.range.start.line + 1}:{location.range.start.character + 1}
              </span>
              <span className="reference-hint">Open location</span>
            </button>
          ))}
        </div>
      ))}
    </Modal>
  );
}

function PromptDialog() {
  const prompt = useUiStore((state) => state.prompt);
  const close = useUiStore((state) => state.closePrompt);
  const [value, setValue] = useState("");
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    setValue(prompt?.initialValue ?? "");
    setError("");
    setBusy(false);
  }, [prompt]);

  if (!prompt) return null;
  const submit = async () => {
    const trimmed = value.trim();
    if (!trimmed) {
      setError("A name is required.");
      return;
    }
    if (!isValidEntryName(trimmed)) {
      setError("Use a name without slashes, control characters, '.' or '..'.");
      return;
    }
    setBusy(true);
    try {
      await prompt.onSubmit(trimmed);
      close();
    } catch (submitError) {
      setError(submitError instanceof Error ? submitError.message : "The operation failed.");
      setBusy(false);
    }
  };
  return (
    <Modal
      title={prompt.title}
      onClose={close}
      footer={
        <>
          <button type="button" className="secondary-button" onClick={close} disabled={busy}>
            Cancel
          </button>
          <button type="button" className="primary-button" onClick={() => void submit()} disabled={busy}>
            {prompt.confirmLabel ?? "OK"}
          </button>
        </>
      }
    >
      <form
        className="prompt-form"
        onSubmit={(event) => {
          event.preventDefault();
          void submit();
        }}
      >
        <label>
          <span>{prompt.label}</span>
          <input
            value={value}
            onChange={(event) => {
              setValue(event.target.value);
              setError("");
            }}
            autoFocus
          />
        </label>
        {error ? <p className="form-error">{error}</p> : null}
        <button type="submit" hidden />
      </form>
    </Modal>
  );
}

function ConfirmDialog() {
  const confirm = useUiStore((state) => state.confirm);
  const close = useUiStore((state) => state.closeConfirm);
  const [busy, setBusy] = useState(false);
  if (!confirm) return null;
  return (
    <Modal
      title={confirm.title}
      onClose={() => {
        if (!busy) close();
      }}
      footer={
        <>
          <button
            type="button"
            className="secondary-button"
            onClick={close}
            disabled={busy}
          >
            Cancel
          </button>
          <button
            type="button"
            className={confirm.danger ? "danger-button" : "primary-button"}
            disabled={busy}
            onClick={() => {
              setBusy(true);
              void Promise.resolve(confirm.onConfirm())
                .catch((error: unknown) => {
                  useUiStore
                    .getState()
                    .toast("error", error instanceof Error ? error.message : "Operation failed");
                })
                .finally(() => {
                  setBusy(false);
                  close();
                });
            }}
          >
            {confirm.confirmLabel ?? "Confirm"}
          </button>
        </>
      }
    >
      <p className="confirm-message">{confirm.message}</p>
    </Modal>
  );
}

function CloseConfirmModal() {
  const open = useUiStore((state) => state.closeConfirmOpen);
  const setOpen = useUiStore((state) => state.setCloseConfirmOpen);
  const tabs = useEditorStore((state) => state.tabs);
  const dirty = tabs.filter((tab) => tab.dirty);
  if (!open) return null;
  return (
    <Modal
      title="Unsaved changes"
      onClose={() => setOpen(false)}
      footer={
        <>
          <button type="button" className="secondary-button" onClick={() => setOpen(false)}>
            Cancel
          </button>
          <button
            type="button"
            className="danger-button"
            onClick={() => void closeDiscarding()}
          >
            Discard and close
          </button>
          <button
            type="button"
            className="primary-button"
            onClick={() => {
              void closeSavingAll().then((closed) => {
                if (closed) setOpen(false);
              });
            }}
          >
            Save all and close
          </button>
        </>
      }
      wide
    >
      <p className="confirm-message">
        Anvil is about to close with unsaved changes in {dirty.length} editor
        {dirty.length === 1 ? "" : "s"}.
      </p>
      <ul className="dirty-list">
        {dirty.map((tab) => (
          <li key={tab.path}>{tab.path}</li>
        ))}
      </ul>
    </Modal>
  );
}

export function Modals() {
  return (
    <>
      <SettingsModal />
      <AboutModal />
      <SecurityModal />
      <RunConfigModal />
      <ReferencesModal />
      <PromptDialog />
      <ConfirmDialog />
      <CloseConfirmModal />
      <SandboxHydrator />
    </>
  );
}

function SandboxHydrator(): ReactNode {
  const info = useCoreStore((state) => state.info);
  const security = useCoreStore((state) => state.security);
  const setSecurity = useCoreStore((state) => state.setSecurity);
  useEffect(() => {
    if (!info || security) return;
    void api
      .securityStatus()
      .then(setSecurity)
      .catch(() => undefined);
  }, [info, security, setSecurity]);
  return null;
}

