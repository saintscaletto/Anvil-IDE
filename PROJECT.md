# Anvil IDE — Project Handbook

> **Single source of truth for any new working session.**
> Read this file first. It is written so a fresh chat can start useful work in
> minutes without re-reading the full master prompt or the entire codebase.

| Field | Value |
|---|---|
| File version | 1.1 |
| Applies to | Anvil IDE **0.0.1** (released), **0.0.2** in progress |
| Last updated | 2026-11-13 |
| Owner | Project author |
| Repository | `github.com/saintscaletto/anvilide` |
| Working branch | `devel/0.0.2` |
| Current artifact | `dist/anvil_0.0.1-1_amd64.deb` (0.0.2 not packaged yet) |
| Target platform | Ubuntu 24.04 LTS, x86_64 |
| License | Apache-2.0 (bundled Monaco/React/Zustand are MIT) |

---

## 60-second orientation

- **Product:** C++-first IDE; Monaco UI in a system WebView, native C++ core.
- **Stack:** C++20 core (`anvil-core`), C++ GTK3/WebKitGTK shell (`anvil`),
  React 18 + TypeScript + Zustand + Monaco UI, JSON-RPC 2.0 over stdio.
- **Security:** workspace jail, argv-only spawn, sandboxed clangd
  (bubblewrap + seccomp), no telemetry, no remote content. §2 is binding.
- **Released:** 0.0.1 — open/edit C++, clangd intelligence, diagnostics, `.deb`.
- **Next:** 0.0.2 — IMP-001/IMP-002/IMP-004 delivered on `devel/0.0.2`
  (Russian UI, themes, fonts); IMP-003 (Codewhale) and IMP-005 (ROS 2
  environment) remain.
- **Roadmap:** §7 — global plan with per-item status for Phase 1–4, release
  map, cross-cutting workstreams, and the definition of 1.0.
- **Your two backlog fields:** §8 Bugs and §9 Improvements. Add entries with
  the templates; never delete old ones.
- **Build/test:** `./scripts/build.sh release`, `./scripts/test.sh`,
  `./scripts/build-deb.sh`; UI gates: `npm run typecheck`, `npm run build`,
  `npm run check:contrast`, `npm run check:i18n`.
- **For a new chat:** use the prompt in §0.1 verbatim.

---

## 0. Как пользоваться этим файлом (для владельца)

- **§8 Bugs** — сюда записывай найденные баги. Одна запись = один блок по
  шаблону. Не удаляй старые записи: меняй `Status` на `FIXED (0.0.x)`.
- **§9 Improvements** — сюда записывай улучшения, которые обязательно нужны
  в следующем релизе. Они не теряются среди фаз: у каждой записи есть ID,
  целевой релиз и критерии приёмки.
- **§7 Roadmap** — глобальная картина по фазам. Улучшения из §9 продублированы
  в плане релиза 0.0.2, чтобы было видно порядок работ.
- Файл можно править вручную и коммитить в git: он не содержит абсолютных
  локальных путей, секретов и машинно-зависимых значений. Именно поэтому его
  можно использовать с другого компьютера.
- Новому чату достаточно дать одну строку: «Read `PROJECT.md` and continue
  from the backlog for release 0.0.2.»

Инструкция для ИИ-сессии — в §10.1. Копируемый стартовый промпт — в §0.1.

### 0.1. Стартовый промпт для нового чата (копировать целиком)

```
You are working on Anvil IDE. Read PROJECT.md at the repository root first.
It is the single source of truth for the product concept, architecture,
security invariants, build/test commands, current state, roadmap, bugs and
improvements.

Do not read prompts/masterprompt_0.0.1.md unless the owner explicitly asks;
PROJECT.md summarizes everything a new session needs. Open only the source
files relevant to the chosen backlog items.

Before coding:
1. Say which backlog IDs (BUG-xxx / IMP-xxx) you will work on and for which
   release.
2. Confirm the change does not violate the invariants in PROJECT.md §2.
3. Produce a short plan with the files you expect to touch.

After coding:
1. Run the verification commands from PROJECT.md §5.
2. Update PROJECT.md statuses and the changelog (§11).
3. Report changed files, evidence, and anything not verified.

Do not push to git, do not rewrite history, and do not start work outside the
selected backlog items without asking.
```

---

## 1. Product concept

**Anvil IDE** is a C++-first desktop IDE: the analytical depth of a heavyweight
IDE with the startup and footprint of a lightweight editor.

- **Slogan:** *Forge C++ at full speed.*
- **Primary audience:** C/C++ and systems developers on real codebases
  (robotics/ROS 2, embedded, infrastructure, HFT, scientific computing).
- **Differentiator:** first-class C++ intelligence (clangd), a native C++ core,
  and a small shell around the system WebView — no Electron, no bundled
  Chromium.
- **Product shape:** a Monaco/React editor UI inside a native WebKitGTK window;
  all filesystem, workspace, language-server, and security logic lives in a
  separate C++ core process.
- **Language scope:** C and C++ are first-class. Rust and Python are planned
  through LSP (Phase 2). The UI shipped in English in 0.0.1; 0.0.2 adds
  English/Russian switching (IMP-001, delivered on `devel/0.0.2`).

### 1.1. What Anvil is not

- Not a web IDE, not a cloud IDE.
- Not an Electron app; Chromium is never bundled.
- Not a generic editor with C++ bolted on: C++ intelligence is the core.
- Not a CLion clone in 0.0.2: build/debug/terminal are later phases.

---

## 2. Invariants (must not be broken)

These override convenience, deadlines, and feature requests. If a request
conflicts with an invariant, stop and name the conflict.

### 2.1. Security invariants

1. **No shell interpolation.** Never use `system()`, `popen()`, or string-built
   `sh -c`. All processes are started with an argv array via the native spawn
   helper. A shell interpreter may only be executed as an explicit,
   user-confirmed, audited action with fixed argv (see IMP-005).
2. **Workspace path jail.** Every IPC path is a workspace-relative POSIX path,
   validated for `..`, absolute paths, backslashes, control characters, length,
   and UTF-8. The core canonicalises and checks containment; symlink escapes are
   rejected. Rename/delete/stat operate on links without following them.
3. **Workspace selection is mediated by the shell.** Only a command-line path,
   a native GTK dialog result, or a path from the user's `recent.json` may
   become a workspace root. A UI-initiated `workspace/open` for an arbitrary
   path is rejected.
4. **Language servers are sandboxed.** clangd runs under bubblewrap with
   seccomp, a read-only workspace, a private cache, no network, and a minimal
   environment. Release builds **fail closed** if the sandbox is unavailable.
   There is no silent unsandboxed fallback. The development override requires
   `ANVIL_DEV_MODE=ON`, `ANVIL_DEV_UNSAFE_LSP=1`, and the
   `security.allowUnsandboxedLsp` setting, and is written to the audit log.
5. **No remote content, no telemetry.** The UI is loaded from packaged local
   assets with a strict CSP (`default-src 'none'`, `connect-src 'none'`, no
   frames/objects). Navigation, popups, downloads, and permission requests are
   denied. Nothing is sent anywhere unless a future feature is explicitly
   opt-in and documented.
6. **Secrets never in JSON or logs.** Future Git/AI credentials must use
   libsecret via D-Bus. Anvil must never receive an API key from the UI. Logs
   and language-server output pass through redaction.
7. **Bounded input.** Requests, files, directory listings, watcher batches,
   diagnostics, search results, and queues are size-limited.
8. **Atomic, conflict-aware saves.** Writes use a temp file + fsync + rename and
   an optimistic mtime check. A stale writer gets a conflict, not a silent
   overwrite.
9. **New processes need a profile.** Any new external process must be started
   with argv, `no_new_privs`, a defined sandbox profile, a minimal explicit
   environment (never the full inherited one), and an audit record.
10. **Project content is untrusted.** `CMakeLists.txt`, `.clangd`,
    `compile_commands.json`, source files, and file names are input, not code to
    execute. Build/configure/setup scripts run only on an explicit, confirmed,
    named action.

### 2.2. Architecture invariants

1. **C++20 core owns business logic.** The shell must stay thin; do not move
   workspace/LSP/security policy into TypeScript or the shell.
2. **Monaco is the editor.** One editor instance, models swapped per tab.
3. **System WebView only.** No Electron, no bundled Chromium, no QtWebEngine.
4. **No UI redesign without an explicit request.** Follow the design tokens and
   layout in `docs/ARCHITECTURE.md` and the existing CSS.
5. **No fake features.** Unimplemented panels show an honest roadmap state; do
   not fabricate content.
6. **Release builds are hardened and reproducible.** LTO, gc-sections, PIE,
   RELRO, stack protector, stripped binaries, no RPATH/RUNPATH, SBOM included.

---

## 3. Architecture

### 3.1. Process model

