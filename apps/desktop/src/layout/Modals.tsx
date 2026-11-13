import { useEffect, useState, type ReactNode } from "react";
import { Modal } from "../components/Modal";
import { CheckIcon, ShieldIcon, WarningIcon } from "../components/Icons";
import { isValidEntryName } from "../lib/paths";
import type { FontFamilyInfo, ThemePreference } from "../ipc/types";
import { isValidFontFamily } from "../theme/fonts";
import { tCount, useI18n, type MessageKey } from "../i18n";
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

const THEME_OPTIONS = [
  { id: "auto", labelKey: "settings.themeAuto", hintKey: "settings.themeAutoHint" },
  { id: "dark", labelKey: "settings.themeDark", hintKey: "settings.themeDarkHint" },
  { id: "light", labelKey: "settings.themeLight", hintKey: "settings.themeLightHint" },
  {
    id: "high-contrast",
    labelKey: "settings.themeHighContrast",
    hintKey: "settings.themeHighContrastHint",
  },
] as const satisfies ReadonlyArray<{
  id: ThemePreference;
  labelKey: MessageKey;
  hintKey: MessageKey;
}>;

function SettingsModal() {
  const { t } = useI18n();
  const open = useUiStore((state) => state.settingsOpen);
  const setOpen = useUiStore((state) => state.setSettingsOpen);
  const values = useSettingsStore((state) => state.values);
  const setValue = useSettingsStore((state) => state.setValue);
  const info = useCoreStore((state) => state.info);
  const security = useCoreStore((state) => state.security);
  const [fonts, setFonts] = useState<FontFamilyInfo[]>([]);

  useEffect(() => {
    if (!open) return;
    let cancelled = false;
    void api
      .shellListFonts()
      .then((result) => {
        if (!cancelled) setFonts(result.families);
      })
      .catch(() => {
        if (!cancelled) setFonts([]);
      });
    return () => {
      cancelled = true;
    };
  }, [open]);

  if (!open) return null;

  const saveFontFamily = (key: "ui.fontFamily" | "editor.fontFamily", raw: string) => {
    const value = raw.trim();
    if (!value) {
      if (values[key] !== "") void setValue(key, "");
      return;
    }
    if (!isValidFontFamily(value)) {
      useUiStore
        .getState()
        .toast("error", t("settings.fontInvalid"));
      return;
    }
    if (value === values[key]) return;
    const installed = fonts.some(
      (font) => font.family.toLocaleLowerCase() === value.toLocaleLowerCase(),
    );
    void setValue(key, value).then(() => {
      if (!installed && fonts.length > 0) {
        useUiStore
          .getState()
          .toast("warning", t("settings.fontNotInstalled", { family: value }));
      }
    });
  };

  return (
    <Modal
      title={t("settings.title")}
      onClose={() => setOpen(false)}
      footer={
        <button type="button" className="primary-button" onClick={() => setOpen(false)}>
          {t("common.done")}
        </button>
      }
    >
      <section className="settings-section">
        <h3>{t("settings.appearance")}</h3>
        <label className="setting-row">
          <span>{t("settings.language")}</span>
          <select
            value={values["ui.language"]}
            onChange={(event) =>
              void setValue(
                "ui.language",
                event.target.value as "auto" | "en" | "ru",
              )
            }
          >
            <option value="auto">{t("settings.languageAuto")}</option>
            <option value="en">{t("settings.languageEnglish")}</option>
            <option value="ru">{t("settings.languageRussian")}</option>
          </select>
        </label>
        <div className="setting-note">{t("settings.languageNote")}</div>
        <div className="setting-row column">
          <span>{t("settings.theme")}</span>
          <div className="theme-picker" role="radiogroup" aria-label={t("settings.theme")}>
            {THEME_OPTIONS.map((option) => (
              <button
                key={option.id}
                type="button"
                role="radio"
                aria-checked={values["ui.theme"] === option.id}
                className={`theme-option${values["ui.theme"] === option.id ? " selected" : ""}`}
                onClick={() => void setValue("ui.theme", option.id)}
              >
                <span className={`theme-swatch ${option.id}`} aria-hidden="true" />
                <span className="theme-option-text">
                  <strong>{t(option.labelKey)}</strong>
                  <span>{t(option.hintKey)}</span>
                </span>
              </button>
            ))}
          </div>
        </div>
        <label className="setting-row column">
          <span>{t("settings.uiFontFamily")}</span>
          <input
            key={`ui-font-${values["ui.fontFamily"]}`}
            type="text"
            list="anvil-font-families"
            defaultValue={values["ui.fontFamily"]}
            placeholder={t("settings.systemDefault")}
            onBlur={(event) => saveFontFamily("ui.fontFamily", event.target.value)}
          />
        </label>
        <label className="setting-row">
          <span>{t("settings.uiFontSize")}</span>
          <input
            type="number"
            min={11}
            max={18}
            value={values["ui.fontSize"]}
            onChange={(event) => {
              const value = Math.max(11, Math.min(18, Number(event.target.value) || 12));
              void setValue("ui.fontSize", value);
            }}
          />
        </label>
        <label className="setting-row column">
          <span>{t("settings.editorFontFamily")}</span>
          <input
            key={`editor-font-${values["editor.fontFamily"]}`}
            type="text"
            list="anvil-font-families"
            defaultValue={values["editor.fontFamily"]}
            placeholder={t("settings.defaultMono")}
            onBlur={(event) => saveFontFamily("editor.fontFamily", event.target.value)}
          />
        </label>
        <label className="setting-row">
          <span>{t("settings.editorLineHeight")}</span>
          <select
            value={values["editor.lineHeight"]}
            onChange={(event) =>
              void setValue(
                "editor.lineHeight",
                event.target.value as "compact" | "normal" | "relaxed",
              )
            }
          >
            <option value="compact">{t("settings.lineHeightCompact")}</option>
            <option value="normal">{t("settings.lineHeightNormal")}</option>
            <option value="relaxed">{t("settings.lineHeightRelaxed")}</option>
          </select>
        </label>
        <label className="setting-row checkbox">
          <span>{t("settings.fontLigatures")}</span>
          <input
            type="checkbox"
            checked={values["editor.fontLigatures"]}
            onChange={(event) => void setValue("editor.fontLigatures", event.target.checked)}
          />
        </label>
        <datalist id="anvil-font-families">
          {fonts.map((font) => (
            <option key={font.family} value={font.family}>
              {font.monospace ? `${font.family} (${t("settings.monospace")})` : font.family}
            </option>
          ))}
        </datalist>
        <div className="setting-note">{t("settings.appearanceNote")}</div>
        {fonts.length === 0 ? (
          <div className="setting-note">{t("settings.fontListUnavailable")}</div>
        ) : null}
      </section>

      <section className="settings-section">
        <h3>{t("settings.editor")}</h3>
        <label className="setting-row">
          <span>{t("settings.fontSize")}</span>
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
          <span>{t("settings.tabSize")}</span>
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
          <span>{t("settings.wordWrap")}</span>
          <input
            type="checkbox"
            checked={values["editor.wordWrap"]}
            onChange={(event) => void setValue("editor.wordWrap", event.target.checked)}
          />
        </label>
        <label className="setting-row checkbox">
          <span>{t("settings.minimap")}</span>
          <input
            type="checkbox"
            checked={values["editor.minimap"]}
            onChange={(event) => void setValue("editor.minimap", event.target.checked)}
          />
        </label>
        <label className="setting-row checkbox">
          <span>{t("settings.hideGit")}</span>
          <input
            type="checkbox"
            checked={values["explorer.hideGit"]}
            onChange={(event) => void setValue("explorer.hideGit", event.target.checked)}
          />
        </label>
        <div className="setting-note">{t("settings.formatNote")}</div>
      </section>

      <section className="settings-section">
        <h3>{t("settings.languageServer")}</h3>
        <label className="setting-row column">
          <span>{t("settings.clangdPath")}</span>
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
          <span>{t("settings.clangdArgs")}</span>
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
          <span>{t("settings.projectConfig")}</span>
          <input
            type="checkbox"
            checked={values["lsp.enableProjectConfig"]}
            onChange={(event) => void setValue("lsp.enableProjectConfig", event.target.checked)}
          />
        </label>
        <div className="setting-note">{t("settings.projectConfigNote")}</div>
        <div className="settings-inline">
          <button
            type="button"
            className="secondary-button"
            onClick={() => void restartLanguageServer()}
          >
            {t("settings.restartLsp")}
          </button>
        </div>
      </section>

      <section className="settings-section">
        <h3>{t("settings.security")}</h3>
        <div className="settings-security">
          <ShieldIcon size={20} />
          <div>
            <strong>
              {security?.sandbox.available ?? info?.security.sandbox.available
                ? t("settings.sandboxAvailable")
                : t("settings.sandboxUnavailable")}
            </strong>
            <p>
              {security?.sandbox.reason ??
                info?.security.sandbox.reason ??
                t("settings.probingBubblewrap")}
            </p>
          </div>
        </div>
        <div className="setting-note">{t("settings.unsandboxedNote")}</div>
        <dl className="settings-paths">
          <div>
            <dt>{t("settings.settingsFile")}</dt>
            <dd>{info ? `${info.configDir}/settings.json` : t("common.unknown")}</dd>
          </div>
          <div>
            <dt>{t("settings.securityLog")}</dt>
            <dd>{security?.logs.security ?? t("common.unknown")}</dd>
          </div>
        </dl>
      </section>
    </Modal>
  );
}

