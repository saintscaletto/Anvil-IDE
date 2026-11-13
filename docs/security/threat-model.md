# Anvil IDE — threat model (v0.0.1)

Status: implemented reference for the 0.0.1 experimental release.
Last reviewed: 2026-11-13.

## 1. Scope

This document covers the Anvil desktop application as shipped in the
`anvil` Debian package:

- the native C++ shell (`anvil`) embedding WebKitGTK;
- the C++ core (`anvil-core`) that owns filesystem, workspace, LSP, and
  security policy;
- the React/Monaco UI loaded from local, packaged assets;
- the external `clangd` process managed by the core;
- the Debian package, desktop metadata, and local APT repository tooling.

Out of scope for 0.0.1: build execution, integrated terminal, debug adapters,
Git write operations, plugins, AI providers, and auto-update. Those features
are not present, so their threats are recorded as future work rather than
claimed as mitigated.

## 2. Assets

The highest-value assets are:

1. The user's source code and uncommitted changes.
2. The user's SSH/GPG keys, Git credentials, cloud tokens, and browser/session
   data reachable from the same account.
3. The integrity of the machine (process execution, filesystem writes, network).
4. The integrity of the Anvil binary and its update channel.
5. The confidentiality of any future secret-bearing feature (AI keys, Git
   credentials). Anvil 0.0.1 stores no secrets.

## 3. Trust boundaries

```
[ untrusted project files ] --reads--> [ C++ core ] --sandbox--> [ clangd ]
                                             ^
                                             | validated JSON-RPC over stdio
                                             |
[ local UI assets ] <--WebKitGTK--> [ C++ shell ] --spawn--> [ core ]
        ^
        | CSP, no network, no remote content
[ user input / XSS surface ]
```

- **Project files are untrusted.** `CMakeLists.txt`, `.clangd`,
  `compile_commands.json`, source files, symlinks, and file names can all be
  hostile. Anvil never executes project build scripts in 0.0.1.
- **The UI is local but not implicitly trusted.** It is packaged with the app
  and loaded over `file://`, but an XSS or a compromised WebKit renderer could
  call the bridge. Every core method validates its input again.
- **The shell is a thin transport.** It does not perform filesystem operations
  on behalf of the UI; it forwards JSON-RPC and implements only native dialogs,
  window title, close, and core restart.
- **clangd is an external process.** Even a genuine clangd is treated as a
  potentially exploitable parser; it is started in a bubblewrap sandbox with
  seccomp and a read-only workspace.
- **The Debian installation is trusted** only insofar as `dpkg`/APT signature
  verification is trusted. The app has no self-updater.

## 4. Threats and mitigations

### 4.1 Filesystem and path traversal

| Threat | Mitigation | Where |
|---|---|---|
| `../../etc/passwd` in IPC | Relative paths only; no `..`, `.`, empty components, absolute paths, backslashes, or control characters | `core/src/ipc/validate.cpp` |
| Symlink inside the workspace pointing outside | Canonicalise, then component-wise containment check; `resolve_nofollow` for rename/delete/stat so the link is handled without following it | `core/src/workspace/workspace.cpp` |
| Renaming/deleting a symlink deletes its target | `resolve_nofollow` resolves the parent, not the leaf | `workspace.cpp`, `server.cpp` |
| Writing outside the project | `resolve_for_create` canonicalises the parent and checks containment | `workspace.cpp` |
| Cross-file race during save | Atomic temp file + `fsync` + `rename`; optimistic mtime check | `core/src/util/fs_util.cpp` |
| Stale writer overwrites newer edits | `expectedMtime` conflict; the UI must explicitly reload or force overwrite | `fs_util.cpp`, `server.cpp`, `stores/editor.ts` |
| Huge or binary file freezes the editor | 8 MiB read cap, 16 MiB write cap, UTF-8 validation, NUL rejection | `validate.cpp`, `fs_util.cpp` |
| Too many directory entries | 2000-entry listing cap with a `truncated` flag | `server.cpp`, `fs_util.cpp` |
| JSON nesting stack exhaustion | 64-level depth scan before parsing; 32 MiB line cap | `core/src/ipc/jsonrpc.cpp` |

The workspace root itself cannot be `/`. A user can still choose a broad
directory such as `$HOME`; that is an explicit user decision and remains the
trust boundary.

### 4.2 Process execution and shell injection

- All processes are started with `fork`/`execve` and an argv array. There is no
  `system()`, `popen()`, or `sh -c` in the codebase.
- `PATH` resolution is done in the parent with an absolute-path requirement by
  the sandbox builder.
