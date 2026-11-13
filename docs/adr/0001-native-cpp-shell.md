# ADR 0001 — A native C++ shell instead of Tauri for 0.0.1

- Status: accepted for 0.0.1
- Date: 2026-11-13
- Decision owner: project author

## Context

The master prompt fixes the architecture as Monaco + Tauri 2.x + Rust host +
C++ core. The author's explicit instruction for this iteration is: **write the
application in C++**. This workspace also does not have a Rust toolchain, and
installing a full Tauri/webkit development chain would be a larger and less
auditable dependency surface than the C++ path already available.

The fixed constraints that remain meaningful are:

- C++ is the first-class language for the implementation.
- Monaco must remain the editor; the UI is web technology.
- No Electron and no bundled Chromium.
- The system WebView is the reason startup and memory can stay small.
- All business logic belongs in the native core, not in JavaScript.
- Runtime dependencies must be available in Ubuntu 24.04 and packageable as a
  `.deb`.

## Decision

Use a native **C++20 shell** built with GTK3 + WebKitGTK 4.1. The shell embeds
the same React + TypeScript + Monaco UI that the Tauri plan called for. It
starts the C++ core as a child process and bridges the UI and core with a
small, validated JSON message protocol. The core speaks JSON-RPC 2.0 over
stdio.

```
React + Monaco (WebKitGTK web process)
        │  window.webkit.messageHandlers.anvil
        ▼
C++ shell (GTK window, native dialogs, core supervisor)
        │  newline-delimited JSON-RPC over pipes
        ▼
C++ core (workspace jail, fs, watcher, LSP manager, security policy)
        │  sandboxed process
        ▼
clangd (bubblewrap + seccomp)
```

## Consequences

Positive:

- The application is C++ end to end on the native side.
- No Rust/Tauri dependency; fewer moving parts in the build.
- WebKitGTK is already the runtime Tauri would use on Linux, so the footprint
  and system-WebView benefits are preserved.
- The core is independently testable over stdio and has no GTK dependency.
- The shell is tiny (~300 KiB) and can be replaced later.
- Security boundaries are explicit and easy to audit: UI → shell → core →
  sandboxed server.

Negative / risks:

- This is a documented deviation from master-prompt §3.1/§3.2, which names
  Tauri. It must be reflected in the next master prompt.
- WebKitGTK API compatibility is now our responsibility; Tauri's webview
  abstraction is gone.
- There is no Tauri updater or single-instance plugin. The app uses APT for
  updates and opens one window per process.
- The packaged `.deb` depends on `libwebkit2gtk-4.1-0` and `libgtk-3-0t64`.
- WebKitGTK's web process is itself sandboxed on Ubuntu, but its security
  posture is not identical to Chromium's; the app compensates with a strict CSP,
  no remote content, no network, and core-side validation.

## Alternatives considered

- **Tauri + Rust host** — closest to the master prompt, but conflicts with the
  explicit C++ instruction and adds a toolchain that is not present.
- **Qt/QtWebEngine** — rejected: Qt UI is forbidden by the design principles
  and QtWebEngine bundles Chromium.
- **GTK-native widgets** — rejected: the design is web-based and Monaco is
  required; rebuilding the UI in GTK would be a redesign.
- **Electron** — rejected categorically.

## Notes for the next master prompt

- Replace the Rust/Tauri layer with the C++ WebKitGTK shell, or explicitly
  commission a Tauri host if that becomes a requirement.
- Keep the IPC contract and core unchanged; the shell is intentionally thin.
- The WebKitGTK shell can be extended with a custom URI scheme if lazy-loaded
  ES modules become necessary.