```
┌──────────────────────────────────────────────────────────────┐
│ WebKitGTK web process                                        │
│ React 18 + TypeScript + Zustand + Monaco                     │
│ CSP: local assets only, no network, no frames                │
└───────────────▲──────────────────────────────────────────────┘
                │ window.webkit.messageHandlers.anvil
                │ {type:request|notification, id, method, params,
                │  apiVersion:1}
┌───────────────┴──────────────────────────────────────────────┐
│ C++ shell: anvil (GTK3 + WebKitGTK 4.1)                      │
│ window, native dialogs, core supervisor, workspace allowlist │
│ forwards non-shell methods as JSON-RPC 2.0 over pipes        │
└───────────────▲──────────────────────────────────────────────┘
                │ newline-delimited JSON-RPC (stdin/stdout)
┌───────────────┴──────────────────────────────────────────────┐
│ C++ core: anvil-core (no GUI dependency)                     │
│ workspace jail · fs · watcher · settings · recent · state    │
│ LSP manager · sandbox/seccomp policy · security audit log    │
└───────────────▲──────────────────────────────────────────────┘
                │ bubblewrap + seccomp, read-only workspace
┌───────────────┴──────────────────────────────────────────────┐
│ clangd (external, installed via apt, not bundled)            │
└──────────────────────────────────────────────────────────────┘
```

### 3.2. Component responsibilities

| Component | Path | Responsibility |
|---|---|---|
| Core | `core/` | Workspace jail, fs, watcher, settings, recent/state, LSP lifecycle, sandbox, audit |
| Shell | `shell/` | Window, WebKit view, JS bridge, native dialogs, core supervision, workspace allowlist, smoke/E2E/screenshot |
| UI | `apps/desktop/` | React/Monaco editor, explorer, tabs, panels, palette, settings dialogs, i18n, themes/fonts |
| Packaging | `packaging/`, `scripts/` | Debian metadata, build/stage/verify, SBOM, signed APT repo |
| Docs | `docs/` | Architecture, threat model, build guide, benchmarks, ADR, release checklist |

### 3.3. IPC contract (summary)

UI → shell → core. All requests carry `apiVersion: 1`. Every core method has a
typed validator in `core/src/ipc/validate.cpp`.

| Domain | Methods |
|---|---|
| System | `system/info`, `system/ping`, `system/shutdown` |
| Workspace | `workspace/open`, `workspace/close`, `workspace/saveState` |
| Filesystem | `fs/read`, `fs/write`, `fs/list`, `fs/stat`, `fs/create`, `fs/rename`, `fs/delete`, `fs/searchFiles` |
| Settings | `settings/get`, `settings/set` |
| Recent | `recent/list`, `recent/remove` |
| LSP docs | `lsp/ensure`, `lsp/didOpen`, `lsp/didChange`, `lsp/didSave`, `lsp/didClose` |
| LSP requests | `lsp/hover`, `lsp/completion`, `lsp/definition`, `lsp/references`, `lsp/documentSymbols`, `lsp/signatureHelp`, `lsp/rename`, `lsp/format` |
| LSP control | `lsp/restart`, `lsp/status` |
| Security | `security/status` |
| Git (read-only) | `git/branch` |
| Cancel | `request/cancel` (notification) |

Core → UI events: `lsp:status`, `lsp:diagnostics`, `lsp:log`, `lsp:message`,
`lsp:progress`, `fs:changed`, `fs:rescan`, `fs:watchUnavailable`.

Shell-only methods: `shell/info`, `shell/openFolder`, `shell/openFile`,
`shell/setTitle`, `shell/close`, `shell/restartCore`.

Error codes are defined in `core/include/anvil/error.hpp` (`-32001`…
`-32015` plus JSON-RPC standard codes).

### 3.4. Key data flows

- **Open file:** shell approves workspace → `workspace/open` → `fs/read`
  (canonical path, UTF-8/size checks) → Monaco model → `lsp/didOpen`.
- **Intelligence:** Monaco provider → `lsp/hover|completion|definition|
  references|signatureHelp` → core maps relative path to `file://` URI →
  clangd request with timeout/cancellation → normalised result.
- **Diagnostics:** clangd `publishDiagnostics` → core filters to workspace
  paths → `lsp:diagnostics` → Monaco markers + Problems panel.
- **Save:** `fs/write` with `expectedMtime` → atomic write → conflict on stale
  mtime → UI offers Reload/Overwrite → `lsp/didSave`.
- **External change:** inotify → `fs:changed` → explorer refresh + editor
  reload/banner.

### 3.5. Persistence

| Path | Content |
|---|---|
| `~/.config/anvil/settings.json` | Flat settings, mode 0600 |
| `~/.config/anvil/recent.json` | Recent projects, mode 0600 |
| `~/.config/anvil/workspace/<hash>.json` | Open tabs and active tab |
| `~/.config/anvil/logs/anvil-core.log` | Rotating application log |
| `~/.config/anvil/logs/security.log` | JSONL audit log (0600, 10 MiB × 5) |
| `~/.cache/anvil/lsp/<workspace-hash>/cpp` | Private clangd HOME/cache |
| `~/.cache/anvil/...` | Future persistent indexes |

The project directory remains the source of truth. Anvil stores no project
metadata in 0.0.1 except files the user creates.

### 3.6. Design system (for UI work)

| Token | Dark (default) | Purpose |
|---|---|---|
| `--bg-primary` | `#1e1f22` | Editor canvas |
| `--bg-secondary` | `#2b2d30` | Sidebar, panels, modals |
| `--bg-tertiary` | `#1a1b1e` | Title bar, tab bar, inputs |
| `--bg-hover` | `#35373a` | Hover |
| `--bg-active` | `#2e436e` | Selection, active states |
| `--border` | `#393b40` | Dividers |
| `--text-primary` | `#dfe1e5` | Primary text |
| `--text-muted` | `#94979c` | Secondary text |
| `--accent` | `#4a88c7` | Brand accent, borders, focus rings |
| `--accent-text` | `#7fb4e8` | Accent-colored text (AA on dark) |
| `--accent-button` | `#2f6fa8` | Solid primary button (white text) |
| `--error` | `#f75464` | Error borders and indicators |
| `--error-text` | `#ff6c7a` | Error text (AA on panels) |
| `--error-button` | `#b3261e` | Solid danger button (white text) |
| `--warning` | `#e5b567` | Warnings |
| `--success` | `#73c991` | Success, additions |
| `--status-bg` / `--status-text` | dark blue / near-white | Status bar |

- **Themes:** `ui.theme` selects `anvil-dark`, `anvil-light`, or
  `anvil-high-contrast`; `auto` follows `prefers-color-scheme`. Tokens are
  scoped under `:root[data-theme="…"]` in `styles.css`; `theme/apply.ts` sets
  the attribute and color scheme. Monaco registers one theme per app theme.
  No component may hardcode a color outside the preview swatches.
- **Fonts:** UI and editor families are validated data stored in settings and
  applied through `--font-ui`, `--font-code`, and `--ui-font-size` by
  `theme/fonts.ts`. The default stacks are system sans and the code stack
  below. Font size scales all chrome; the status bar caps its own chrome text
  so it remains usable at 18 px.
- UI font stack: system sans (`-apple-system, "Segoe UI", Roboto,
  "Noto Sans", sans-serif`). Code: `"JetBrains Mono", "Fira Code", Consolas,
  "Liberation Mono", monospace`.
- Sizes: activity bar 48 px, sidebar 260 px, right panel 320 px, title bar
  36 px, tab bar 34 px, breadcrumbs 22 px, status bar 24 px. 8 px baseline.
- Radius: 3 px inputs/buttons, 6 px modals/panels, 12 px chips.
- Monaco themes: `anvil-dark`, `anvil-light`, `anvil-high-contrast` in
  `apps/desktop/src/editor/theme.ts`; token colors follow the tables there. Do
  not hardcode colors in components — use tokens.
- Accessibility: visible `:focus-visible`, ARIA labels on icon-only buttons,
  focus trap in modals, keyboard path for every primary action. All three
  themes pass WCAG AA (4.5:1) for UI text pairs (`npm run check:contrast`).

### 3.7. Settings schema (current)

| Key | Type | Range / default | Notes |
|---|---|---|---|
| `editor.fontSize` | int | 8–32, default 14 | Monaco |
| `editor.tabSize` | int | 1–8, default 4 | Indentation |
| `editor.wordWrap` | bool | false | Monaco |
| `editor.minimap` | bool | true | Monaco |
| `editor.formatOnSave` | bool | false | Reserved; no silent formatting |
| `lsp.clangdPath` | string | `clangd` | Name or absolute path |
| `lsp.clangdArgs` | string[] | `[]` | ≤ 32 entries, ≤ 256 chars each |
| `lsp.enableProjectConfig` | bool | false | Reads untrusted `.clangd` |
| `security.allowUnsandboxedLsp` | bool | false | Dev builds only |
| `explorer.hideGit` | bool | true | Hide `.git` in tree |
| `editor.fontFamily` | string | `""` (default mono stack) | Validated family name |
| `editor.fontLigatures` | bool | true | Monaco |
| `editor.lineHeight` | string | `normal` | `compact`, `normal`, `relaxed` |
| `ui.theme` | string | `dark` | `auto`, `dark`, `light`, `high-contrast` |
| `ui.fontFamily` | string | `""` (system sans) | Validated family name |
| `ui.fontSize` | int | 11–18, default 12 | Scales UI chrome via `--ui-font-size` |
| `ui.language` | string | `auto` | `auto`, `en`, `ru` |

