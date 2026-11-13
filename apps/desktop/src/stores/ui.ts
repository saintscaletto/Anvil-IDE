import { create } from "zustand";
import type { LspLocation } from "../ipc/types";

export type Activity = "explorer" | "search" | "scm" | "ai";
export type PanelTab = "problems" | "output" | "terminal" | "debug";
export type ToastKind = "info" | "success" | "warning" | "error";

export interface Toast {
  id: number;
  kind: ToastKind;
  message: string;
}

export interface PromptConfig {
  title: string;
  label: string;
  initialValue?: string;
  confirmLabel?: string;
  onSubmit: (value: string) => void | Promise<void>;
}

export interface ConfirmConfig {
  title: string;
  message: string;
  confirmLabel?: string;
  danger?: boolean;
  onConfirm: () => void | Promise<void>;
}

interface UiState {
  activity: Activity;
  sidebarVisible: boolean;
  panelVisible: boolean;
  panelTab: PanelTab;
  paletteOpen: boolean;
  quickOpenOpen: boolean;
  settingsOpen: boolean;
  aboutOpen: boolean;
  securityOpen: boolean;
  runConfigOpen: boolean;
  closeConfirmOpen: boolean;
  prompt: PromptConfig | null;
  confirm: ConfirmConfig | null;
  references: { filePath: string; locations: LspLocation[] } | null;
  toasts: Toast[];
  setActivity: (activity: Activity) => void;
  toggleSidebar: () => void;
  togglePanel: () => void;
  showPanel: (tab: PanelTab) => void;
  setPaletteOpen: (open: boolean) => void;
  setQuickOpenOpen: (open: boolean) => void;
  setSettingsOpen: (open: boolean) => void;
  setAboutOpen: (open: boolean) => void;
  setSecurityOpen: (open: boolean) => void;
  setRunConfigOpen: (open: boolean) => void;
  setCloseConfirmOpen: (open: boolean) => void;
  setReferences: (references: { filePath: string; locations: LspLocation[] } | null) => void;
  showPrompt: (config: PromptConfig) => void;
  closePrompt: () => void;
  showConfirm: (config: ConfirmConfig) => void;
  closeConfirm: () => void;
  toast: (kind: ToastKind, message: string) => void;
  dismissToast: (id: number) => void;
}

let nextToastId = 1;

export const useUiStore = create<UiState>((set, get) => ({
  activity: "explorer",
  sidebarVisible: true,
  panelVisible: true,
  panelTab: "problems",
  paletteOpen: false,
  quickOpenOpen: false,
  settingsOpen: false,
  aboutOpen: false,
  securityOpen: false,
  runConfigOpen: false,
  closeConfirmOpen: false,
  prompt: null,
  confirm: null,
  references: null,
  toasts: [],
  setActivity: (activity) => {
    const state = get();
    if (state.activity === activity) {
      set({ sidebarVisible: !state.sidebarVisible });
    } else {
      set({ activity, sidebarVisible: true });
    }
  },
  toggleSidebar: () => set((state) => ({ sidebarVisible: !state.sidebarVisible })),
  togglePanel: () => set((state) => ({ panelVisible: !state.panelVisible })),
  showPanel: (panelTab) => set({ panelVisible: true, panelTab }),
  setPaletteOpen: (paletteOpen) => set({ paletteOpen }),
  setQuickOpenOpen: (quickOpenOpen) => set({ quickOpenOpen }),
  setSettingsOpen: (settingsOpen) => set({ settingsOpen }),
  setAboutOpen: (aboutOpen) => set({ aboutOpen }),
  setSecurityOpen: (securityOpen) => set({ securityOpen }),
  setRunConfigOpen: (runConfigOpen) => set({ runConfigOpen }),
  setCloseConfirmOpen: (closeConfirmOpen) => set({ closeConfirmOpen }),
  setReferences: (references) => set({ references }),
  showPrompt: (prompt) => set({ prompt }),
  closePrompt: () => set({ prompt: null }),
  showConfirm: (confirm) => set({ confirm }),
  closeConfirm: () => set({ confirm: null }),
  toast: (kind, message) => {
    const toast: Toast = { id: nextToastId++, kind, message };
    set((state) => ({ toasts: [...state.toasts.slice(-4), toast] }));
    window.setTimeout(() => {
      set((state) => ({ toasts: state.toasts.filter((item) => item.id !== toast.id) }));
    }, 6500);
  },
  dismissToast: (id) =>
    set((state) => ({ toasts: state.toasts.filter((item) => item.id !== id) })),
}));
