export interface LspPosition {
  line: number;
  character: number;
}

export interface LspRange {
  start: LspPosition;
  end: LspPosition;
}

export interface LspDiagnostic {
  range: LspRange;
  severity?: 1 | 2 | 3 | 4;
  code?: string | number;
  source?: string;
  message: string;
  relatedInformation?: unknown[];
  tags?: number[];
}

export interface LspMarkupContent {
  kind: "markdown" | "plaintext";
  value: string;
}

export interface LspHover {
  contents: string | LspMarkupContent | Array<string | { language: string; value: string }>;
  range?: LspRange;
}

export interface LspCompletionItem {
  label: string | { label: string; detail?: string; description?: string };
  kind?: number;
  detail?: string;
  documentation?: string | LspMarkupContent;
  sortText?: string;
  filterText?: string;
  insertText?: string;
  insertTextFormat?: 1 | 2;
  textEdit?: { range: LspRange; newText: string };
  additionalTextEdits?: Array<{ range: LspRange; newText: string }>;
  deprecated?: boolean;
  preselect?: boolean;
  labelDetails?: { detail?: string; description?: string };
}

export interface LspCompletionList {
  isIncomplete: boolean;
  items: LspCompletionItem[];
}

export type LspCompletionResponse = LspCompletionItem[] | LspCompletionList | null;

export interface LspLocation {
  path: string;
  range: LspRange;
  originSelectionRange?: LspRange;
}

export interface LspLocationsResult {
  locations: LspLocation[];
}

export interface LspSignatureHelp {
  signatures: Array<{
    label: string;
    documentation?: string | LspMarkupContent;
    parameters?: Array<{ label: string | [number, number]; documentation?: string | LspMarkupContent }>;
  }>;
  activeSignature?: number;
  activeParameter?: number;
}

export interface RecentProject {
  path: string;
  name: string;
  lastOpened: number;
}

export interface DirEntry {
  name: string;
  path: string;
  isDir: boolean;
  isSymlink: boolean;
  size: number;
  mtime: number;
}

export interface DirListing {
  path: string;
  entries: DirEntry[];
  truncated: boolean;
}

export interface FileReadResult {
  path: string;
  content: string;
  language: string;
  mtime: number;
  size: number;
}

export interface GitInfo {
  repository: boolean;
  branch?: string;
  detached?: boolean;
  worktree?: boolean;
  reason?: string;
}

export interface WorkspaceState {
  tabs: string[];
  active: string;
}

export interface WorkspaceOpenResult {
  root: string;
  name: string;
  state: WorkspaceState;
  recent: RecentProject[];
  settings: Settings;
  git: GitInfo;
  watch: boolean;
}

export interface SandboxInfo {
  available: boolean;
  networkNamespace: boolean;
  bwrap: string;
  reason: string;
}

export interface SecurityInfo {
  sandbox: SandboxInfo;
  policy: {
    workspaceScoped: boolean;
    shellExecution: boolean;
    seccomp: boolean;
    lspIsolation: string;
    networkForLsp: boolean;
  };
  logs: { security: string };
  workspace: { open: boolean; root: string };
}

export interface SystemInfo {
  product: string;
  version: string;
  apiVersion: number;
  core: string;
  platform: string;
  configDir: string;
  cacheDir: string;
  devMode: boolean;
  workspace: { open: boolean; root?: string; name?: string };
  settings: Settings;
  recent: RecentProject[];
  security: { sandbox: SandboxInfo };
  capabilities: {
    fsWatch: boolean;
    fileSearch: boolean;
    gitBranch: boolean;
    lspLanguages: string[];
  };
}

export type ThemePreference = "auto" | "dark" | "light" | "high-contrast";
export type LanguagePreference = "auto" | "en" | "ru";
export type EditorLineHeight = "compact" | "normal" | "relaxed";

export interface Settings {
  "editor.fontSize": number;
  "editor.tabSize": number;
  "editor.wordWrap": boolean;
  "editor.minimap": boolean;
  "editor.formatOnSave": boolean;
  "editor.fontFamily": string;
  "editor.fontLigatures": boolean;
  "editor.lineHeight": EditorLineHeight;
  "lsp.clangdPath": string;
  "lsp.clangdArgs": string[];
  "lsp.enableProjectConfig": boolean;
  "security.allowUnsandboxedLsp": boolean;
  "explorer.hideGit": boolean;
  "ui.theme": ThemePreference;
  "ui.fontFamily": string;
  "ui.fontSize": number;
  "ui.language": LanguagePreference;
}

export interface FontFamilyInfo {
  family: string;
  monospace: boolean;
}

export interface FontListResult {
  families: FontFamilyInfo[];
  truncated: boolean;
}

export interface LspStatusPayload {
  languageId: string;
  state: "starting" | "running" | "stopped" | "error";
  message: string;
  server: string;
  version: string;
  binary: string;
  openDocuments: number;
}

export interface LspDiagnosticsPayload {
  languageId: string;
  path: string;
  version?: number;
  diagnostics: LspDiagnostic[];
}

export interface SearchResult {
  path: string;
  name: string;
}

export interface SearchFilesResult {
  results: SearchResult[];
  truncated: boolean;
}

export interface FsChange {
  path: string;
  kind: "created" | "deleted" | "modified" | "renamed" | string;
}

export interface FsChangedPayload {
  changes: FsChange[];
}
