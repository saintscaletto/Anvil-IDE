import { create } from "zustand";
import type { SecurityInfo, SystemInfo } from "../ipc/types";

export interface LogLine {
  id: number;
  time: number;
  source: "core" | "clangd" | "ui";
  text: string;
}

interface CoreState {
  info: SystemInfo | null;
  security: SecurityInfo | null;
  status: "starting" | "ready" | "error" | "restarting";
  error: string | null;
  logs: LogLine[];
  setInfo: (info: SystemInfo) => void;
  setSecurity: (security: SecurityInfo) => void;
  setStatus: (status: CoreState["status"], error?: string | null) => void;
  addLog: (source: LogLine["source"], text: string) => void;
  clearLogs: () => void;
}

let nextLogId = 1;

export const useCoreStore = create<CoreState>((set) => ({
  info: null,
  security: null,
  status: "starting",
  error: null,
  logs: [],
  setInfo: (info) => set({ info }),
  setSecurity: (security) => set({ security }),
  setStatus: (status, error = null) => set({ status, error }),
  addLog: (source, text) =>
    set((state) => {
      const line: LogLine = {
        id: nextLogId++,
        time: Date.now(),
        source,
        text: text.length > 4000 ? `${text.slice(0, 4000)}…` : text,
      };
      const logs = state.logs.length >= 1200 ? state.logs.slice(-1199) : state.logs;
      return { logs: [...logs, line] };
    }),
  clearLogs: () => set({ logs: [] }),
}));
