# Anvil 0.0.1 — master-prompt checklist status

> Current roadmap, release scope, and the Bugs/Improvements backlog live in
> `PROJECT.md` at the repository root. This file is the historical 0.0.1
> verification record.

This is the honest status of the Phase 1 MVP and the release checklist for
this iteration. Checked means implemented and verified by tests or E2E
evidence; partial items state exactly what is missing.

## §9 Phase 1 — MVP

- [x] One-instance HTML prototype was the design reference; the React/TS/
  Zustand/Monaco port is in `apps/desktop`.
- [x] C++ sidecar with JSON-RPC over stdio and `fs/*`, workspace, settings,
  recent, state, watcher, LSP, security, and read-only Git branch methods.
- [x] clangd startup through bubblewrap + seccomp with a read-only workspace,
  private cache, no network, and a fail-closed release path. The production
  sandbox could not execute in the build container because user namespaces are
  blocked; the sandbox argv/deny rules and the seccomp filter are unit-tested,
  and the real clangd path was verified through the documented development
  override.
- [x] Monaco ↔ clangd: hover, completion, definition, signature help,
  diagnostics, formatting request, and a references command/modal. Hover and
  diagnostics were verified end-to-end with real clangd (screenshots).
- [x] File tree on the real filesystem with inotify refresh.
- [x] Tabs on real files, dirty state, view-state restore, close confirmation.
- [x] Problems panel on real clangd diagnostics with click-to-navigate.
- [x] Status bar: line/column, language, encoding, EOL, clangd status, sandbox
  status, branch, problem counts.
- [x] Command palette with 30+ real commands and keyboard shortcuts.
- [x] IPC schema validation for every method, with tests for traversal, shell
  injection-shaped input, oversized payloads, malformed JSON, and invalid
  UTF-8.
- [x] `docs/security/threat-model.md`.
- [x] First `.deb` built, signed-repo metadata, SBOM, GUI smoke test.
- [x] Reproducible shell E2E: `/etc` is rejected as a workspace and the CLI
  workspace is accepted (`scripts/test-e2e.sh`).

## §12.10 release checklist

- [~] `dpkg-buildpackage`/lintian: the package is assembled with `dpkg-deb`
  from `scripts/build-deb.sh` because the repository is not a classic Debian
  source tree. `lintian` is not installed in the reference environment; the
  verifier performs structural, metadata, RPATH, dependency, and smoke checks.
- [~] `apt install ./anvil_*.deb` on a clean Ubuntu 24.04: not possible in the
  unprivileged build container. The package was extracted, dependency-checked,
  version-checked, self-tested, and GUI-smoke-tested. The final install command
  is documented.
- [x] `anvil --version` and `anvil-core --version`.
- [~] `apt remove`/`apt purge`: maintainer scripts are non-destructive and do
  not touch `~/.config/anvil`; an actual root install/remove cycle was not
  available in this environment.
- [x] Repository GPG signature and APT Packages hash verification. Individual
  `.deb` files are not signed separately; Debian verifies them through signed
  repository metadata, which is the correct model.
- [x] APT repository layout and signed Release/InRelease.
- [x] SBOM at `/usr/share/doc/anvil/sbom.json`.
- [x] Threat model included in the package and source tree.
- [~] Benchmarks: package size and binary size are in range; startup and
  full-stack memory with clangd do not meet the aspirational targets and are
  reported in `docs/benchmarks.md`.

## §8.2 efficiency checklist

- [x] Core spawn is parallel with UI startup.
- [x] clangd starts only after the first C++ document.
- [x] Monaco editor instance is created only when a file opens; the welcome
  screen does not instantiate it.
- [x] Models are disposed on tab close; one editor is reused.
- [x] `didChange` coalescing at 55 ms with a minimal prefix/suffix range.
- [x] LSP timeouts and cancellation.
- [x] clangd idle shutdown after five minutes without open documents.
- [x] Release hardening: LTO, function/data sections, gc-sections, PIE, RELRO,
  stack protector, stripped binaries.
- [~] Lazy React chunks / lazy Monaco script: not achieved; the single classic
  bundle is a deliberate `file://` compatibility trade-off. This is the main
  startup optimization for 0.0.2.
- [~] LSP LRU cache: clangd owns its project cache; Anvil bounds open
  documents and stops idle servers, but does not implement a separate LRU
  layer.
- [x] The reference environment startup and memory numbers are documented,
  including the targets that were missed.

## Deliberately deferred (Phase 3/4)

- Build/run integration, CMake configuration, build-output diagnostics.
- DAP, breakpoints, call stack, variables, watch.
- Integrated PTY terminal.
- Git staging, commits, diffs; only the current branch is read.
- Tree-sitter indexing.
- AI provider integration (opt-in, redaction, local models).
- WASM plugin sandbox.
- Auto-update and a production signing key.
- Rust/Python language servers.