New settings must be added in three places: defaults and validation in
`core/src/settings.cpp`, IPC validation in `core/src/ipc/validate.cpp`, and the
typed interface plus UI in `apps/desktop/src/stores/settings.ts`.

The shell reads only the persisted `ui.theme` id for first-paint boot and the
normalized system locale for `auto`; core remains the owner and validator of
both values. `shell/listFonts` enumerates fontconfig families read-only for the
font picker.

---

## 4. Repository map

```
CMakeLists.txt              top-level build, hardening, tests toggle
core/
  include/anvil/            public headers (settings, error, version, …)
  src/ipc/                  JSON-RPC framing, validation, server dispatch
  src/lsp/                  transport, client, manager, URI mapping
  src/security/             sandbox (bwrap), seccomp, (future secrets)
  src/workspace/            workspace jail, inotify watcher
  src/util/                 fs, process, strings, logging, thread pool
  src/settings.cpp          settings schema and persistence
  src/recent.cpp            recent projects + workspace state
  src/git_branch.cpp        read-only .git/HEAD inspection
  src/main.cpp              CLI: --stdio, --self-test, --dev
  tests/                    55 GoogleTest cases + fake LSP server
shell/src/main.cpp          GTK3/WebKitGTK shell, bridge, dialogs, E2E
apps/desktop/
  src/layout/               TitleBar, ActivityBar, Sidebar, EditorArea,
                            RightPanel, StatusBar, palette, modals, toasts
  src/features/             Explorer, Search, SCM, AI, MonacoEditor
  src/editor/               models, theme, providers, markers, text sync
  src/stores/               Zustand: core, workspace, editor, lsp, settings, ui
  src/ipc/                  typed bridge, API wrappers, types
  src/commands.ts           command registry (message-keyed titles)
  src/actions.ts            user-facing workflows
  src/i18n/                 typed en/ru dictionaries, t(), pluralization
  src/theme/                theme resolution and font application
  scripts/                  contrast and untranslated-text gates
  src/styles.css            theme-scoped design tokens and layout
assets/                     icon PNG/SVG
packaging/debian/           control, changelog, copyright, desktop, metainfo,
                            maintainer scripts, bwrap AppArmor profile
scripts/                    build, test, package, verify, E2E, SBOM, sysroot
tests/fixtures/sample_cpp/  C++ fixture used by E2E (run prepare-fixtures.sh)
tests/e2e/                  shell allowlist test page
docs/                       ARCHITECTURE, BUILD, benchmarks, threat-model,
                            RELEASE-CHECKLIST, adr/
prompts/masterprompt_0.0.1.md  original product specification (historical)
dist/                       built .deb, SBOM, signed APT repo (generated)
```

---

## 5. Build, run, test, package

All commands are run from the repository root.

### 5.1. Dependencies (Ubuntu 24.04)

```bash
sudo apt install -y build-essential cmake ninja-build pkg-config \
  nlohmann-json3-dev libspdlog-dev libgtest-dev \
  libwebkit2gtk-4.1-dev libgtk-3-dev nodejs npm
# runtime for real C++ intelligence:
sudo apt install -y bubblewrap clangd-18
```

If you cannot use `sudo` (restricted container), extract packages locally:

```bash
./scripts/setup-sysroot.sh
source .sysroot/env.sh
```

### 5.2. Build

```bash
./scripts/build.sh release     # optimized binaries + production UI
./scripts/build.sh dev         # debug binaries, tests enabled
```

Outputs: `build/<mode>/core/anvil-core`, `build/<mode>/shell/anvil`,
`apps/desktop/dist/`.

### 5.3. Run from the build tree

```bash
./scripts/run-dev.sh /path/to/project
```

For a container without user namespaces, the development LSP override is
documented in `docs/BUILD.md` §4. It is compiled out of release builds.

### 5.4. Tests and checks

```bash
./scripts/test.sh              # 55 core/security/integration tests
./scripts/prepare-fixtures.sh  # generate the fixture's compile_commands.json
./scripts/test-e2e.sh          # shell workspace-allowlist + bridge smoke
cd apps/desktop && npm run typecheck && npm run build
cd apps/desktop && npm run check:contrast   # WCAG AA gate for all three themes
cd apps/desktop && npm run check:i18n       # fails on literal untranslated UI text
```

The fixture build directory is git-ignored, so a fresh clone must run
`prepare-fixtures.sh` before any clangd E2E/screenshot that relies on project
include paths. The script only configures CMake; it does not compile or execute
project scripts.

Package verification:

```bash
./scripts/build-deb.sh
./scripts/verify-deb.sh dist/anvil_0.0.1-1_amd64.deb
```

`build-deb.sh` runs tests, release build, staging, SBOM, dependency
calculation, `dpkg-deb`, signed APT repo generation, and the verifier.

### 5.5. Current measured results (reference VM)

| Metric | Value | Target | Status |
|---|---|---|---|
| Startup to interactive welcome | ~0.85 s | < 0.5 s | Missed |
| Idle PSS (no workspace) | ~255 MiB | < 300 MiB | Met (PSS) |
| Full stack with clangd | > 800 MiB RSS | < 300 MiB | Missed |
| `.deb` size | ~1.35 MiB | < 15 MiB | Met |
| Core/security tests | 55/55 | all | Met |

Details and methodology: `docs/benchmarks.md`. Do not quote these numbers
without re-measuring on the target machine.

On `devel/0.0.2`, startup and memory have **not** been re-measured yet. The UI
bundle grew from ~3.62 MB to ~3.67 MB (+1.4%) and CSS from ~175.6 KB to
~183.5 KB (+4.5%); the localization dictionaries and three Monaco themes are
bundled, with no new network or runtime downloads. Re-measure before shipping
0.0.2.

---

## 6. Current state — 0.0.1

### 6.1. Working features

- Open a folder, lazy file tree, create/rename/delete, inotify refresh.
- Tabs, dirty state, atomic saves, external-change detection, view-state
  restore, workspace state persistence.
- Monaco with Anvil dark theme, C++ highlighting, minimap, bracket
  colorization, per-file models.
- clangd lifecycle in a sandbox: hover, completion, definition, references,
  signature help, formatting request, diagnostics.
- Problems panel with click-to-navigate; output panel; command palette;
  quick open; settings; security status; about.
- Status bar: branch, problem counts, cursor, indentation, EOL, encoding,
  language, clangd state, sandbox state.
- Appearance: Anvil Dark, Anvil Light, and Anvil High Contrast themes with
  `auto` system following; UI and editor font family/size, ligatures, and line
  height, persisted in settings. All text pairs pass WCAG AA.
- Localization: English and Russian UI dictionaries with live switching
  (`Automatic`, English, Русский), typed message keys, pluralization, localized
  dates, and `<html lang>`. Technical diagnostics/logs stay in their original
  language; native GTK dialogs follow the system locale.
- Smoke/E2E/screenshot modes; shell workspace allowlist E2E. 0.0.2 evidence
  screenshots live in `docs/screenshots/0.0.2/`.
- `.deb` with desktop entry, AppStream metadata, icons, SBOM, threat model,
  bwrap AppArmor profile, signed local APT repository.

### 6.2. Explicit non-features (not bugs)

Terminal, debugger/DAP, build/run/CMake configure, Git staging/commit/diff,
AI provider, plugins, auto-update, Rust/Python servers, Tree-sitter index.
Their panels show a phase notice; the Start button in run configuration is
disabled.

### 6.3. Known limitations carried from 0.0.1

| ID | Limitation | Impact | Plan |
|---|---|---|---|
| L-01 | Startup ~0.85 s, above 500 ms goal | Perceived slowness | Lazy Monaco bundle / custom URI scheme; target 0.0.3 |
| L-02 | Full stack with clangd exceeds 300 MiB | Memory pressure | Tune clangd background index, lazy UI; investigate in 0.0.3 |
| L-03 | Single classic UI bundle (`file://` constraint) | No code splitting | Custom URI scheme in 0.0.3 |
| L-04 | `lintian` unavailable in reference environment | Package lint unverified | Run on a clean Ubuntu box before public release |
| L-05 | Clean-VM `apt install` not executed in container | Install path unverified end-to-end | Owner to verify on a real Ubuntu 24.04 machine |
| L-06 | Demo APT signing key | Not for public distribution | Generate an offline release key before publishing |
| L-07 | Container blocks user namespaces | Release LSP fails closed there | AppArmor profile shipped; verify on a real desktop |
| L-08 | Native GTK dialog labels follow the system locale, not `ui.language` | File/folder dialogs stay in the system language | Documented; no fix planned for 0.0.2 |

Owner-reported bugs belong in §8, not here. If the owner promotes a limitation
to a required fix, convert it to a `BUG-xxx` entry.

---

## 7. Roadmap (global)

This is the maintained global plan. Every item from `prompts/masterprompt_0.0.1.md`
§9 is tracked here with a status and, where relevant, evidence or a note. If this
section and the master prompt disagree about **status or ordering**, this file
wins: it is updated per release, the master prompt is historical.

Phases are themes of work, not version numbers. Releases are cut from phases.

**Status markers**