- Language-server paths must resolve to `/usr`, `/bin`, `/sbin`, `/lib`,
  `/lib64`, or `/opt`. A user-writable clangd is rejected in release builds.
- `PR_SET_NO_NEW_PRIVS` is set for every direct child.
- In release builds, an unsandboxed LSP fallback is compiled out. The
  development fallback requires a build with `ANVIL_DEV_MODE=ON`, the
  `ANVIL_DEV_UNSAFE_LSP=1` environment variable, and the
  `security.allowUnsandboxedLsp` setting. Its use is recorded in the security
  log.

### 4.3 Language-server isolation

The production LSP sandbox is built in `core/src/security/sandbox.cpp`:

- bubblewrap with a separate PID, IPC, UTS, and (where the kernel allows)
  network namespace;
- read-only `/usr`, `/lib`, `/lib64`, `/bin`, `/sbin`, `/etc/ld.so.cache`,
  `/etc/alternatives`, `/etc/passwd`, `/etc/group`, and `/etc/nsswitch.conf`;
- read-only workspace; the only writable bind is a private cache directory
  under `~/.cache/anvil/lsp/...` with mode 0700 and a pre-created `HOME`;
- `/proc`, `/dev`, and a private `/tmp`;
- `--die-with-parent`, `--new-session`, `--cap-drop ALL`, `--clearenv`, and a
  minimal explicit environment;
- a raw cBPF seccomp filter passed as `--seccomp 3`:
  - denies `ptrace`, process-vm reads/writes, mount/namespace operations,
    module loading, keyring operations, BPF, `perf_event_open`, `userfaultfd`,
    and io_uring. `execve` is deliberately allowed: bubblewrap installs the
    seccomp filter immediately before its own `execvp()` of the payload, so an
    exec-deny filter would prevent clangd from starting. The payload runs with
    `no_new_privs`, a read-only workspace, a private cache, and no network;
    exec cannot reach outside that namespace or gain privileges.
  - denies non-AF_UNIX sockets, `connect`, `bind`, `listen`, `accept*`, `sendto`,
    and `recvfrom`; network namespaces add a second layer where available.

`.clangd` project configuration is disabled by default (`--enable-config=false`)
because it is untrusted project input. `compile_commands.json` is treated as
data, but its flags still influence clangd. A malicious flag such as
`-fplugin=` can load code into clangd; the sandbox contains that code (no
network, read-only workspace), but it can read project files, crash
clangd, and consume the private cache. This is a residual risk, not a VM
boundary.

Ubuntu 24.04 restricts unprivileged user namespaces through AppArmor. The
package installs `/etc/apparmor.d/anvil-bwrap`, an unconfined profile whose only
purpose is to grant the `userns` permission to `/usr/bin/bwrap`; bubblewrap
still performs the namespace and seccomp isolation. Without this profile,
bwrap fails with `setting up uid map: Permission denied`. The profile is a
conffile, is loaded by `postinst` when `apparmor_parser` is present, and is
removed on purge. On systems that block user namespaces entirely (including
some containers), release builds fail closed and language-server startup is
refused. The UI shows the reason and the security status. There is no silent
unsandboxed fallback.

### 4.4 IPC validation and UI compromise

- The shell accepts messages only from the local page's `window.webkit` handler.
  Navigation is restricted to the exact packaged `index.html`; new windows,
  downloads, permissions, and remote subresources are denied.
- The page is loaded from a packaged `file://` asset directory with a strict CSP:
  `default-src 'none'`, scripts/styles from self, `connect-src 'none'`,
  `frame-src 'none'`, `object-src 'none'`, `base-uri 'none'`, and
  `form-action 'none'`. No `unsafe-eval` is required.
- WebKit settings disable WebGL, plugins, media, WebAudio, page cache, HTML5
  local storage, offline cache, and developer extras in release builds.
- Every UI request carries `apiVersion: 1`; the core rejects missing or unknown
  versions.
- Every core method has a typed validator. Unknown fields, wrong types, missing
  required fields, oversized values, invalid UTF-8, and out-of-range numbers are
  rejected before reaching services.
- The shell's local methods (`shell/openFolder`, `shell/setTitle`, etc.) do not
  touch the filesystem beyond native GTK dialogs and the window title.
- Workspace selection is mediated by the shell. Only the command-line
  workspace, paths returned by native GTK dialogs, and paths in the user's
  `recent.json` are allowed to become a workspace root. An arbitrary
  `workspace/open` from the UI is rejected with `PathOutsideWorkspace`, so an
  XSS cannot silently widen the jail to `$HOME` or `/etc`. A shell-mediated
  test verifies that `/etc` is rejected and the CLI workspace is accepted.
