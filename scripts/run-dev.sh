#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [ ! -x "$ROOT/build/dev/shell/anvil" ]; then
    "$ROOT/scripts/build.sh" dev
fi
export ANVIL_UI_DIR="$ROOT/apps/desktop/dist"
export ANVIL_CORE_PATH="$ROOT/build/dev/core/anvil-core"
exec "$ROOT/build/dev/shell/anvil" --dev "$@"
