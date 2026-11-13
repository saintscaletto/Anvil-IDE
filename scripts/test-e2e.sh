#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHELL_BIN="$ROOT/build/dev/shell/anvil"
CORE_BIN="$ROOT/build/dev/core/anvil-core"
if [ ! -x "$SHELL_BIN" ] || [ ! -x "$CORE_BIN" ]; then
    "$ROOT/scripts/build.sh" dev
fi
if [ -z "${DISPLAY:-}" ]; then
    echo "skip: no DISPLAY for the WebKit smoke test" >&2
    exit 0
fi
TMP="$(mktemp -d /tmp/anvil-e2e-XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
OUTPUT="$(ANVIL_CONFIG_DIR="$TMP/config" ANVIL_CACHE_DIR="$TMP/cache" \
    ANVIL_UI_DIR="$ROOT/tests/e2e/workspace-allowlist" \
    ANVIL_CORE_PATH="$CORE_BIN" \
    timeout 45 "$SHELL_BIN" --smoke-test --dev \
        --ui-dir "$ROOT/tests/e2e/workspace-allowlist" \
        --core "$CORE_BIN" \
        --workspace "$ROOT/tests/fixtures/sample_cpp" 2>/dev/null || true)"
echo "$OUTPUT"
echo "$OUTPUT" | grep -q 'SMOKE_OK' || {
    echo "error: shell workspace allowlist or bridge smoke test failed" >&2
    exit 1
}
echo "shell allowlist smoke: PASS"