| Marker | Meaning |
|---|---|
| `[x]` | Done and verified in a shipped build |
| `[~]` | Partially done — the note says what exists and what is missing |
| `[ ]` | Planned, not started |
| `[-]` | Replaced or dropped — the note says why |

### 7.1. Release map

The version numbers after 0.0.2 are **tentative**. The owner sets release
numbers and scope; this table only records the intended order so a new session
knows what comes next.

| Release | Theme | Contents | Status |
|---|---|---|---|
| **0.0.1** | Phase 1 MVP | C++ editing, clangd intelligence, sandbox, `.deb` | Shipped 2026-11-13 |
| **0.0.2** | Personalization + environment + agent | IMP-001…IMP-005 (§7.6) | In progress — IMP-001/002/004 delivered on `devel/0.0.2`; IMP-003/005 pending |
| 0.0.3 (tentative) | Multi-language + navigation | Phase 2 + performance debt L-01…L-03 | Planned |
| 0.0.4 (tentative) | Build / run / debug | Phase 3 | Planned |
| 0.0.5 (tentative) | Parity + ecosystem | Phase 4 remainder | Planned |
| 1.0.0 (tentative) | Public release | §7.9 criteria | Planned |

### 7.2. Phase 1 — MVP (shipped in 0.0.1)

Goal: open a real C++ project, edit it, see diagnostics, navigate to
definitions.

- [x] Single-file HTML prototype as the design reference (preserved in
      masterprompt §14; not shipped).
- [~] Port the prototype to React + TypeScript + Zustand + Monaco.
      **Tauri host replaced by a native C++ WebKitGTK shell** — see ADR-0001
      and §7.8. The React/Monaco port itself is complete.
- [x] C++ sidecar with a JSON-RPC loop and base `fs/*` methods
      (`core/src/ipc/server.cpp`, `validate.cpp`).
- [x] clangd started in a bubblewrap + seccomp sandbox
      (`core/src/security/sandbox.cpp`, `seccomp.cpp`). Production sandbox
      argv and deny rules are unit-tested; real bwrap cannot run in the
      reference container (L-07), so desktop verification is still open.
- [x] Monaco ↔ clangd: hover, completion, definition, references,
      signature help, formatting request, `publishDiagnostics`.
- [x] File tree on the real filesystem with inotify watching.
- [x] Tab bar on real open files, with dirty state, state restore, and
      external-change handling.
- [x] Problems panel on real diagnostics, click-to-navigate.
- [x] Status bar: line/column, language, encoding, EOL, clangd status,
      sandbox status, branch, problem counts.
- [x] Command palette with 30+ real commands and keyboard shortcuts
      (master prompt asked for ~10).
- [x] IPC validation for every method, with security tests
      (`core/tests/test_validate.cpp`).
- [x] Threat model document (`docs/security/threat-model.md`).
- [x] First `.deb` package with SBOM, AppStream, icons, bwrap AppArmor
      profile, signed local APT repository (`scripts/build-deb.sh`).
- [x] Shell workspace allowlist: only CLI, native dialog, or recent projects
      can become a workspace root (`scripts/test-e2e.sh`).

**Exit criterion status:** open a real CMake project, edit, save, see errors,
navigate by symbol — verified end-to-end with real clangd (screenshots in
`docs/screenshots/`). The clean-VM `apt install` check is still outstanding
(L-05).

### 7.3. Phase 2 — Multi-language and navigation

Goal: Anvil stops being C++-only and gains project-wide navigation.

- [ ] `rust-analyzer` for Cargo projects, in the sandbox.
- [ ] `pylsp`/`pyright` for Python projects, in the sandbox.
- [~] Symbol outline panel. The core method `lsp/documentSymbols` and a typed
      API wrapper exist; there is no outline UI yet.
- [ ] Go to Symbol in File (`Ctrl+Shift+O`) and in Workspace (`Ctrl+T`).
- [x] Find All References with grouping. Delivered early in 0.0.1 as
      `Shift+F12` plus a grouped references dialog (not a Monaco peek view).
- [~] Rename symbol. The core implements `lsp/rename` with workspace-edit
      normalisation and validation tests; there is no UI command or typed
      wrapper yet. Do not call it “done” until the UI applies edits safely.
- [~] Global search. File-name search over the workspace exists
      (`fs/searchFiles`); full-text content search via `ripgrep` (argv style)
      is not implemented.
- [~] Multi-cursor and column selection. Monaco provides multi-cursor and
      column selection out of the box; Anvil has not documented, verified, or
      customised them.
- [ ] LSP completion for languages other than C/C++ in the UI providers
      (providers are currently registered for `cpp` only).

**Exit criterion (from the master prompt):** a Rust or Python project gets
completion, diagnostics, and navigation inside Anvil, sandboxed, with no
C++ regressions.

### 7.4. Phase 3 — Build, run, debug

Goal: build, run, and debug a real CMake project without leaving Anvil.

- [ ] CMake configuration: parse and run CMake only on an explicit, confirmed
      user action, inside the sandbox.
- [ ] Build runner (`cmake --build`, `cargo build`, `make`), streamed to the
      panel.
- [ ] Build stderr parsed into Problems with file:line mapping.
- [~] Run configurations UI. The dialog exists as a disabled Phase 3 preview;
      there is no real launch logic.
- [ ] DAP: launch, attach, breakpoints (gutter + panel), step, continue,
      pause, stop.
- [ ] Debug panel: call stack, variables tree, watch, breakpoints.
- [ ] Integrated terminal (xterm.js + native PTY, isolated).

**Exit criterion:** open a CMake project, build, run under GDB, hit a
breakpoint, inspect variables — all inside Anvil.

### 7.5. Phase 4 — Parity and ecosystem

Goal: an IDE that can replace a heavyweight one for daily C++ work, plus an
extension surface.

- [ ] Git panel: stage/unstage/commit/branch switch/diff.
- [ ] Diff editor (Monaco `createDiffEditor`) driven by Git status.
- [ ] Refactorings beyond LSP: extract function, inline variable
      (through `libclang` in the core).
- [ ] Global index (Tree-sitter + `libclang`) for instant project-wide search.
- [ ] AI assistant panel. **IMP-003** brings a Codewhale connector to 0.0.2
      as an opt-in, read-only MVP; it is not started yet. Other providers and
      agentic tool use are not scheduled.
- [ ] Plugin SDK with a WASM sandbox and explicit capabilities.
- [x] Light theme and high-contrast theme. **IMP-002** delivered theme
      selection in 0.0.2: Anvil Dark, Anvil Light, Anvil High Contrast, and
      `auto`, with Monaco variants and an AA contrast gate.
- [x] Settings UI. Shipped in 0.0.1 as a modal with editor, C/C++, and
      security sections; 0.0.2 adds Appearance (language, theme, fonts).
- [ ] Auto-updater through APT, with explicit user consent.
- [x] Localization. **IMP-001** delivered English/Russian switching in 0.0.2
      with typed dictionaries, pluralization, and `<html lang>`. The structure
      supports more languages; none are scheduled yet.
- [ ] Secret storage through libsecret/D-Bus. Nothing in 0.0.1 needs secrets;
      this becomes mandatory before Git credentials or API keys are stored.
- [x] Security audit log with redaction (shipped early in 0.0.1).
- [ ] Dependency auditing in CI (`npm audit` is manual today; C++ and Rust
      dependency audits are not wired into a pipeline).
- [x] Font selection (**IMP-004**, owner request, not in the master prompt):
      delivered in 0.0.2 for UI and editor families, sizes, ligatures, and line
      height, with fontconfig enumeration in the shell.

### 7.6. Release 0.0.2 scope (pulled-forward Phase 4 items)

Theme: **“Make it yours, make it resolve your environment, connect an agent.”**

| ID | Item | Type | Depends on | Status | Exit criterion |
|---|---|---|---|---|---|
| IMP-005 | Automatic dependency/environment resolution (ROS 2 / rclcpp) | Improvement | — | PLANNED | ROS 2 fixture resolves `rclcpp` after a confirmed environment profile; no auto-execution |
| IMP-002 | Theme selection in Settings | Improvement | Appearance foundation | DONE (0.0.2) | Theme switches live and persists, AA contrast |
| IMP-004 | Font selection (UI + editor) | Improvement | Appearance foundation | DONE (0.0.2) | Fonts switch live, persist, fallback safe |
| IMP-001 | Russian UI localization | Improvement | — | DONE (0.0.2) | Language switch works without restart; all UI strings translated |
| IMP-003 | Connect Codewhale agent to the AI panel | Improvement | IMP-001 optional | PLANNED | Opt-in prompt/response with streaming; no automatic edits |

**Recommended order:** appearance (IMP-002 + IMP-004) and localization
(IMP-001) are delivered on `devel/0.0.2`. Next: IMP-005 (user-blocking,
security-sensitive, needs the most review), then IMP-003 (needs an ADR, a
security review, and a fake-Codewhale transport test).

**0.0.2 exit criteria (all required):**

- [x] IMP-001, IMP-002, IMP-004 meet their acceptance criteria in §9 (delivered
      on `devel/0.0.2`).
- [ ] IMP-003 and IMP-005 meet their acceptance criteria in §9.
- [x] `./scripts/test.sh` passes (55/55); new settings validation tests cover
      new external input.