- If an XSS succeeds, it can call only the exposed core methods. It cannot open
  a network connection, load remote content, spawn a shell, or escape the
  workspace path jail. It could reopen a previously approved recent workspace,
  read/modify files inside that workspace, and corrupt the editor UI; this is a
  serious but bounded impact.

### 4.5 Secrets and logs

- Anvil 0.0.1 has no Git credentials, AI keys, or account tokens. No secret
  store is exposed, and no secrets are written to `settings.json`.
- Settings and recent/workspace state are mode 0600 under a 0700 config
  directory.
- Security audit events are JSON Lines in
  `~/.config/anvil/logs/security.log`, mode 0600, rotated at 10 MiB × 5.
- Log messages and language-server log/stderr lines pass through redaction:
  Authorization headers, bearer tokens, `token=`, `password=`, `api_key=`, and
  similar key/value pairs are masked; sensitive JSON keys are replaced; file
  contents are never logged.
- Arguments are never shell-interpolated, so log lines cannot become commands.
- Future secret-bearing features must use libsecret through D-Bus; the UI must
  never receive a secret.

### 4.6 Update channel and package integrity

- The package is a normal Debian package; installation and upgrades use APT.
- The local APT repository metadata is signed with a demonstration Ed25519 key
  in this source tree. Production releases must use an offline release key and
  publish the public key through an HTTPS channel.
- There is no automatic updater in 0.0.1 and no network access from the app.
- The package contains an SBOM and the Apache-2.0 license text.
- The `postinst`/`postrm` scripts do not execute project content and do not
  delete user data under `~/.config/anvil`.

### 4.7 Resource exhaustion

- Bounded IPC lines, JSON depth, file sizes, directory listings, watcher event
  batches, diagnostic counts, search results, and thread-pool queues.
- File watching is recursive but capped (4096 watches, depth 32) and ignores
  `.git`, `node_modules`, `build`, `target`, `.cache`, `dist`, and `.anvil`.
  `IN_Q_OVERFLOW` results in a rescan event, not a crash.
- The LSP reader enforces header/body limits and cancels timed-out requests.
- The editor's model count is bounded by open tabs; models are disposed on
  close.
- The core stops idle clangd after five minutes without open documents.

## 5. Residual risks and accepted limitations

1. **WebKitGTK zero-day or renderer escape.** A compromised engine can defeat
   the app-level boundary. Mitigations are system updates, no remote content,
   no network, CSP, and the fact that the web process is itself sandboxed by
   WebKitGTK.
2. **clangd plugin flags.** A hostile `compile_commands.json` can load code
   into clangd. It remains inside bubblewrap + seccomp with a read-only
   workspace, but can read project files and cause denial of service.
3. **bubblewrap availability.** Ubuntu 24.04 desktops support user namespaces;
   restricted containers may not. In those environments the release app fails
   closed for LSP rather than silently dropping isolation.
4. **No independent verification of clangd.** Anvil trusts the system package
   manager for the clangd binary. Future releases should verify a pinned
   package version or signature.
5. **No secret storage yet.** Future Git/AI features must add libsecret and
   D-Bus keyring integration; plaintext JSON must never be used.
6. **No build/debug/terminal sandbox yet.** Those features are not implemented;
   when they are, they need separate capability profiles. Running arbitrary
   project build scripts would be a much larger trust decision and is explicitly
   deferred.
7. **Test-key signing.** The repository signing key generated by the local
   packaging script is a demonstration key. It must not be used for public
   distribution.

## 6. Security test coverage

The test suite (`core/tests`, 51 cases) covers:

- every IPC validator: malformed types, unknown fields, missing fields,
  oversized payloads, traversal, absolute paths, NUL/control characters,
  invalid UTF-8, range ordering, and setting bounds;
- JSON-RPC framing, depth limits, and API version rejection;
- path jail and symlink escape;
- atomic writes and mtime conflicts;
- seccomp filter size, exec policy, and AF_INET socket denial;
- sandbox argv construction, fail-closed behavior, writable-path containment,
  trusted program prefixes, and no `sh -c`;
- security log redaction;
- LSP transport framing and a fake-server handshake/diagnostic/hover test;
- a full core JSON-RPC lifecycle over stdio, including a malicious path and a
  missing API version;
- a shell-level workspace allowlist test: `/etc` is rejected and the CLI
  workspace is accepted.

Manual and E2E evidence for the UI (screenshots, hover, diagnostics) is recorded
in `docs/benchmarks.md` and `docs/screenshots/`.
