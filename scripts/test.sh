#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build/tests"
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DANVIL_BUILD_SHELL=OFF \
    -DANVIL_BUILD_TESTS=ON \
    -DANVIL_DEV_MODE=ON \
    -DANVIL_LTO=OFF
cmake --build "$BUILD_DIR" -j "$(nproc)"
(cd "$BUILD_DIR" && ctest --output-on-failure -j "$(nproc)")