- [ ] New tests cover the environment profile and the AI transport.
- [x] `npm run typecheck && npm run build` passes.
- [x] UI gates pass: `npm run check:contrast` (three themes, AA) and
      `npm run check:i18n` (no literal untranslated JSX text).
- [x] Screenshots in `en` and `ru` exist under `docs/screenshots/0.0.2/`.
- [ ] `./scripts/build-deb.sh` produces a `.deb` under 15 MiB and
      `./scripts/verify-deb.sh` passes.
- [ ] Startup and idle memory are not worse than 0.0.1 by more than 10%; record
      the numbers in `docs/benchmarks.md`.
- [x] `PROJECT.md` statuses and §11 changelog are updated.
- [ ] `docs/security/threat-model.md` is reviewed for IMP-003/IMP-005.
- [x] No invariant from §2 is weakened by the delivered changes.

**Explicitly out of 0.0.2:** build execution, debugger, terminal, Git writes,
plugin sandbox, auto-update, Rust/Python servers. Do not start them unless the
owner re-scopes.

### 7.7. Cross-cutting workstreams

These are not tied to a phase and can be scheduled in any release.

- [ ] CI pipeline: build, tests, typecheck, package verification on every
      push (GitHub Actions or equivalent).
- [ ] `lintian` clean run and a clean-Ubuntu `apt install` / `remove` /
      `purge` verification before any public release (L-04, L-05).
- [ ] Production GPG signing key for the APT repository and a published,
      HTTPS-served repository (L-06).
- [ ] Startup and memory debt L-01…L-03: lazy Monaco, custom URI scheme,
      clangd background-index tuning.
- [~] SBOM generation. Shipped per release; not yet published as a separate
      release asset.
- [ ] Wayland verification and a screen-reader/accessibility audit.
- [ ] ARM64 packaging.
- [ ] Opt-in crash reporting (never on by default, never with source content).
- [ ] Documentation localization (the handbook stays English; the master
      prompt is Russian).

### 7.8. Recorded deviations and decisions

1. **Tauri replaced by a native C++ WebKitGTK shell.** The owner instructed
   C++ for the implementation and the reference environment has no Rust
   toolchain. Rationale and consequences: `docs/adr/0001-native-cpp-shell.md`.
   The core and IPC contract are unaffected; the shell is replaceable.
2. **Phase 4 items pulled into 0.0.2.** The owner requested localization,
   themes, fonts, a Codewhale connector, and ROS 2 environment resolution.
   The owner's instruction overrides the phase placement. Remaining Phase 4
   items stay in §7.5.
3. **References, signature help, formatting, and the settings UI shipped in
   0.0.1**, although the master prompt listed references in Phase 2 and
   settings in Phase 4.
4. **No `execve` deny in the seccomp profile.** bubblewrap installs the filter
   immediately before exec'ing the payload, so an exec-deny rule would prevent
   clangd from starting. Isolation relies on namespaces, `no_new_privs`, a
   read-only workspace, and network denial. Documented in the threat model.
5. **Bundled classic UI script instead of lazy ES-module chunks.** `file://`
   cannot reliably load lazy module chunks; a custom URI scheme is the planned
   fix (L-03).

### 7.9. Milestones and the definition of 1.0

| Milestone | Meaning |
|---|---|
| 0.0.1 | Phase 1: a working C++ editor with clangd and a sandbox |
| 0.0.2 | Personalization, ROS 2 environment, Codewhale connector |
| 0.1.0 (tentative) | Phase 2: multi-language and navigation |
| 0.2.0 (tentative) | Phase 3: build, run, debug, terminal |
| 1.0.0 (tentative) | Phase 4 essentials + production release criteria |

**1.0.0 is not “all phases done”.** It means:

- [ ] A real CMake C++ project can be built, run, and debugged inside Anvil.
- [ ] At least one non-C++ language works end-to-end through LSP.
- [ ] Startup and idle memory meet the targets in §5.5, or the targets are
      formally revised by the owner with measurements.
- [ ] Package installs and purges cleanly on a clean Ubuntu 24.04 VM.
- [ ] `lintian` is clean; the APT repository uses a production signing key.
- [ ] Threat model reviewed against the shipped feature set.
- [ ] No known P0 or P1 bugs open.

When a release starts, generate `prompts/masterprompt_<version>.md` from §7.6
and §9 for that release. `PROJECT.md` stays the living status file; the
masterprompt is the frozen assignment for that version.

---

## 8. Bugs

<!-- BEGIN OWNER BUGS -->
<!-- Add one block per bug. Keep IDs stable. Do not delete fixed bugs. -->
<!-- END OWNER BUGS -->

**Status vocabulary:** `NEW`, `TRIAGED`, `IN_PROGRESS`, `FIXED (version)`,
`CANNOT_REPRODUCE`, `WONTFIX`, `DEFERRED (target)`.

**Severity:**
- `P0` — data loss, security bypass, crash on startup.
- `P1` — core workflow broken (open/save/intelligence), no workaround.
- `P2` — broken with a workaround, visual/UX defect.
- `P3` — minor polish.

### 8.1. Bug template (copy this block)

```
### BUG-000 — Short title

- Status: NEW
- Severity: P1
- Area: editor | explorer | lsp | fs | settings | packaging | shell | ui
- Found in: 0.0.1
- Environment: Ubuntu 24.04, clangd 18, X11/Wayland

**Steps to reproduce**
1.
2.
3.

**Expected**

**Actual**

**Evidence**
- Logs / screenshot / file sample (no secrets, no private code).

**Notes**
- Suspected cause, related files, workaround.
```

*No bugs recorded yet.*

---

## 9. Improvements

<!-- BEGIN OWNER IMPROVEMENTS -->
<!-- Add one block per improvement. Keep IDs stable. Owner may write in Russian. -->
<!-- END OWNER IMPROVEMENTS -->

**Status vocabulary:** `NEW`, `TRIAGED`, `PLANNED (version)`, `IN_PROGRESS`,
`DONE (version)`, `DEFERRED (version)`, `REJECTED`.

Every improvement must state: goal, in-scope, out-of-scope, UX, technical
approach, security constraints, acceptance criteria, verification.

---

### IMP-001 — Russian UI localization

- **Status:** DONE (0.0.2)
- **Priority:** high
- **Area:** UI + shell + settings
- **Why:** the owner and a significant part of the target audience work in
  Russian; English-only UI slows daily use.

- **Entry points:** `core/src/settings.cpp` (`is_known_key`, `reset_defaults`,
  `validate_assignment`), `core/src/ipc/validate.cpp` (`parse_settings_set`),
  `shell/src/main.cpp` (`Json boot`, `handle_shell_message`, argument parsing),
  `apps/desktop/src/bootstrap.ts` (`BootInfo`),
  `apps/desktop/src/stores/settings.ts` (`Settings`, `defaultSettings`), new
  `apps/desktop/src/i18n/`, and every component under
  `apps/desktop/src/layout/` and `apps/desktop/src/features/`, plus
  `apps/desktop/src/commands.ts` and `apps/desktop/src/actions.ts`.

**Goal:** a fully translated Russian interface with a language switcher and
persisted choice, without translating technical diagnostics.

**In scope**
- Typed message dictionaries (`en`, `ru`) bundled into the UI; no runtime
  downloads and no project-provided translations.
- A `t(key, params)` helper and a React hook/context for the current locale.
- Settings → Appearance → Display language: `Automatic (system)`, `English`,
  `Русский`. Persist as `ui.language` ∈ `auto | en | ru`.
- System locale detection: the shell passes the normalized `LC_ALL` /
  `LC_MESSAGES` / `LANG` value in `window.__ANVIL_BOOT__`; `auto` resolves to
  `ru` for `ru*`, otherwise `en`.
- Switching applies immediately without restart; update `<html lang>`.
- Translate all UI chrome: menus, activity bar, explorer, search, SCM, AI
  panel, welcome, tabs context, panels, status bar, palette, quick open,
  settings, about, security, run configuration, references, prompts, confirms,
  toasts, error mapping, empty states, phase notices.
- Pluralization for counts (files, references, problems) and locale-aware
  date/time via `Intl`.
- Keep keyboard shortcut labels and product/CLI names unchanged.

**Out of scope**
- Translating clangd/compiler diagnostics, log files, `--help`, or developer
  documentation. Technical output stays in the tool's language.
- Native GTK dialog button labels: they follow the system locale, not the app
  setting. Document this.
- Additional languages beyond `en` and `ru` (structure must allow them).
- Translating the master prompt.

**Technical approach**
- Create `apps/desktop/src/i18n/` with `en.ts`, `ru.ts`, and `index.ts`
  (typed keys; compile-time completeness check by
  `satisfies Record<MessageKey, string>`).
- `useI18n()` reads `ui.language` from the settings store and boot locale.
- Replace hardcoded strings component by component; add a test/lint that fails
  on obvious untranslated JSX text in `layout/` and `features/` (allowlist for
  product names and shortcuts).
- Core: add `ui.language` to `Settings` and to `parse_settings_set` validation
  (`auto|en|ru`). Shell: add `--lang` and `ANVIL_LANG` override for tests,
  pass `locale` in boot info.
- Accessibility: set `document.documentElement.lang`; verify screen-reader
  labels are translated.