function AboutModal() {
  const { t } = useI18n();
  const open = useUiStore((state) => state.aboutOpen);
  const setOpen = useUiStore((state) => state.setAboutOpen);
  const info = useCoreStore((state) => state.info);
  const security = useCoreStore((state) => state.security);
  if (!open) return null;
  return (
    <Modal
      title={t("about.title")}
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
            {t("command.securityStatus")}
          </button>
          <button type="button" className="primary-button" onClick={() => setOpen(false)}>
            {t("common.close")}
          </button>
        </>
      }
    >
      <div className="about-hero">
        <div className="welcome-logo small">
          Anvil<span>IDE</span>
        </div>
        <p>{t("editor.tagline")}</p>
        <span className="version-chip">v{info?.version ?? "0.0.1"} · {t("editor.experimental")}</span>
      </div>
      <p className="about-text">{t("about.description")}</p>
      <dl className="about-grid">
        <div>
          <dt>{t("about.product")}</dt>
          <dd>{info?.product ?? t("app.product")}</dd>
        </div>
        <div>
          <dt>{t("about.core")}</dt>
          <dd>{info ? `${info.core} ${info.version} (API ${info.apiVersion})` : "anvil-core"}</dd>
        </div>
        <div>
          <dt>{t("about.platform")}</dt>
          <dd>{info?.platform ?? "linux"}</dd>
        </div>
        <div>
          <dt>{t("about.license")}</dt>
          <dd>Apache-2.0</dd>
        </div>
        <div>
          <dt>{t("about.config")}</dt>
          <dd>{info?.configDir ?? "~/.config/anvil"}</dd>
        </div>
        <div>
          <dt>{t("about.cache")}</dt>
          <dd>{info?.cacheDir ?? "~/.cache/anvil"}</dd>
        </div>
        <div>
          <dt>{t("settings.securityLog")}</dt>
          <dd>{security?.logs.security ?? t("common.notLoaded")}</dd>
        </div>
        <div>
          <dt>{t("about.build")}</dt>
          <dd>{info?.devMode ? t("about.buildDevelopment") : t("about.buildRelease")}</dd>
        </div>
      </dl>
      <p className="about-fine">{t("about.fine")}</p>
    </Modal>
  );
}

