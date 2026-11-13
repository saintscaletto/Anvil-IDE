import { openCommandPalette, openFileDialog, openFolderDialog, openQuickOpen, openWorkspacePath, removeRecent } from "../actions";
import { CloseIcon, FilesIcon } from "../components/Icons";
import { MonacoEditor } from "../features/MonacoEditor";
import { basename } from "../lib/paths";
import { useCoreStore } from "../stores/core";
import { useEditorStore } from "../stores/editor";
import { useWorkspaceStore } from "../stores/workspace";
import { useUiStore } from "../stores/ui";
import { useI18n } from "../i18n";

export function EditorArea() {
  const { t } = useI18n();
  const tabs = useEditorStore((state) => state.tabs);
  const activePath = useEditorStore((state) => state.activePath);
  const activeTab = tabs.find((tab) => tab.path === activePath);
  const root = useWorkspaceStore((state) => state.root);
  const recent = useWorkspaceStore((state) => state.recent);
  const version = useCoreStore((state) => state.info?.version ?? "0.0.1");

  return (
    <main className="editor-area">
      <div className="tab-bar" role="tablist" aria-label={t("editor.tabsLabel")}>
        {tabs.map((tab) => (
          <div
            key={tab.path}
            role="tab"
            aria-selected={tab.path === activePath}
            tabIndex={tab.path === activePath ? 0 : -1}
            className={`tab${tab.path === activePath ? " active" : ""}`}
            title={tab.path}
            onClick={() => useEditorStore.getState().activate(tab.path)}
            onAuxClick={(event) => {
              if (event.button === 1) void useEditorStore.getState().closeFile(tab.path);
            }}
            onKeyDown={(event) => {
              if (event.key === "Enter" || event.key === " ") {
                event.preventDefault();
                useEditorStore.getState().activate(tab.path);
              }
            }}
          >
            <span className={`tab-file-icon ${tab.language === "cpp" ? "cpp" : ""}`}>
              {tab.language === "cpp" ? "C++" : <FilesIcon size={12} />}
            </span>
            <span className="tab-name">{tab.name}</span>
            {tab.dirty ? <span className="tab-dirty" aria-label={t("editor.unsavedChanges")}>●</span> : null}
            <button
              type="button"
              className="tab-close"
              aria-label={t("editor.closeTab", { name: tab.name })}
              onClick={(event) => {
                event.stopPropagation();
                void useEditorStore.getState().closeFile(tab.path);
              }}
            >
              <CloseIcon size={12} />
            </button>
          </div>
        ))}
      </div>
      {activeTab ? (
        <div className="breadcrumbs" aria-label={t("editor.breadcrumbs")}>
          {root ? <span className="crumb root">{basename(root)}</span> : null}
          {activeTab.path.split("/").map((part, index) => (
            <span className="crumb" key={`${part}-${index}`}>
              <span className="sep">›</span>
              {part}
            </span>
          ))}
        </div>
      ) : null}
      {activeTab?.externalChange ? (
        <div className={`editor-banner${activeTab.deletedOnDisk ? " danger" : " warning"}`}>
          <span>
            {activeTab.deletedOnDisk
              ? t("editor.deletedOnDisk", { name: activeTab.name })
              : t("editor.changedOnDisk", { name: activeTab.name })}
          </span>
          {activeTab.deletedOnDisk ? (
            <button
              type="button"
              className="banner-action"
              onClick={() => void useEditorStore.getState().saveFile(activeTab.path, true)}
            >
              {t("editor.recreateFile")}
            </button>
          ) : (
            <>
              <button
                type="button"
                className="banner-action"
                onClick={() => void useEditorStore.getState().reloadFromDisk(activeTab.path)}
              >
                {t("editor.reload")}
              </button>
              <button
                type="button"
                className="banner-action"
                onClick={() => void useEditorStore.getState().saveFile(activeTab.path, true)}
              >
                {t("editor.overwrite")}
              </button>
            </>
          )}
          <button
            type="button"
            className="banner-dismiss"
            aria-label={t("editor.dismiss")}
            onClick={() => useEditorStore.getState().setExternalChange(activeTab.path, false)}
          >
            <CloseIcon size={12} />
          </button>
        </div>
      ) : null}
      <div className="editor-stack">
        <MonacoEditor
          onSave={() => void useEditorStore.getState().saveFile(useEditorStore.getState().activePath ?? "")}
          onSaveAll={() => void useEditorStore.getState().saveAll()}
          onClose={() => {
            const path = useEditorStore.getState().activePath;
            if (path) void useEditorStore.getState().closeFile(path);
          }}
        />
        {tabs.length === 0 ? (
          <section className="welcome-overlay" aria-label={t("editor.welcome")}>
            <div className="welcome-logo">
              Anvil<span>IDE</span>
            </div>
            <div className="welcome-sub">{t("editor.tagline")} <span className="version-chip">v{version} · {t("editor.experimental")}</span></div>
            <div className="welcome-grid">
              <div className="welcome-card">
                <h4>{t("editor.start")}</h4>
                <ul>
                  <li onClick={() => void openFolderDialog()}>
                    <span>{t("command.openFolder")}</span>
                    <kbd>Ctrl+Shift+O</kbd>
                  </li>
                  <li onClick={() => void openFileDialog()}>
                    <span>{t("command.openFile")}</span>
                    <kbd>Ctrl+O</kbd>
                  </li>
                  {root ? (
                    <li onClick={() => useUiStore.getState().showPrompt({
                      title: t("dialog.newFileTitle"),
                      label: t("dialog.newFileLabelRoot"),
                      confirmLabel: t("common.create"),
                      onSubmit: async (name) => {
                        const path = await useWorkspaceStore.getState().createEntry("", name, "file");
                        if (path) await useEditorStore.getState().openFile(path);
                      },
                    })}>
                      <span>{t("command.newFile")}</span>
                      <kbd>Ctrl+N</kbd>
                    </li>
                  ) : null}
                  <li onClick={openQuickOpen}>
                    <span>{t("command.goFile")}</span>
                    <kbd>Ctrl+P</kbd>
                  </li>
                  <li onClick={openCommandPalette}>
                    <span>{t("command.palette")}</span>
                    <kbd>Ctrl+Shift+P</kbd>
                  </li>
                </ul>
              </div>
              <div className="welcome-card">
                <h4>{t("editor.recent")}</h4>
                <ul>
                  {recent.length === 0 ? (
                    <li className="welcome-muted">{t("editor.noRecent")}</li>
                  ) : (
                    recent.slice(0, 6).map((entry) => (
                      <li key={entry.path} title={entry.path}>
                        <span className="recent-name" onClick={() => void openWorkspacePath(entry.path)}>
                          {entry.name}
                        </span>
                        <button
                          type="button"
                          className="recent-remove"
                          aria-label={t("editor.removeRecent", { name: entry.name })}
                          onClick={(event) => {
                            event.stopPropagation();
                            void removeRecent(entry.path);
                          }}
                        >
                          <CloseIcon size={10} />
                        </button>
                      </li>
                    ))
                  )}
                </ul>
              </div>
              <div className="welcome-card">
                <h4>{t("editor.learn")}</h4>
                <ul>
                  <li><span>{t("command.palette")}</span><kbd>Ctrl+Shift+P</kbd></li>
                  <li><span>{t("command.goDefinition")}</span><kbd>F12</kbd></li>
                  <li><span>{t("command.findReferences")}</span><kbd>Shift+F12</kbd></li>
                  <li><span>{t("command.save")}</span><kbd>Ctrl+S</kbd></li>
                  <li><span>{t("command.toggleSidebar")}</span><kbd>Ctrl+B</kbd></li>
                  <li><span>{t("panel.problems")}</span><kbd>Ctrl+Shift+M</kbd></li>
                </ul>
              </div>
            </div>
            <div className="welcome-footer">
              {t("editor.welcomeFooter")}
            </div>
          </section>
        ) : null}
      </div>
    </main>
  );
}
