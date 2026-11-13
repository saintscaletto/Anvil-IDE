# Anvil 0.0.1 — measured results

Reference environment:

- Ubuntu 24.04.5 LTS, kernel 7.0.0, x86_64
- 16 vCPU, 31 GiB RAM, SSD
- X11 display (`:1`), WebKitGTK 2.52.6
- clangd 18.1.3 (extracted local package for the container E2E run)
- Release shell/core build, production UI bundle

The container used for this build cannot create user namespaces. bubblewrap
therefore could not be exercised here; the release sandbox probe fails closed.
The LSP E2E run used the explicit development override in a dev build. On a
normal Ubuntu desktop, the production path uses bubblewrap + seccomp.

## Startup

`anvil --smoke-test` (WebKit window, React UI, core handshake, no workspace):

| Build | Runs | Median |
|---|---|---|
| Release | 800, 908, 936 ms | ~0.87 s |
| Debug | 835, 839, 878 ms | ~0.85 s |

The master-prompt target is <500 ms. **It is not met in this environment.**
The floor is dominated by WebKitGTK process startup and parsing the ~3.6 MB
Monaco/React bundle, not by the C++ core. The next iteration should split
Monaco into a lazily loaded script served from a custom URI scheme, which
removes the editor parse from the welcome path.

## Memory (PSS)

Idle welcome screen, no workspace and no language server:

| Process | PSS |
|---|---|
| `anvil` shell | ~65 MiB |
| WebKit web process | ~164 MiB |
| WebKit network process | ~22 MiB |
| `anvil-core` | ~3 MiB |
| **Total PSS** | **~255 MiB** |

RSS sums are larger because shared WebKit/GTK libraries are counted in each
process; PSS is the fairer total. The master-prompt <300 MiB target is met for
the idle welcome screen by PSS, but not by a naive RSS sum.

With the sample C++ project open and clangd running, RSS measured around
810 MiB across shell, WebKit processes, core, and clangd (clangd ~339 MiB,
WebKit web process ~286 MiB, shell ~179 MiB). PSS is lower but the full stack
with clangd is above the 300 MiB goal. clangd is the largest contributor;
`--background-index` and system-header indexing drive it. The core stops idle
clangd after five minutes without open documents.

## LSP responsiveness

Observed on the sample project through the real core and clangd:

- hover result: well under 200 ms after warm-up;
- initial diagnostics after `didOpen`: about 1–2 s in this container;
- rename/format/reference requests are implemented but not part of the 0.0.1
  UI beyond references.

The integration test with a fake LSP server completes handshake, didOpen,
diagnostics, hover, and shutdown in ~0.08 s, proving the transport path.

## Artifacts (final build)

- Release `anvil-core`, stripped in package: 1,006,952 bytes (~983 KiB)
- Release `anvil` shell, stripped in package: 227,640 bytes (~222 KiB)
- UI JavaScript bundle: 3,621,543 bytes (~3.45 MiB) minified
- UI CSS: 175,575 bytes
- `.deb`: approximately 1.35 MiB, Installed-Size ~5.2 MiB (exact size/hash in
  `dist/SHA256SUMS`)
- SHA-256: see `dist/SHA256SUMS` (the hash covers the package that contains
  this document; publishing it inside the package would be circular).
- Package layout verified by `scripts/verify-deb.sh`; the signed local APT
  repository hashes and signatures were checked with `gpgv`.

The extracted release package was also launched with `--e2e` against the sample
workspace. It opened the project and `src/main.cpp`; clangd was not on PATH in
that environment, so the UI correctly reported a clangd error while the editor
remained fully usable.

## Tests

- `ctest`: 51/51 pass (core unit, security, sandbox builder, transport, fake
  LSP, full server lifecycle).
- UI TypeScript: `tsc --noEmit` passes.
- UI production build: passes.
- E2E screenshots: welcome, editor, hover, and diagnostics are checked in
  under `docs/screenshots/`.

## Known gaps

- Startup target not met (see above).
- Full-stack memory with clangd exceeds the 300 MiB goal.
- Release LSP cannot run in this container because bubblewrap user namespaces
  are unavailable; the dev fallback was used only for verification.
- Build/run, DAP, PTY terminal, Git writes, AI, and plugins are intentionally
  absent from 0.0.1.