function SecurityModal() {
  const { t } = useI18n();
  const open = useUiStore((state) => state.securityOpen);
  const setOpen = useUiStore((state) => state.setSecurityOpen);
  const info = useCoreStore((state) => state.info);
  const security = useCoreStore((state) => state.security);
  if (!open) return null;
  const sandbox = security?.sandbox ?? info?.security.sandbox;
  return (
    <Modal
      title={t("security.title")}
      onClose={() => setOpen(false)}
      footer={
        <button type="button" className="primary-button" onClick={() => setOpen(false)}>
          {t("common.close")}
        </button>
      }
      wide
    >
      {sandbox?.available ? (
        <div className="security-banner ok">
          <ShieldIcon size={20} />
          <div>
            <strong>{t("security.isolated")}</strong>
            <p>{sandbox.reason}</p>
          </div>
        </div>
      ) : (
        <div className="security-banner warning">
          <WarningIcon size={20} />
          <div>
            <strong>{t("security.unavailable")}</strong>
            <p>{sandbox?.reason ?? t("security.bwrapUnverified")}</p>
            <p>{t("security.unavailableNote")}</p>
          </div>
        </div>
      )}
      <dl className="about-grid">
        <div>
          <dt>bubblewrap</dt>
          <dd>{sandbox?.bwrap || t("security.notFound")}</dd>
        </div>
        <div>
          <dt>{t("security.networkNamespace")}</dt>
          <dd>{sandbox?.networkNamespace ? t("security.isolatedValue") : t("security.seccompDeny")}</dd>
        </div>
        <div>
          <dt>{t("security.workspaceRoot")}</dt>
          <dd>{security?.workspace.root || info?.workspace.root || t("common.none")}</dd>
        </div>
        <div>
          <dt>{t("security.auditLog")}</dt>
          <dd>{security?.logs.security ?? t("common.notLoaded")}</dd>
        </div>
      </dl>
      <ul className="security-list">
        <li>
          <CheckIcon size={13} /> {t("security.listFs")}
        </li>
        <li>
          <CheckIcon size={13} /> {t("security.listArgv")}
        </li>
        <li>
          <CheckIcon size={13} /> {t("security.listReadonly")}
        </li>
        <li>
          <CheckIcon size={13} /> {t("security.listTelemetry")}
        </li>
        <li>
          <CheckIcon size={13} /> {t("security.listSecrets")}
        </li>
      </ul>
      <p className="setting-note">{t("security.residual")}</p>
    </Modal>
  );
}

