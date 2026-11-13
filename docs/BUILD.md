# Building Anvil 0.0.1

Target: Ubuntu 24.04 LTS (noble), x86_64.

## 1. Recommended: install the build dependencies

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  nlohmann-json3-dev libspdlog-dev libgtest-dev \
  libwebkit2gtk-4.1-dev libgtk-3-dev \
  nodejs npm \
  clangd-18 clang-18
```

`clangd-18` is only required to exercise C/C++ intelligence; the application
still builds and opens files without it.

## 2. Unprivileged container fallback

If you cannot use `sudo`, the repository can extract the required development
packages into a local sysroot:

```bash
./scripts/setup-sysroot.sh
source .sysroot/env.sh
```

The script uses `apt-get download` and `dpkg-deb -x`; it does not install
anything system-wide. It is a development convenience, not a replacement for
the Debian Build-Depends.

## 3. Build

```bash
./scripts/build.sh release    # release binaries + production UI
./scripts/build.sh dev        # debug binaries, tests enabled
```

Outputs:

- `build/release/core/anvil-core`
- `build/release/shell/anvil`
- `apps/desktop/dist/` (UI)

The shell needs the WebKitGTK development package only at build time. At
runtime it uses the system `libwebkit2gtk-4.1-0` and GTK 3 libraries.

## 4. Run from the build tree

```bash
./scripts/run-dev.sh /path/to/project
```

For the E2E build with a locally extracted clangd, set a development config
(examples in `docs/benchmarks.md`):

```bash
export ANVIL_CONFIG_DIR=/tmp/anvil-dev-config
export ANVIL_CACHE_DIR=/tmp/anvil-dev-cache
export ANVIL_DEV_UNSAFE_LSP=1
export ANVIL_DEV_LD_LIBRARY_PATH=$PWD/.sysroot/root/usr/lib/x86_64-linux-gnu
./scripts/run-dev.sh /path/to/project
```

The unsafe override is compiled only when `ANVIL_DEV_MODE=ON`. It is never
available in the release `.deb`.

## 5. Tests

```bash
./scripts/test.sh
```

This configures `build/tests` with GoogleTest and runs 51 tests, including:

- every IPC validator and JSON-RPC framing;
- path traversal and symlink escapes;
- atomic writes and optimistic concurrency;
- seccomp deny rules and sandbox argv construction;
- LSP transport and a fake-server client handshake/hover/diagnostics test;
- a full `anvil-core` stdio lifecycle.

## 6. Prepare the C++ fixture and run the shell E2E test

The fixture's `compile_commands.json` is generated, not committed. On a fresh
clone, configure it once so clangd can resolve project headers:

```bash
./scripts/prepare-fixtures.sh
```

This runs CMake configuration only; it does not compile and does not execute
any project script. Then run the shell E2E/allowlist test:

```bash
./scripts/test-e2e.sh
```

This loads a minimal test page in the real WebKit shell. The page tries
`workspace/open` for `/etc` (the shell must reject it as outside the allowed
workspace set) and then for the CLI workspace (the shell and core must accept
it). It is also a regression test for the native bridge and the workspace
security gate.

## 7. UI checks

```bash
cd apps/desktop
npm ci
npm run typecheck
npm run build
```

The build uses Vite IIFE output plus a deferred classic script tag so that the
bundle works from `file://` inside WebKitGTK. Monaco is loaded as a single
bundle but the editor instance is created only when the first file opens.

## 8. E2E and screenshots

The shell provides test hooks:

```bash
ANVIL_UI_DIR=$PWD/apps/desktop/dist \
ANVIL_CORE_PATH=$PWD/build/dev/core/anvil-core \
build/dev/shell/anvil --smoke-test --dev

ANVIL_UI_DIR=$PWD/apps/desktop/dist \
ANVIL_CORE_PATH=$PWD/build/dev/core/anvil-core \
build/dev/shell/anvil --e2e \
  --workspace $PWD/tests/fixtures/sample_cpp \
  --e2e-file src/main.cpp \
  --e2e-hover 7:8 \
  --screenshot /tmp/anvil-hover.png --screenshot-delay 8 --dev
```

`--smoke-test` verifies that WebKit, React, Monaco (on demand), the shell
bridge, and `anvil-core` all start. `--screenshot` captures the web view
through `WebKitWebView.get_snapshot()`.

## 9. Build the `.deb`

```bash
./scripts/build-deb.sh
```

Artifacts are written to `dist/`:

- `anvil_0.0.1-1_amd64.deb`
- `sbom.json`
- `apt/` — a signed local Apt repository layout
- `anvil-test-key.asc` — demonstration signing key (not for production)

Install locally with:

```bash
sudo apt install ./dist/anvil_0.0.1-1_amd64.deb
anvil ~/src/my-project
```

The package Recommends `bubblewrap`, `clangd-18`, `gdb | lldb`, `git`,
`cmake`, and `ninja-build`. Language intelligence is disabled gracefully if
clangd or bubblewrap is not installed.

On Ubuntu 24.04 the package installs a minimal AppArmor profile for
`/usr/bin/bwrap` that grants `userns`. Ubuntu's default user-namespace
restriction otherwise prevents bubblewrap from starting. The profile is a
conffile and is removed on purge. Systems that block user namespaces entirely
(including many containers) will see the language server fail closed; the
Security Status dialog shows the exact reason.

## 10. Verification

```bash
./scripts/verify-deb.sh dist/anvil_0.0.1-1_amd64.deb
```

The verifier checks the package metadata, extracts it into a temporary root,
runs `anvil --version`, `anvil-core --version`, `anvil-core --self-test`, a GUI
smoke test when a display is available, validates the desktop/AppStream files,
checks for missing shared libraries, and checks RPATH and size limits.

`lintian` is not available in the reference environment; the script prints a
note instead of pretending it ran.
