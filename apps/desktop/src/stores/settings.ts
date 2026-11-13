import { create } from "zustand";
import { api } from "../ipc/api";
import type { Settings } from "../ipc/types";
import { useUiStore } from "./ui";
import { t } from "../i18n";

export const defaultSettings: Settings = {
  "editor.fontSize": 14,
  "editor.tabSize": 4,
  "editor.wordWrap": false,
  "editor.minimap": true,
  "editor.formatOnSave": false,
  "editor.fontFamily": "",
  "editor.fontLigatures": true,
  "editor.lineHeight": "normal",
  "lsp.clangdPath": "clangd",
  "lsp.clangdArgs": [],
  "lsp.enableProjectConfig": false,
  "security.allowUnsandboxedLsp": false,
  "explorer.hideGit": true,
  "ui.theme": "dark",
  "ui.fontFamily": "",
  "ui.fontSize": 12,
  "ui.language": "auto",
};

interface SettingsState {
  values: Settings;
  setFrom: (values: Settings) => void;
  setValue: <K extends keyof Settings>(key: K, value: Settings[K]) => Promise<void>;
}

export const useSettingsStore = create<SettingsState>((set) => ({
  values: defaultSettings,
  setFrom: (values) => set({ values: { ...defaultSettings, ...values } }),
  setValue: async (key, value) => {
    try {
      const values = await api.settingsSet(key, value);
      set({ values: { ...defaultSettings, ...values } });
    } catch (error) {
      useUiStore
        .getState()
        .toast("error", error instanceof Error ? error.message : t("settings.updateFailed"));
    }
  },
}));
