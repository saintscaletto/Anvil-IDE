#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-release}"
BUILD_DIR="$ROOT/build/$MODE"

if [ -f "$ROOT/.sysroot/env.sh" ]; then
    # shellcheck disable=SC1091
    source "$ROOT/.sysroot/env.sh"
elif [ -d "$ROOT/.sysroot/root/usr/lib/x86_64-linux-gnu/pkgconfig" ]; then
    export PKG_CONFIG_PATH="$ROOT/.sysroot/root/usr/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
    export LD_LIBRARY_PATH="$ROOT/.sysroot/root/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
fi

if ! pkg-config --exists webkit2gtk-4.1; then
    echo "error: webkit2gtk-4.1 development files are missing." >&2
    echo "Install packaging/debian/control Build-Depends, or run scripts/setup-sysroot.sh." >&2
    exit 1
fi

if [ "$MODE" = "dev" ]; then
    CMAKE_BUILD_TYPE=Debug
    DEV_MODE=ON
    TESTS=ON
    LTO=OFF
else
    CMAKE_BUILD_TYPE=Release
    DEV_MODE=OFF
    TESTS=OFF
    LTO=ON
fi

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DANVIL_BUILD_SHELL=ON \
    -DANVIL_BUILD_TESTS="$TESTS" \
    -DANVIL_DEV_MODE="$DEV_MODE" \
    -DANVIL_LTO="$LTO"
cmake --build "$BUILD_DIR" -j "$(nproc)"

if [ ! -d "$ROOT/apps/desktop/node_modules" ]; then
    (cd "$ROOT/apps/desktop" && npm ci --no-audit --no-fund)
fi
(cd "$ROOT/apps/desktop" && npm run build)

echo "core:  $BUILD_DIR/core/anvil-core"
echo "shell: $BUILD_DIR/shell/anvil"
echo "ui:    $ROOT/apps/desktop/dist"
