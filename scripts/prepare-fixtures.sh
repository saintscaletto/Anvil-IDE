#!/usr/bin/env bash
# Configure the C++ test fixture so clangd has compile_commands.json.
# This only runs CMake configuration; it does not compile the project and it
# does not execute any project-provided script.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURE="$ROOT/tests/fixtures/sample_cpp"
BUILD="${1:-$FIXTURE/build}"

if ! command -v cmake >/dev/null 2>&1; then
    echo "error: cmake is required to prepare the fixture" >&2
    exit 1
fi

cmake -S "$FIXTURE" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if [ ! -f "$BUILD/compile_commands.json" ]; then
    echo "error: compile_commands.json was not generated in $BUILD" >&2
    exit 1
fi

echo "fixture ready: $BUILD/compile_commands.json"
echo "clangd will resolve project headers when this workspace is opened."
