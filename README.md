# Anvil IDE

> **Status: 0.0.1 (experimental).** C and C++ editing with clangd
> intelligence, a native C++ core, a sandboxed language server, and a Monaco
> UI inside WebKitGTK. Build/run, debugging, the integrated terminal, Git write
> operations, plugins, and AI are **not** part of this release.

## Project handbook

[`PROJECT.md`](PROJECT.md) is the single source of truth for a new working
session: concept, architecture, security invariants, build and test commands,
current release state, roadmap, and the owner-maintained **Bugs** and
**Improvements** backlog. Start there; the master prompt is historical.

## What is Anvil?

Anvil is a modern integrated development environment focused on C and C++, built for developers who want the analytical power of a heavyweight IDE without the memory cost, startup time, and background churn.

Most modern editors bolt C++ support on top of a generic text editor. Anvil takes the opposite approach: C++ is the first-class citizen from day one. Its architecture combines a battle-tested editor frontend (Monaco) with a purpose-built native core written in C++, giving you:

- **Real C++ intelligence** — powered by clangd, the same engine that drives
  world-class C++ tooling. Accurate completion, go-to-definition, find
  references, signature help, formatting, and diagnostics that understand your
  code, not just your text.
- **A native core** — workspace access, language-server management, file
  watching, settings, and security policy are all C++20, behind a validated
  JSON-RPC contract, not a JavaScript bridge.
- **A tiny footprint** — no bundled Chromium, no Electron. The entire `.deb`
  is about 1.4 MB; the system WebView provides the renderer.
- **Security first** — project files are untrusted, every IPC path is
  workspace-scoped and validated, and clangd runs in bubblewrap + seccomp with
  a read-only workspace and no network.
- **Multi-language by design** — the LSP manager is generic; 0.0.1 wires only
  C/C++. Rust and Python are Phase 2.

## Why "Anvil"?

Because good code isn't written — it's forged. Anvil is the surface you strike against: hard, precise, and honest. Every tool in your workflow swings down onto it, and what comes out is something you can ship.

## What works in 0.0.1

- Open a real folder; lazy file tree with inotify refresh; create, rename, and
  delete files and folders.
- Tabs with dirty state, crash-safe workspace state restore, atomic saves, and
  external-change detection with Reload/Overwrite.
- Monaco editor with the Anvil dark theme, C++ syntax highlighting, minimap,
  bracket colorization, view-state restore, and per-file models.
- clangd lifecycle in a sandbox: hover, completion, go-to-definition, find
  references, signature help, formatting requests, and diagnostics.
- Problems panel with real clangd diagnostics and click-to-navigate.
- Status bar with branch, problem counts, cursor, indentation, EOL, encoding,
  language, clangd state, and sandbox state.
- Command palette, quick open by file name, settings, security status, and
  native folder/file dialogs.
- A built-in smoke/E2E mode and web-view screenshots for verification.

Everything else is an explicit roadmap empty state, not a fake implementation.

## Install (Ubuntu 24.04)

A ready package is built into `dist/`:

```bash
sudo apt install ./dist/anvil_0.0.1-1_amd64.deb
anvil /path/to/your/project
```

Recommended packages (installed automatically as Recommends by apt):
`bubblewrap`, `clangd-18`, `gdb` or `lldb`, `git`, `cmake`, and `ninja-build`.

The package also installs a minimal AppArmor profile for `/usr/bin/bwrap`
(`/etc/apparmor.d/anvil-bwrap`) so bubblewrap can create user namespaces on
Ubuntu 24.04. If your environment blocks user namespaces entirely, the language
server is refused rather than run unsandboxed; the Security Status dialog shows
why.

To build from source, see [docs/BUILD.md](docs/BUILD.md).

## Architecture

```
React + Monaco (WebKitGTK web process)
        │ window.webkit.messageHandlers.anvil
        ▼
C++ shell (GTK window, dialogs, core supervisor)
        │ newline-delimited JSON-RPC over pipes
        ▼
C++ core (workspace jail, fs, watcher, LSP manager, security)
        │ bubblewrap + seccomp
        ▼
clangd
```

The master prompt names Tauri for the desktop host. The author's instruction
for this iteration was to write the application in C++, and the Rust/Tauri
toolchain is not present in the reference build environment. The shell is
therefore a native C++ WebKitGTK host; it is intentionally thin and can be
replaced without touching the core. The rationale and consequences are
recorded in
[docs/adr/0001-native-cpp-shell.md](docs/adr/0001-native-cpp-shell.md).

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the component map, IPC
methods, data flows, and deliberate omissions.

## Security

- Workspace-scoped relative paths only; symlink escapes are rejected after
  canonicalisation; rename/delete operate on links without following them.
- Workspace selection is mediated by the native shell: only the CLI path,
  native-dialog choices, and recent projects can become roots, so a compromised
  UI cannot silently widen the jail.
- No `system()`, `popen()`, or `sh -c`: every process is started with an argv
  array.
- clangd runs in bubblewrap with a read-only workspace, a private cache, no
  network, and a seccomp deny-list (ptrace, mount, keyring, BPF, io_uring,
  non-UNIX sockets).
- Release builds fail closed if the sandbox is unavailable. Unsandboxed LSP is
  compiled out; the development override requires a dev build, an environment
  variable, and a setting, and is written to the audit log.
- Strict CSP, local assets only, no remote content, no telemetry, no AI
  provider. Secrets are not stored in 0.0.1.
- Security events are written to `~/.config/anvil/logs/security.log` with
  redaction.

Read the full [threat model](docs/security/threat-model.md).

## Build and test

```bash
./scripts/test.sh          # 51 core/security/integration tests
./scripts/build.sh release # release binaries + UI bundle
./scripts/build-deb.sh     # .deb, SBOM, signed local APT repo
./scripts/verify-deb.sh dist/anvil_0.0.1-1_amd64.deb
```

Test hooks:

```bash
ANVIL_UI_DIR=$PWD/apps/desktop/dist \
ANVIL_CORE_PATH=$PWD/build/dev/core/anvil-core \
build/dev/shell/anvil --smoke-test --dev
```

Screenshots and benchmark notes are in [docs/benchmarks.md](docs/benchmarks.md);
UI evidence is in `docs/screenshots/`.

## Known limitations of 0.0.1

- Start-up to the interactive welcome screen measured ~0.85 s in the reference
  VM, above the 500 ms goal. WebKitGTK process startup and the Monaco bundle
  dominate. A lazy Monaco bundle is planned.
- Full-stack memory with clangd exceeds the 300 MiB goal; clangd's background
  index is the largest contributor.
- No build/run, debugger, terminal, Git staging/commit, plugin, or AI support
  yet. Those panels state their phase instead of pretending to work.
- The UI is a single bundle because `file://` cannot reliably load lazy ES
  module chunks; a custom URI scheme is planned.
- The local APT repository in `dist/` is signed with a demonstration key. It
  must not be used for public distribution.
- `lintian` was not available in the reference environment; structural
  package checks and a GUI smoke test were used instead.

## License

Apache-2.0. See [LICENSE](LICENSE). The bundled Monaco, React, React DOM, and
Zustand components are MIT-licensed; notices are in the package copyright file.