function RunConfigModal() {
  const { t } = useI18n();
  const open = useUiStore((state) => state.runConfigOpen);
  const setOpen = useUiStore((state) => state.setRunConfigOpen);
  if (!open) return null;
  return (
    <Modal
      title={t("runConfig.title")}
      onClose={() => setOpen(false)}
      footer={
        <>
          <button type="button" className="secondary-button" onClick={() => setOpen(false)}>
            {t("common.cancel")}
          </button>
          <button
            type="button"
            className="primary-button"
            disabled
            title={t("common.phase3")}
          >
            {t("common.start")}
          </button>
        </>
      }
      wide
    >
      <div className="roadmap-banner">
        <strong>{t("runConfig.phase3Title")}</strong>
        <p>{t("runConfig.phase3Body")}</p>
      </div>
      <div className="form-grid">
        <label>
          <span>{t("runConfig.target")}</span>
          <input disabled placeholder={t("common.notConfigured")} />
        </label>
        <label>
          <span>{t("runConfig.configuration")}</span>
          <input disabled placeholder={t("common.notConfigured")} />
        </label>
        <label>
          <span>{t("runConfig.workingDirectory")}</span>
          <input disabled placeholder={t("common.notConfigured")} />
        </label>
        <label>
          <span>{t("runConfig.arguments")}</span>
          <input disabled placeholder={t("common.notConfigured")} />
        </label>
        <label className="full">
          <span>{t("runConfig.environment")}</span>
          <textarea disabled rows={2} placeholder={t("common.notConfigured")} />
        </label>
      </div>
      <p className="setting-note">{t("runConfig.note")}</p>
    </Modal>
  );
}

function ReferencesModal() {
  const { t } = useI18n();
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
      title={t("references.title", { count: references.locations.length })}
      onClose={() => setReferences(null)}
      footer={
        <button type="button" className="primary-button" onClick={() => setReferences(null)}>
          {t("common.close")}
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
              <span className="reference-hint">{t("common.openLocation")}</span>
            </button>
          ))}
        </div>
      ))}
    </Modal>
  );
}

function PromptDialog() {
  const { t } = useI18n();
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
      setError(t("prompt.nameRequired"));
      return;
    }
    if (!isValidEntryName(trimmed)) {
      setError(t("prompt.invalidName"));
      return;
    }
    setBusy(true);
    try {
      await prompt.onSubmit(trimmed);
      close();
    } catch (submitError) {
      setError(
        submitError instanceof Error ? submitError.message : t("prompt.operationFailed"),
      );
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
            {t("common.cancel")}
          </button>
          <button type="button" className="primary-button" onClick={() => void submit()} disabled={busy}>
            {prompt.confirmLabel ?? t("common.ok")}
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
  const { t } = useI18n();
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
            {t("common.cancel")}
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
                    .toast("error", error instanceof Error ? error.message : t("confirm.operationFailed"));
                })
                .finally(() => {
                  setBusy(false);
                  close();
                });
            }}
          >
            {confirm.confirmLabel ?? t("common.confirm")}
          </button>
        </>
      }
    >
      <p className="confirm-message">{confirm.message}</p>
    </Modal>
  );
}

function CloseConfirmModal() {
  const { t } = useI18n();
  const open = useUiStore((state) => state.closeConfirmOpen);
  const setOpen = useUiStore((state) => state.setCloseConfirmOpen);
  const tabs = useEditorStore((state) => state.tabs);
  const dirty = tabs.filter((tab) => tab.dirty);
  if (!open) return null;
  return (
    <Modal
      title={t("dialog.unsavedTitle")}
      onClose={() => setOpen(false)}
      footer={
        <>
          <button type="button" className="secondary-button" onClick={() => setOpen(false)}>
            {t("common.cancel")}
          </button>
          <button
            type="button"
            className="danger-button"
            onClick={() => void closeDiscarding()}
          >
            {t("dialog.discardAndClose")}
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
            {t("dialog.saveAllAndClose")}
          </button>
        </>
      }
      wide
    >
      <p className="confirm-message">{tCount("dialog.unsavedCount", dirty.length)}</p>
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

