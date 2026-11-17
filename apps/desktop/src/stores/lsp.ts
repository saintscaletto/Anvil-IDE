import { create } from "zustand";
import type {
  IncludeCandidate,
  LspDiagnostic,
  LspStatusPayload,
} from "../ipc/types";
import { AnvilError } from "../ipc/bridge";
import { useUiStore } from "./ui";
import { useCoreStore } from "./core";
import { t } from "../i18n";

interface LspState {
  statuses: Record<string, LspStatusPayload>;
  diagnostics: Record<string, LspDiagnostic[]>;
  progress: { languageId: string; percentage?: number; title?: string } | null;
  unresolved: { path: string; candidates: IncludeCandidate[] } | null;
  failureNotified: boolean;
  setStatus: (payload: LspStatusPayload) => void;
  setDiagnostics: (path: string, diagnostics: LspDiagnostic[]) => void;
  clearDiagnostics: (path: string) => void;
  reset: () => void;
  setProgress: (progress: LspState["progress"]) => void;
  setUnresolved: (unresolved: LspState["unresolved"]) => void;
  handleFailure: (error: unknown) => void;
}

export const useLspStore = create<LspState>((set, get) => ({
  statuses: {},
  diagnostics: {},
  progress: null,
  unresolved: null,
  failureNotified: false,
  setStatus: (payload) =>
    set((state) => ({
      statuses: { ...state.statuses, [payload.languageId]: payload },
    })),
  setDiagnostics: (path, diagnostics) =>
    set((state) => ({
      diagnostics: { ...state.diagnostics, [path]: diagnostics },
    })),
  clearDiagnostics: (path) =>
    set((state) => {
      const diagnostics = { ...state.diagnostics };
      delete diagnostics[path];
      return { diagnostics };
    }),
  reset: () =>
    set({
      statuses: {},
      diagnostics: {},
      progress: null,
      unresolved: null,
      failureNotified: false,
    }),
  setProgress: (progress) => set({ progress }),
  setUnresolved: (unresolved) => set({ unresolved }),
  handleFailure: (error) => {
    const message =
      error instanceof AnvilError
        ? error.message
        : error instanceof Error
          ? error.message
          : t("error.lsp");
    useCoreStore.getState().addLog("clangd", message);
    if (!get().failureNotified) {
      useUiStore.getState().toast("warning", message);
      set({ failureNotified: true });
    }
  },
}));

export function problemCounts(diagnostics: Record<string, LspDiagnostic[]>) {
  let errors = 0;
  let warnings = 0;
  for (const list of Object.values(diagnostics)) {
    for (const diagnostic of list) {
      if (diagnostic.severity === 1) errors += 1;
      else if (diagnostic.severity === 2) warnings += 1;
    }
  }
  return { errors, warnings };
}
