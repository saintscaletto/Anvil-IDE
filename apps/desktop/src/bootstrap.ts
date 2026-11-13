import type { ThemePreference } from "./ipc/types";

export interface BootInfo {
  native: boolean;
  version: string;
  e2e: boolean;
  devMode: boolean;
  initialWorkspace: string;
  e2eFile: string;
  theme: ThemePreference;
  locale: string;
}

declare global {
  interface Window {
    __ANVIL_BOOT__?: Partial<BootInfo>;
    __ANVIL_NATIVE__?: boolean;
    __anvilReceive?: (message: unknown) => void;
    __anvilTest?: {
      openFile(path: string): Promise<void>;
      saveAll(): Promise<boolean>;
      hover(line: number, character: number): void;
      getState(): unknown;
    };
    webkit?: {
      messageHandlers?: {
        anvil?: {
          postMessage(message: unknown): void;
        };
      };
    };
  }
}

export const boot: BootInfo = {
  native: Boolean(window.__ANVIL_NATIVE__),
  version: window.__ANVIL_BOOT__?.version ?? "0.0.1",
  e2e: Boolean(window.__ANVIL_BOOT__?.e2e),
  devMode: Boolean(window.__ANVIL_BOOT__?.devMode),
  initialWorkspace: window.__ANVIL_BOOT__?.initialWorkspace ?? "",
  e2eFile: window.__ANVIL_BOOT__?.e2eFile ?? "",
  theme: window.__ANVIL_BOOT__?.theme ?? "auto",
  locale: window.__ANVIL_BOOT__?.locale ?? "en",
};

export const nativeBridgeAvailable = Boolean(
  window.webkit?.messageHandlers?.anvil?.postMessage,
);