**Security constraints**
- Dictionaries are compiled into the bundle; no filesystem or network loading.
- No translated string may be inserted via `innerHTML`; React text nodes only.
- Locale values are validated before persistence and before use.

**Acceptance criteria**
1. Switching `en → ru → auto` changes every visible string without restart.
2. Choice persists in `settings.json` and survives restart.
3. `auto` follows the system locale (`ru_RU.UTF-8` → Russian).
4. No untranslated UI string remains (manual sweep + automated check).
5. Long Russian strings do not clip or overlap at 1280×800 and 1920×1080.
6. Diagnostics and logs remain in their original language.
7. `npm run typecheck`, `npm run build`, core tests pass.
8. E2E screenshots: welcome, editor, problems, settings in both languages.

**Verification**
`npm run typecheck && npm run build`; `./scripts/test.sh`; E2E screenshots;
manual sweep of every panel with the language set to Russian.

**Delivered evidence (0.0.2):**
- `apps/desktop/src/i18n/{en,ru,index}.tsx`: typed dictionaries,
  `satisfies Record<MessageKey, string>`, pluralization, `Intl` dates.
- `ui.language` is validated in `core/src/settings.cpp` and
  `core/src/ipc/validate.cpp` (tests in `test_settings.cpp` /
  `test_validate.cpp`); the shell passes normalized `LC_ALL`/`LC_MESSAGES`/
  `LANG` plus `--lang`/`ANVIL_LANG` overrides in boot info.
- `npm run check:i18n` finds no literal untranslated JSX text or attributes;
  screenshots: `docs/screenshots/0.0.2/{ru-light-editor,ru-light-settings,
  en-dark-settings,ru-light-large-font}.png`.
- `./scripts/test.sh`: 55/55; `npm run typecheck && npm run build`: pass.

**Open questions**
- Should `auto` also consult `LANGUAGE`? (Proposal: yes, after `LC_ALL`.)
- Should the language switcher also appear in the command palette?
  (Proposal: yes, two commands.)

---

### IMP-002 — Theme selection in Settings

- **Status:** DONE (0.0.2)
- **Priority:** high
- **Area:** UI (appearance), settings
- **Why:** the dark-only palette is a blocker for users in bright environments
  and for accessibility needs.

- **Entry points:** `apps/desktop/src/styles.css` (`:root` tokens),
  `apps/desktop/src/editor/theme.ts` (`ensureTheme`, `editorOptions`,
  `applyEditorOptions`), new `apps/desktop/src/theme/apply.ts`,
  `apps/desktop/src/stores/settings.ts`,
  `apps/desktop/src/layout/Modals.tsx` (Settings sections),
  `core/src/settings.cpp` and `core/src/ipc/validate.cpp` for `ui.theme`.

**Goal:** a Settings → Appearance tab with a theme picker, live switching, and
persistence.

**In scope**
- Themes: **Anvil Dark** (current, default), **Anvil Light**, **Anvil High
  Contrast** (dark background, stronger borders, AA+).
- `ui.theme` ∈ `auto | dark | light | high-contrast`; `auto` follows
  `prefers-color-scheme`.
- All existing CSS tokens become theme-scoped (`:root[data-theme="light"]`,
  etc.); no component-level hardcoded colors.
- Monaco theme variants registered for each app theme; switching updates the
  editor without recreating models.
- Apply the persisted theme before first paint (boot info or an inline
  pre-render script) to avoid a flash.
- Preview cards in Settings with a small swatch of background/foreground/accent.

**Out of scope**
- User-defined theme files, per-language themes, marketplace themes.
- Light theme for embedded terminals (no terminal yet).

**Technical approach**
- Extend `Settings` with `ui.theme` and validation.
- `theme/apply.ts`: sets `data-theme` on `<html>`, updates Monaco theme, updates
  `color-scheme`.
- Keep the existing Anvil dark palette values unchanged for the dark theme.
- Add a contrast checker script/test for token pairs used for text.

**Security constraints**
- Theme ID is an allowlisted enum; never interpolate CSS from user input.
- No remote fonts or images in themes.

**Acceptance criteria**
1. Each theme applies instantly across shell chrome, panels, modals, palette,
   editor, and status bar.
2. Choice persists and is restored before the first frame.
3. Text/background pairs meet WCAG AA in every theme.
4. Monaco decorations, diagnostics, selections, and suggest widget remain
   legible in light mode.
5. Screenshots for all three themes at 1920×1080.

**Verification**
Typecheck/build, E2E screenshots per theme, manual contrast check.

**Delivered evidence (0.0.2):**
- `ui.theme` (`auto|dark|light|high-contrast`) validated in core + IPC;
  `theme/apply.ts` resolves `auto`, sets `data-theme` and `color-scheme`;
  boot info applies the persisted theme before the first frame.
- CSS tokens are theme-scoped; Monaco registers `anvil-dark`, `anvil-light`,
  and `anvil-high-contrast`.
- `npm run check:contrast` passes 4.5:1 for all three themes; screenshots:
  `docs/screenshots/0.0.2/{ru-light-editor,ru-high-contrast-editor,
  en-dark-welcome}.png`.

---

### IMP-003 — Codewhale agent in the AI assistant

- **Status:** PLANNED (0.0.2)
- **Priority:** high
- **Area:** UI AI panel + core process management + security
- **Why:** the owner uses Codewhale and wants in-IDE assistance without copying
  code into a separate chat.

- **Entry points:** `core/src/ipc/server.cpp` (`Impl::dispatch`) for a new
  `ai/` method namespace, `core/src/ipc/validate.cpp` for their validators,
  `core/src/security/sandbox.cpp` (`SandboxSpec`) for the child profile,
  `core/src/util/process.cpp` for argv spawn and streaming,
  `apps/desktop/src/features/AiPanel.tsx` (currently a Phase 4 placeholder),
  new `apps/desktop/src/stores/ai.ts`, `apps/desktop/src/ipc/api.ts` and
  `apps/desktop/src/ipc/bridge.ts` for streamed events,
  `apps/desktop/src/commands.ts` for toggle commands, and
  `apps/desktop/src/layout/Modals.tsx` for the one-time consent dialog.

**Goal:** connect a locally installed Codewhale agent to the Anvil AI panel,
opt-in, with explicit context control and no automatic edits.

**Proposed integration (needs an ADR before coding)**
- Detect `codewhale` on `PATH` (or a configured absolute path) from the shell or
  core; expose availability in `system/info`.
- **MVP transport:** spawn `codewhale exec --output-format stream-json` with
  fixed argv, workspace cwd, a minimal environment (`PATH`, `HOME`, `XDG_*`,
  `LANG`), and no inherited Anvil/secret variables; stream parsed events into
  the panel. No tools, no automatic approvals, no file writes to the workspace.
- **Later transport:** `codewhale mcp-server` over stdio with an approval
  bridge for tool calls. Do not enable tools in 0.0.2.
- Context is explicit: active file, selection, and `@file` mentions chosen by
  the user. Show a context chip list and total size before sending.
- Provider setting: `ai.provider` ∈ `none | codewhale`; default `none`.
  `ai.codewhalePath` optional absolute path.
- Codewhale owns its own credentials/auth; Anvil never reads or stores API
  keys. If Codewhale is not authenticated, show its own error message.

**In scope**
- AI panel UI: message list, streaming assistant output (Markdown, code
  blocks), stop button, clear, context chips.
- Shell/core: process lifecycle, timeout, cancellation, stderr → output panel,
  redaction of secrets in logs, audit event per prompt (metadata only, never
  content).
- Error states: binary missing, not authenticated, provider unreachable,
  rate-limited, cancelled.
- A one-time consent dialog per workspace explaining what is sent and where.

**Out of scope**
- Automatic file edits, multi-step agentic tool use, repository-wide context
  crawling, telemetry, cloud Anvil accounts.
- Other providers (OpenAI/Anthropic/Ollama) — keep the provider interface
  extensible but do not implement them in 0.0.2.
- Sending secrets or `.env` contents; explicit denylist and user warning.

**Security constraints**
- The consent dialog must name the executable, the workspace, and the fact
  that source excerpts leave the machine to the configured model provider.
- Redact secrets from context before sending; never attach `.env`, key files,
  or `~/.ssh`, `~/.config/anvil` by default.
- Network is allowed only for the Codewhale child when the feature is enabled;
  Anvil itself still makes no network requests.
- The child runs with argv (no shell), `no_new_privs`, and a defined sandbox
  profile: read-only workspace, read-only system paths, writable only to
  Codewhale's own config/cache directories so it can keep its auth and
  sessions, and network access only when the feature is enabled.
- The AI consent dialog lists the exact executable and states that Codewhale
  reads its own credentials; Anvil never reads or forwards them.
- Every prompt/response metadata event goes to the audit log; content is not
  logged.
- A visible “AI is enabled” indicator in the panel and status bar.

**Acceptance criteria**
1. With `codewhale` installed and authenticated, the user can enable the panel,
   attach the current selection, ask a question, and see a streamed answer.
2. Stop cancels the child and leaves the panel usable.
3. With `codewhale` missing, the panel explains how to install it and stays
   disabled; no crash.
