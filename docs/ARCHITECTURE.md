# Anvil 0.0.1 architecture

## 1. Components

### Native shell (`shell/`)

A C++20 GTK3 + WebKitGTK 4.1 application. Responsibilities:

- create the main window and WebKit view;
- inject `window.__ANVIL_BOOT__` and the `anvil` script-message handler;
- start and supervise `anvil-core`;
- forward JSON messages between the web view and the core;
- provide native folder/file choosers, window title, close handshake, and core
  restart;
- enforce navigation, permission, download, and popup restrictions;
- support smoke tests, E2E hooks, and deterministic screenshots.

The shell does not read or write project files and does not implement language
intelligence. It is deliberately replaceable.

### C++ core (`core/`)

A standalone process with no GUI dependency. It owns:

- the workspace root and path jail;
- file read/write/list/create/rename/delete/search;
- inotify workspace watching;
- settings, recent projects, and workspace tab state;
- LSP lifecycle, document synchronisation, diagnostics, hover, completion,
  definition, references, signature help, rename, and formatting requests;
- bubblewrap/seccomp sandbox construction and process supervision;
- security logging and redaction;
- JSON-RPC validation and dispatch.

The core is usable without the shell: `anvil-core --stdio` is a testable
contract, and `anvil-core --self-test` verifies the security invariants that do
not require a workspace.

### Web UI (`apps/desktop/`)

React 18 + TypeScript + Zustand + Monaco, bundled by Vite into a single classic
script for reliable `file://` loading. It renders:

- title bar, menus, activity bar, explorer, editor tabs, breadcrumbs, welcome;
- Monaco with a custom Anvil theme;
- problems/output/terminal/debug panels;
- command palette, quick open, settings/about/security/run-configuration
  dialogs;
- status bar with cursor, encoding, EOL, language, clangd, and sandbox state.

Non-Phase-1 surfaces (terminal, debugger, AI, Git writes, build/run) show
explicit roadmap empty states rather than fabricated content.

## 2. IPC

### UI → shell

```json
{"type":"request","id":1,"method":"fs/read","params":{"path":"src/main.cpp"},"apiVersion":1}
```

The shell handles `shell/*` locally and forwards everything else as JSON-RPC
2.0 to the core. Events from the core are delivered as:

```json
{"type":"event","method":"lsp:diagnostics","params":{...}}
```

### Core methods

| Domain | Methods |
|---|---|
| System | `system/info`, `system/ping`, `system/shutdown` |
| Workspace | `workspace/open`, `workspace/close`, `workspace/saveState` |
| Filesystem | `fs/read`, `fs/write`, `fs/list`, `fs/stat`, `fs/create`, `fs/rename`, `fs/delete`, `fs/searchFiles` |
| Settings | `settings/get`, `settings/set` |
| Recent | `recent/list`, `recent/remove` |
| LSP documents | `lsp/ensure`, `lsp/didOpen`, `lsp/didChange`, `lsp/didSave`, `lsp/didClose` |
| LSP requests | `lsp/hover`, `lsp/completion`, `lsp/definition`, `lsp/references`, `lsp/documentSymbols`, `lsp/signatureHelp`, `lsp/rename`, `lsp/format` |
| LSP control | `lsp/restart`, `lsp/status` |
| Security | `security/status` |
| Git (read-only) | `git/branch` |
| Cancellation | `request/cancel` notification |

All request methods require `apiVersion: 1`. Every method has a typed
validator in `core/src/ipc/validate.cpp`; tests live in
`core/tests/test_validate.cpp`.

### Core notifications

`lsp:status`, `lsp:diagnostics`, `lsp:log`, `lsp:message`, `lsp:progress`,
`fs:changed`, `fs:rescan`, `fs:watchUnavailable`.

## 3. Data flows

### Open a file

1. The UI calls `workspace/open` with an absolute path chosen by a native
   dialog (or a recent project).
2. The core canonicalises the root, restricts the jail, starts an inotify
   watcher, and returns saved tab state.
3. The UI calls `fs/read`; the core resolves the relative path, rejects symlink
   escapes, enforces size/UTF-8 limits, and returns content plus mtime.
4. Monaco creates a model; the UI sends `lsp/didOpen`.
5. The core starts clangd in the sandbox on demand and forwards the document.

### Hover / definition / diagnostics

1. Monaco calls the registered provider.
2. The UI calls `lsp/hover` (or `definition`, `completion`, `references`).
3. The core maps the relative path to a percent-encoded `file://` URI, sends the
   LSP request, and waits with cancellation and a timeout.
4. The result is normalised (URIs/paths, location shapes) and returned.
5. clangd diagnostics arrive as `textDocument/publishDiagnostics`; the core
   filters them to workspace files and emits `lsp:diagnostics`; the UI stores
   them and sets Monaco markers.

### Save

1. `fs/write` carries the current `expectedMtime`.
2. The core atomic-writes a temp file, fsyncs, re-checks the mtime, renames, and
   fsyncs the directory.
3. A stale mtime returns `Conflict`; the UI shows the external-change banner
   with Reload/Overwrite options. Overwrite is an explicit force write.
4. `lsp/didSave` is sent to clangd.

## 4. Security model

- Workspace-scoped relative paths only. Workspace selection itself is
  mediated by the shell: CLI paths, native dialog results, and recent projects
  are allowed; an arbitrary `workspace/open` from the UI is rejected.
- No shell interpolation; argv arrays only.
- clangd in bubblewrap with a read-only workspace, private cache, no network,
  and a seccomp deny-list.
- Release builds fail closed when the sandbox is unavailable.
- Strict CSP, local assets only, no network, no remote navigation, no
  telemetry.
- Audit log in JSON Lines under the user's config directory.
- No secrets are stored in 0.0.1.

See `docs/security/threat-model.md` for the full analysis.

## 5. Persistence

- `<config>/settings.json` — flat settings, mode 0600.
- `<config>/recent.json` — recent projects, mode 0600.
- `<config>/workspace/<hash>.json` — open tabs and active tab per workspace.
- `<config>/logs/anvil-core.log` — rotating application log.
- `<config>/logs/security.log` — JSONL audit log.
- `<cache>/lsp/<workspace-hash>/cpp` — private clangd HOME/cache.
- Project files remain the only source of truth; Anvil writes no project
  metadata in 0.0.1 except user-created files.

## 6. Performance choices

- Editor models are created lazily on the first file open; the welcome screen
  does not instantiate Monaco.
- One Monaco editor instance is reused; models are swapped and disposed.
- `didChange` is coalesced at 55 ms and sent as one minimal text range computed
  from a prefix/suffix diff; large or ambiguous changes fall back to a full
  text replacement only when the diff is not representable.
- LSP requests have timeouts and cooperative cancellation.
- clangd is stopped after five minutes without open documents.
- Directory listings, search results, watcher events, diagnostics, and IPC
  payloads are bounded.
- Release builds use LTO, `-Os`/`-O2`, `--gc-sections`, and stripped binaries.

## 7. Deliberate 0.0.1 omissions

- Build/run integration, PTY terminal, DAP, Git writes, AI, plugins, and
  auto-update are not implemented. Their panels and the run configuration
  dialog show a Phase 3/4 roadmap state.
- Tree-sitter indexing is deferred; clangd is the source of C++ symbol
  intelligence for this release.
- The UI is a single bundle instead of lazy ES-module chunks because the
  `file://` origin cannot reliably load module chunks. A custom URI scheme and
  lazy Monaco split are planned for the next iteration.
