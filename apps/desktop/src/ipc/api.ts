import { bridge } from "./bridge";
import type {
  DirListing,
  FileReadResult,
  FsChangedPayload,
  GitInfo,
  LspCompletionResponse,
  LspDiagnostic,
  LspHover,
  LspLocationsResult,
  LspPosition,
  LspRange,
  LspSignatureHelp,
  LspStatusPayload,
  RecentProject,
  SearchFilesResult,
  SecurityInfo,
  Settings,
  SystemInfo,
  WorkspaceOpenResult,
} from "./types";

export interface LspTextChange {
  range?: LspRange;
  text: string;
}

export const api = {
  systemInfo: () => bridge.request<SystemInfo>("system/info", {}, 10_000),

  openWorkspace: (path: string) =>
    bridge.request<WorkspaceOpenResult>("workspace/open", { path }, 20_000),

  closeWorkspace: () => bridge.request<{ ok: boolean }>("workspace/close", {}, 10_000),

  saveWorkspaceState: (tabs: string[], active: string) =>
    bridge.request<{ ok: boolean }>("workspace/saveState", { tabs, active }, 8_000),

  readFile: (path: string, maxBytes?: number) =>
    bridge.request<FileReadResult>(
      "fs/read",
      maxBytes === undefined ? { path } : { path, maxBytes },
      20_000,
    ),

  writeFile: (
    path: string,
    content: string,
    expectedMtime?: number,
    force?: boolean,
  ) =>
    bridge.request<{ path: string; mtime: number; size: number }>(
      "fs/write",
      {
        path,
        content,
        ...(expectedMtime === undefined ? {} : { expectedMtime }),
        ...(force ? { force: true } : {}),
      },
      30_000,
    ),

  listDir: (path: string) =>
    bridge.request<DirListing>("fs/list", { path }, 20_000),

  stat: (path: string) =>
    bridge.request<{
      path: string;
      isDir: boolean;
      isFile: boolean;
      isSymlink: boolean;
      size: number;
      mtime: number;
    }>("fs/stat", { path }, 10_000),

  createEntry: (path: string, kind: "file" | "directory") =>
    bridge.request<{ path: string; isDir: boolean; isFile: boolean }>(
      "fs/create",
      { path, kind },
      10_000,
    ),

  renameEntry: (from: string, to: string) =>
    bridge.request<{ path: string }>("fs/rename", { from, to }, 10_000),

  deleteEntry: (path: string, recursive = false) =>
    bridge.request<{ ok: boolean }>("fs/delete", { path, recursive }, 10_000),

  searchFiles: (query: string, limit = 200) =>
    bridge.request<SearchFilesResult>("fs/searchFiles", { query, limit }, 10_000),

  settingsSet: (key: string, value: unknown) =>
    bridge.request<Settings>("settings/set", { key, value }, 10_000),

  securityStatus: () => bridge.request<SecurityInfo>("security/status", {}, 10_000),

  gitBranch: () => bridge.request<GitInfo>("git/branch", {}, 8_000),

  recentList: () =>
    bridge.request<{ recent: RecentProject[] }>("recent/list", {}, 8_000),

  removeRecent: (path: string) =>
    bridge.request<{ removed: boolean }>("recent/remove", { path }, 8_000),

  lspEnsure: (path: string, languageId: string) =>
    bridge.request<{ languageId: string; state: string; server: string; version: string }>(
      "lsp/ensure",
      { path, languageId },
      35_000,
    ),

  lspDidOpen: (path: string, languageId: string, text: string, version: number) =>
    bridge.request<{ ok: boolean }>(
      "lsp/didOpen",
      { path, languageId, text, version },
      35_000,
    ),

  lspDidChange: (
    path: string,
    version: number,
    payload: { changes: LspTextChange[] } | { fullText: string },
  ) =>
    bridge.request<{ version?: number; ignored?: boolean }>(
      "lsp/didChange",
      { path, version, ...payload },
      20_000,
    ),

  lspDidSave: (path: string) =>
    bridge.request<{ ok: boolean }>("lsp/didSave", { path }, 10_000),

  lspDidClose: (path: string) =>
    bridge.request<{ ok: boolean }>("lsp/didClose", { path }, 10_000),

  lspHover: (path: string, position: LspPosition) =>
    bridge.request<LspHover | null>("lsp/hover", {
      path,
      line: position.line,
      character: position.character,
    }, 20_000),

  lspCompletion: (
    path: string,
    position: LspPosition,
    triggerCharacter?: string,
  ) =>
    bridge.request<LspCompletionResponse>(
      "lsp/completion",
      {
        path,
        line: position.line,
        character: position.character,
        ...(triggerCharacter ? { triggerCharacter } : {}),
      },
      30_000,
    ),

  lspDefinition: (path: string, position: LspPosition) =>
    bridge.request<LspLocationsResult>("lsp/definition", {
      path,
      line: position.line,
      character: position.character,
    }, 25_000),

  lspReferences: (
    path: string,
    position: LspPosition,
    includeDeclaration = true,
  ) =>
    bridge.request<LspLocationsResult>("lsp/references", {
      path,
      line: position.line,
      character: position.character,
      includeDeclaration,
    }, 25_000),

  lspDocumentSymbols: (path: string) =>
    bridge.request<unknown>("lsp/documentSymbols", { path }, 20_000),

  lspSignatureHelp: (path: string, position: LspPosition) =>
    bridge.request<LspSignatureHelp | null>("lsp/signatureHelp", {
      path,
      line: position.line,
      character: position.character,
    }, 15_000),

  lspFormat: (path: string, tabSize: number, insertSpaces: boolean) =>
    bridge.request<Array<{ range: LspRange; newText: string }> | null>(
      "lsp/format",
      { path, tabSize, insertSpaces },
      25_000,
    ),

  lspRestart: (languageId = "") =>
    bridge.request<{ ok: boolean }>("lsp/restart", { languageId }, 15_000),

  lspStatus: () =>
    bridge.request<{
      servers: LspStatusPayload[];
      sandbox: { available: boolean; networkNamespace: boolean; reason: string };
    }>("lsp/status", {}, 10_000),

  shellOpenFolder: () =>
    bridge.request<{ path?: string; cancelled?: boolean }>("shell/openFolder", {}, 120_000),

  shellOpenFile: () =>
    bridge.request<{ path?: string; cancelled?: boolean }>("shell/openFile", {}, 120_000),

  shellSetTitle: (title: string) =>
    bridge.request<{ ok: boolean }>("shell/setTitle", { title }, 5_000),

  shellClose: () => bridge.request<{ ok: boolean }>("shell/close", {}, 5_000),

  shellRestartCore: () =>
    bridge.request<{ ok: boolean }>("shell/restartCore", {}, 15_000),
};

export type { LspDiagnostic, FsChangedPayload };