4. With the feature disabled (default), no child process and no network.
5. No automatic edits; the workspace is not modified.
6. Audit log contains one entry per prompt with size/model metadata only.
7. The `.deb` does not depend on Codewhale (Suggests only, if anything).

**Verification**
Fake Codewhale executable test (stream-json fixture) in core tests; manual E2E
with the real binary; screenshots of consent, streaming, error, and stop.

**Open questions for the owner**
1. Should 0.0.2 allow Codewhale tools (`--auto`) with per-call approvals, or
   keep the MVP read-only? (Recommendation: read-only in 0.0.2.)
2. Should Anvil pass a model/provider flag explicitly, or use the user's
   Codewhale config? (Recommendation: use Codewhale config; no flags.)
3. Is `codewhale exec` stable enough, or should the MVP target
   `codewhale mcp-server` from the start? (Recommendation: `exec` MVP, MCP in
   0.0.3.)

---

### IMP-004 — Font selection

- **Status:** DONE (0.0.2)
- **Priority:** medium
- **Area:** UI (appearance), settings, shell
- **Why:** developers have strong font preferences; the current hardcoded stack
  ignores installed fonts such as JetBrains Mono or Fira Code.

- **Entry points:** `shell/src/main.cpp` (`handle_shell_message`, add the
  `shell/listFonts` method), `apps/desktop/src/styles.css` (`--font-ui`,
  `--font-code`), `apps/desktop/src/editor/theme.ts` (`editorOptions`),
  `apps/desktop/src/stores/settings.ts` and
  `apps/desktop/src/layout/Modals.tsx`,
  `core/src/settings.cpp` and `core/src/ipc/validate.cpp` for the new keys.

**Goal:** choose UI and editor fonts and sizes in Settings, with live preview,
persistence, and safe fallbacks.

**In scope**
- Settings → Appearance:
  - UI font family + size (11–18 px).
  - Editor font family + size (10–32 px, existing `editor.fontSize`).
  - Editor line height (compact/normal/relaxed) — optional.
  - Ligatures toggle (`editor.fontLigatures`).
- Font family picker lists installed fonts from fontconfig (`fc-list`), with a
  free-text field for a family name; monospace fonts are marked.
- Shell method `shell/listFonts` returns `{family, monospace}` entries from
  fontconfig; no network, no file access outside fontconfig.
- CSS variables `--font-ui`, `--font-code`; Monaco options updated live.
- Fallback chain always ends with a system font; missing font falls back
  silently with a toast.
- Persist `ui.fontFamily`, `ui.fontSize`, `editor.fontFamily`,
  `editor.fontLigatures`, `editor.lineHeight`.

**Out of scope**
- Bundling fonts (licensing/size), per-language fonts, font smoothing controls.

**Technical approach**
- Shell: enumerate fontconfig with `FcFontList` or `fc-list` via argv (prefer
  the library to avoid parsing). Bound the list (e.g., 300 families) and cache.
- UI: validated CSS custom properties; never build a style string from raw
  input. Validate family names against a conservative pattern and length.
- Monaco: `updateOptions({ fontFamily, fontSize, fontLigatures, lineHeight })`.
- Persist through `settings/set` (core validation) and restore on boot.

**Security constraints**
- Font family input is data, not CSS: reject `;`, `{`, `}`, `<`, `>`, quotes,
  newlines, and control characters; cap length at 128.
- No remote font loading; CSP already blocks it.
- `fc-list`/fontconfig is read-only.

**Acceptance criteria**
1. Selecting an installed font changes UI/editor immediately and persists.
2. Invalid or missing family falls back safely and shows a warning.
3. Sizes are clamped to their documented ranges.
4. Ligatures toggle affects the editor only.
5. Layout remains intact at 1280×800 with the largest allowed UI font.
6. No CSS injection is possible through the font field (unit tests).
7. Screenshots: default, JetBrains Mono/Fira Code if installed, large UI font.

**Verification**
Typecheck/build; unit tests for family validation; E2E screenshots; manual
font switch.

**Delivered evidence (0.0.2):**
- Settings → Appearance exposes UI/editor families, UI size (11–18), editor
  size, line height, and ligatures. `theme/fonts.ts` applies validated values
  through CSS variables; Monaco updates live.
- `shell/listFonts` enumerates fontconfig families (bounded at 300) and marks
  monospace fonts; no network or project access.
- Family validation is shared by `settings.cpp` and `validate.cpp` and covered
  in `test_settings.cpp` / `test_validate.cpp` (injection attempts rejected).
- Screenshot at UI font 18 px: `docs/screenshots/0.0.2/ru-light-large-font.png`;
  panel tabs wrap and the status bar caps chrome text so nothing clips.

---

### IMP-005 — Automatic dependency and environment resolution (ROS 2 / rclcpp)

- **Status:** PLANNED (0.0.2)
- **Priority:** highest (blocks real ROS 2 work)
- **Area:** LSP manager, sandbox, settings, UI, packaging docs
- **Why:** opening a ROS 2 package shows “file not found” for `rclcpp` and
  other headers because clangd lacks the environment (`AMENT_PREFIX_PATH`,
  `CMAKE_PREFIX_PATH`, include dirs) that a sourced `setup.bash` provides.

- **Entry points:** `core/src/lsp/manager.cpp` (`find_compile_commands`,
  clangd `arguments`, `SandboxSpec` construction),
  `core/src/security/sandbox.cpp` (`SandboxSpec::extra_env`,
  `writable_paths`, `--ro-bind` emission), `core/src/util/process.cpp` (spawn),
  new `core/src/environment.cpp` + header for the profile store,
  `core/src/ipc/server.cpp` and `core/src/ipc/validate.cpp` for an `env/`
  method namespace,
  `apps/desktop/src/layout/Modals.tsx` (Configure environment + consent) and
  the diagnostics handler in `apps/desktop/src/App.tsx` for the banner,
  `scripts/stage-deb.sh` and `packaging/debian/anvil.install` to ship
  `/usr/lib/anvil/envprobe.sh` and the `anvil-envprobe` helper binary.

**Goal:** make a real ROS 2 workspace resolve its dependencies with one
explicit, auditable setup action, and stop reporting unresolved headers that
the configured environment provides.

**Design (three layers)**

1. **Compile-commands discovery (automatic, safe).**
   - Extend discovery to colcon layouts: `build/<pkg>/compile_commands.json`,
     `install/`, `log/latest_build/`, and multiple CMake build dirs.
   - Merge all discovered databases into a cache copy under
     `~/.cache/anvil/env/<workspace-hash>/compile_commands.json`, rewriting
     `directory`/`file` to absolute paths. This is **data transformation only**
     — no process execution.
   - Pass the directory containing the merged `compile_commands.json` to
     clangd via `--compile-commands-dir`.
   - Add `--query-driver` support in settings if needed for system includes.

2. **Environment profile (explicit, confirmed).**
   - New action: **“Configure environment…”** in Settings → C/C++ and in the
     Problems banner when unresolved headers are detected.
   - User picks a setup script via a native file dialog (e.g.
     `/opt/ros/jazzy/setup.bash`). Anvil shows the exact path, its SHA-256, and
     a warning that the script will execute.
   - On confirmation, execute a fixed Anvil-owned wrapper, never a constructed
     shell string:
     `bwrap … -- /usr/bin/bash --noprofile --norc /usr/lib/anvil/envprobe.sh <setup-script>`.
     The shipped `envprobe.sh` contains only `set -e; source "$1"; exec
     /usr/lib/anvil/anvil-envprobe --dump`; the user-selected script path is a
     positional argument, not interpolated text. The helper runs inside
     bubblewrap with read-only `/opt`, `/usr`, `/usr/local`, the workspace, the
     wrapper, and the selected script, and no network. `anvil-envprobe` filters
     the environment to the whitelist and emits NUL-separated `KEY=VALUE`
     pairs.
   - Store only a whitelisted delta in
     `~/.config/anvil/environments/<id>.json`:
     `AMENT_PREFIX_PATH`, `CMAKE_PREFIX_PATH`, `COLCON_PREFIX_PATH`,
     `LD_LIBRARY_PATH`, `PKG_CONFIG_PATH`, `PYTHONPATH`, `ROS_DISTRO`,
     `ROS_VERSION`, `RMW_IMPLEMENTATION`, plus the script path/hash/timestamp.
   - Invalidate the profile when the script hash changes; prompt to recapture.
   - Pass the whitelisted variables into the clangd sandbox and add **read-only**
     binds for every prefix under `/opt`, `/usr`, `/usr/local`, and (if the
     user explicitly approved it) the ROS workspace under `$HOME`. Writable
     paths never change: only the private cache and the workspace stay
     writable as before.
   - Audit: script path, hash, variable names (not values), timestamp.

3. **Diagnostics UX.**
   - When a `.cpp` file reports missing headers matching known ROS patterns,
     show an actionable banner: “ROS 2 environment is not configured.
     Configure environment…”.
   - After the profile is active, show a small status-bar chip
     (e.g. `ROS jazzy`) and list the profile in Settings.

**In scope**
- ROS 2 / colcon detection, environment profile, sandbox read-only binds,
  merged compile commands, settings UI, tests and fixture.
