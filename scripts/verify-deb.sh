#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEB="${1:?usage: verify-deb.sh <package.deb>}"
EXTRACT="$(mktemp -d /tmp/anvil-deb-XXXXXX)"
trap 'rm -rf "$EXTRACT"' EXIT

pass() { printf '  [ok] %s\n' "$1"; }
warn() { printf '  [warn] %s\n' "$1"; }
fail() { printf '  [FAIL] %s\n' "$1" >&2; exit 1; }

echo "Verifying $DEB"
[ -f "$DEB" ] || fail "package missing"

SIZE="$(stat -c%s "$DEB")"
[ "$SIZE" -lt $((15 * 1024 * 1024)) ] || fail "package is ${SIZE} bytes (limit 15 MiB)"
pass "size $((SIZE / 1024)) KiB (< 15 MiB)"

dpkg-deb --info "$DEB" >/dev/null
dpkg-deb -f "$DEB" Package Version Architecture Depends Installed-Size | sed 's/^/  /'
PACKAGE="$(dpkg-deb -f "$DEB" Package)"
VERSION="$(dpkg-deb -f "$DEB" Version)"
RECOMMENDS="$(dpkg-deb -f "$DEB" Recommends)"
echo "$RECOMMENDS" | grep -q 'bubblewrap' || fail "bubblewrap is not recommended"
[ "$PACKAGE" = "anvil" ] || fail "unexpected package name"
[ "$VERSION" = "0.0.1-1" ] || fail "unexpected version"
pass "package metadata"

dpkg-deb -x "$DEB" "$EXTRACT"
[ -x "$EXTRACT/usr/bin/anvil" ] || fail "anvil binary missing"
[ -x "$EXTRACT/usr/lib/anvil/anvil-core" ] || fail "core binary missing"
[ -f "$EXTRACT/usr/share/anvil/ui/index.html" ] || fail "UI bundle missing"
[ -f "$EXTRACT/usr/share/applications/anvil.desktop" ] || fail "desktop file missing"
[ -f "$EXTRACT/usr/share/metainfo/dev.anvil.Anvil.metainfo.xml" ] || fail "metainfo missing"
[ -f "$EXTRACT/usr/share/icons/hicolor/256x256/apps/anvil.png" ] || fail "icon missing"
[ -f "$EXTRACT/usr/share/doc/anvil/sbom.json" ] || fail "SBOM missing"
[ -f "$EXTRACT/usr/share/doc/anvil/threat-model.md" ] || fail "threat model missing"
[ -f "$EXTRACT/etc/apparmor.d/anvil-bwrap" ] || fail "bwrap AppArmor profile missing"
grep -q 'userns,' "$EXTRACT/etc/apparmor.d/anvil-bwrap" || fail "AppArmor profile does not permit userns"
if command -v apparmor_parser >/dev/null 2>&1; then
    apparmor_parser -Q -T "$EXTRACT/etc/apparmor.d/anvil-bwrap" || fail "AppArmor profile syntax is invalid"
fi
pass "package layout (including validated bwrap AppArmor profile)"

if dpkg-deb -c "$DEB" | grep -Eq '\.sysroot|/build/|/tmp/|anvil-test-key|/gnupg/'; then
    fail "package contains build/development artifacts"
fi
pass "no build artifacts or signing keys in package"

if grep -Rqs 'rm -rf.*\.config/anvil' "$EXTRACT"/usr/share/doc 2>/dev/null; then
    fail "documentation suggests deleting user config"
fi
pass "package scripts do not delete user data"

"$EXTRACT/usr/bin/anvil" --version | grep -q '^anvil 0.0.1' || fail "shell version failed"
"$EXTRACT/usr/lib/anvil/anvil-core" --version | grep -q '^anvil-core 0.0.1' || fail "core version failed"
pass "version commands"

SELF_TEST_CONFIG="$(mktemp -d /tmp/anvil-selftest-XXXXXX)"
SELF_TEST_CACHE="$(mktemp -d /tmp/anvil-selftest-cache-XXXXXX)"
trap 'rm -rf "$EXTRACT" "$SELF_TEST_CONFIG" "$SELF_TEST_CACHE"' EXIT
ANVIL_CONFIG_DIR="$SELF_TEST_CONFIG" ANVIL_CACHE_DIR="$SELF_TEST_CACHE" \
    "$EXTRACT/usr/lib/anvil/anvil-core" --self-test 2>/dev/null |
    python3 -c 'import json,sys; data=json.load(sys.stdin); assert data["ok"] is True, data; assert data["checks"]["pathTraversalRejected"] is True'
pass "core self-test"

if [ -n "${DISPLAY:-}" ] && [ "${ANVIL_SKIP_GUI_SMOKE:-0}" != "1" ]; then
    SMOKE_OUT="$(ANVIL_UI_DIR="$EXTRACT/usr/share/anvil/ui" \
        ANVIL_CORE_PATH="$EXTRACT/usr/lib/anvil/anvil-core" \
        ANVIL_CONFIG_DIR="$SELF_TEST_CONFIG" \
        ANVIL_CACHE_DIR="$SELF_TEST_CACHE" \
        timeout 45 "$EXTRACT/usr/bin/anvil" --smoke-test 2>/dev/null || true)"
    echo "$SMOKE_OUT" | grep -q 'SMOKE_OK' || fail "GUI smoke test failed: $SMOKE_OUT"
    pass "GUI smoke test (WebKit + React + core bridge)"
else
    warn "GUI smoke test skipped (no DISPLAY or ANVIL_SKIP_GUI_SMOKE=1)"
fi

if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$EXTRACT/usr/share/applications/anvil.desktop"
    pass "desktop entry validates"
else
    warn "desktop-file-validate not installed"
fi

if command -v appstreamcli >/dev/null 2>&1; then
    appstreamcli validate "$EXTRACT/usr/share/metainfo/dev.anvil.Anvil.metainfo.xml" \
        >/tmp/anvil-appstream.log 2>&1 || {
            cat /tmp/anvil-appstream.log >&2
            fail "AppStream metadata validation failed"
        }
    pass "AppStream metadata validates"
else
    warn "appstreamcli not installed"
fi

for binary in "$EXTRACT/usr/bin/anvil" "$EXTRACT/usr/lib/anvil/anvil-core"; do
    if readelf -d "$binary" | grep -Eq '(RPATH|RUNPATH)'; then
        fail "unexpected RPATH/RUNPATH in $binary"
    fi
    if ldd "$binary" 2>/dev/null | grep -q 'not found'; then
        ldd "$binary" | grep 'not found' >&2
        fail "missing shared library for $binary"
    fi
done
pass "no RPATH and all shared libraries resolve"

if [ -d "$ROOT/dist/apt/dists/noble" ]; then
    gpgv --keyring "$ROOT/dist/anvil-test-key.gpg" \
        "$ROOT/dist/apt/dists/noble/Release.gpg" \
        "$ROOT/dist/apt/dists/noble/Release" >/dev/null 2>&1 || fail "APT Release signature invalid"
    HASH="$(sha256sum "$DEB" | awk '{print $1}')"
    grep -q "$HASH" "$ROOT/dist/apt/dists/noble/main/binary-amd64/Packages" || fail "APT Packages hash mismatch"
    pass "APT repository signature and package hash"
fi

if command -v lintian >/dev/null 2>&1; then
    lintian "$DEB" || warn "lintian reported findings"
else
    warn "lintian is not installed; structural checks above were used instead"
fi

echo "PASS: $DEB"