- Generic enough to support other prefix-based SDKs (e.g. `/opt/custom`).

**Out of scope**
- Running `colcon build`, `cmake`, `apt install`, or dependency installation.
  That is Phase 3 build integration and must remain a separate confirmed action.
- Automatically sourcing scripts on workspace open. **Never do this.**
- Auto-generating `compile_commands.json` (requires a build; Phase 3).

**Security constraints**
- Setup scripts execute only after an explicit confirmation naming the path and
  hash; no auto-run, no default profile, no silent retry.
- The helper runs in a sandbox with no network and read-only system paths.
- Capture only whitelisted variables; never log values that may contain tokens.
- Extra read-only binds are restricted to canonical absolute prefixes that
  exist and are under trusted roots; writable mounts are unchanged.
- The release build still fails closed if bubblewrap is unavailable.

**Acceptance criteria**
1. Fixture: a minimal ROS 2 package (`package.xml`, `#include <rclcpp/rclcpp.hpp>`,
   `compile_commands.json`) shows unresolved-header diagnostics before setup.
2. After “Configure environment…” with `/opt/ros/<distro>/setup.bash`,
   diagnostics clear and hover/definition work for `rclcpp`.
3. The profile persists across restarts and is reused automatically (without
   re-running the script).
4. Changing the script invalidates the profile and asks to recapture.
5. Cancelling the consent dialog changes nothing.
6. Sandbox remains read-only for the workspace and system prefixes; clangd has
   no network.
7. Audit log records profile creation with path + hash and no secret values.
8. Tests: merged compile-commands builder, env-var whitelist, hash
   invalidation, sandbox bind construction, malicious script path rejection.
9. A generic non-ROS fixture still works exactly as in 0.0.1.

**Verification**
Unit tests for the three layers; E2E with a ROS 2 fixture (headers can be faked
with a stub `rclcpp` tree if the real distro is unavailable); screenshots of the
banner and resolved diagnostics.

**Open questions for the owner**
1. Which ROS 2 distro is the primary target (Humble, Jazzy, Rolling)?
2. Where do your workspaces live (`~/ros2_ws`, `/opt/...`)? This defines the
   read-only bind policy.
3. Is running `setup.bash` acceptable at all under an explicit confirmation, or
   should 0.0.2 only support an environment file the user exports manually
   (a `.env`-style JSON)? The confirmation design assumes the former.

---

## 10. Working agreement for AI sessions

### 10.1. Session protocol

1. Read this file only. Do not re-read the master prompt unless the owner asks.
2. Ask for or confirm the target release and the backlog IDs.
3. Inspect only the files relevant to those IDs; use `rg` to find call sites.
4. State the plan, the security impact, and the verification approach.
5. Implement the smallest coherent change; do not refactor unrelated code.
6. Run tests, typecheck, build, and E2E where applicable.
7. Update this file (statuses, changelog) and any affected docs.
8. Report: changed files, evidence, unverified items, and next steps.

### 10.2. Definition of done

- [ ] Code compiles with `-Wall -Wextra -Wpedantic -Werror`.
- [ ] Tests pass (`./scripts/test.sh`), including new security-relevant tests.
- [ ] `npm run typecheck`, `npm run build`, `npm run check:contrast`, and
      `npm run check:i18n` pass for UI changes.
- [ ] E2E/screenshot evidence exists for UI changes (0.0.2 evidence:
      `docs/screenshots/0.0.2/`).
- [ ] `./scripts/build-deb.sh` succeeds and `verify-deb.sh` passes for
      release changes.
- [ ] No invariant in §2 is weakened.
- [ ] `PROJECT.md` status updated; changelog line added.
- [ ] Threat model updated if the change touches processes, paths, network,
      secrets, or new IPC.
- [ ] Startup/memory numbers re-measured if the change affects the UI bundle or
      the LSP path.

### 10.3. Security review triggers

A change needs an explicit security note in the PR/summary and tests when it:

- starts a new process or changes a spawn profile;
- accepts a new path from the UI;
- adds network access;
- reads or stores a secret;
- writes outside the workspace/cache/config directories;
- adds an IPC method or event;
- widens file access for a sandboxed process;
- executes project or system scripts.

### 10.4. What not to do

- Do not switch away from Monaco, WebKitGTK, C++ core, Zustand, or the current
  design language without an owner decision and an ADR.
- Do not introduce Electron, Qt widgets, QtWebEngine, Tailwind, or a UI kit.
- Do not add `system()`, `popen()`, or string-built shell commands.
- Do not run build/setup scripts automatically or silently retry them.
- Do not ship fake UI content for unimplemented features.
- Do not add telemetry, analytics, remote assets, or auto-update in 0.0.2.
- Do not push, rewrite history, or change git remotes.

### 10.5. Git conventions

- Branches: `feat/`, `fix/`, `refactor/`, `docs/`, `chore/`, `security/`.
- Commits: imperative, ≤ 72 characters, optional body explaining why.
- Never commit `build/`, `.sysroot/`, `dist/`, `node_modules/`, or signing keys.
- `PROJECT.md` is committed; it must contain no absolute local paths, no
  secrets, and no machine-specific identifiers.

### 10.6. How to update this file

- **Bugs:** append a new `BUG-xxx` block; never reuse an ID. Move `Status` to
  `FIXED (version)` and add a one-line resolution. Keep the reproduction.
- **Improvements:** append `IMP-xxx`; set `Status` and target release. When
  done, mark `DONE (version)` and add the verification evidence line.
- **Roadmap:** only the owner changes priorities and release scope. Agents may
  add technical notes and dependency links.
- **Release history:** add a row per release with date, theme, IDs delivered,
  and artifact name.
- **Metadata:** bump `File version`, update `Last updated` and `Applies to`.
- **New release:** create `prompts/masterprompt_<version>.md` from §7.6 + §9,
  keep this file as the live tracker, and add a release-history row when the
  release ships.

---

## 11. Release history and changelog

| Version | Date | Theme | Delivered | Artifact |
|---|---|---|---|---|
| 0.0.1 | 2026-11-13 | Phase 1 MVP | C++ editing, clangd intelligence, sandbox, `.deb`, tests, threat model | `anvil_0.0.1-1_amd64.deb` |
| 0.0.2 | in progress | Personalization + environment + agent | IMP-001, IMP-002, IMP-004 delivered; IMP-003, IMP-005 pending | `anvil_0.0.2-1_amd64.deb` (not built) |

### 0.0.1 — 2026-11-13

- Native C++20 core with a workspace jail, validated JSON-RPC, fs, watcher,
  settings, recent/workspace state, read-only Git branch.
- C++ WebKitGTK shell with a JS bridge, native dialogs, workspace allowlist,
  smoke/E2E/screenshot modes.
- React/Monaco UI: explorer, tabs, breadcrumbs, welcome, problems/output,
  palette, quick open, settings, security, about, status bar.
- clangd hover, completion, definition, references, signature help, formatting,
  diagnostics; bubblewrap + seccomp isolation; fail-closed release path.
- 51 tests; `.deb` with SBOM, threat model, AppStream, bwrap AppArmor profile,
  signed local APT repo.

### 0.0.2 — in progress (`devel/0.0.2`)

Delivered:

- **IMP-001 Russian UI localization:** typed `en`/`ru` dictionaries, live
  switching via `ui.language`, system-locale detection, pluralization,
  localized dates, translated ARIA labels, `check:i18n` gate.
- **IMP-002 Theme selection:** Anvil Dark, Anvil Light, Anvil High Contrast,
  and `auto`; theme-scoped CSS tokens; Monaco variants; pre-paint boot theme;
  WCAG AA contrast gate; light theme darkens/adjusts semantic text tokens for
  AA while keeping the brand accent hue.
- **IMP-004 Font selection:** UI/editor font family, UI size 11–18, editor
  size, line height, ligatures; fontconfig-backed `shell/listFonts`; validated
  CSS variables and Monaco updates; large-font layout fixes.
- Core settings schema and IPC validation extended; 55 tests pass.
- Verification screenshots in `docs/screenshots/0.0.2/`; visual checks were run
  under Xvfb with the real WebKitGTK shell.
- Not started: IMP-003 (Codewhale, needs ADR/security review) and IMP-005
  (ROS 2 environment, needs owner answers on distro/workspace policy).

---

## 12. Reference documents

| Document | Use |
|---|---|
| `docs/ARCHITECTURE.md` | Detailed component map, IPC, data flows |
| `docs/security/threat-model.md` | Threat model, mitigations, residual risks |
| `docs/BUILD.md` | Full build, sysroot fallback, E2E, packaging |
| `docs/benchmarks.md` | Measured startup/memory/size and methodology |
| `docs/RELEASE-CHECKLIST.md` | Per-release checklist status for 0.0.1 |
| `docs/adr/0001-native-cpp-shell.md` | Why the shell is C++ WebKitGTK, not Tauri |
| `docs/releases/v0.0.1.md` | Release notes for 0.0.1 (also used on GitHub) |
| `prompts/masterprompt_0.0.1.md` | Original product specification (historical) |

If a reference document and this file disagree about **current status, scope, or
priorities**, this file wins. If they disagree about **security rules**, the
threat model and §2 win; report the conflict instead of guessing.
