#!/usr/bin/env bash
# Version consistency guard.
# core/include/anvil/version.hpp is canonical; CMake, the shell, the UI
# manifest, the Debian changelog and the AppStream metadata must agree.
# Usage: check-version.sh [--print]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION_FILE="$ROOT/core/include/anvil/version.hpp"

fail() { printf 'error: %s\n' "$1" >&2; exit 1; }

VERSION="$(sed -n 's/.*inline constexpr std::string_view kVersion = "\([^"]*\)".*/\1/p' \
    "$VERSION_FILE")"
[ -n "$VERSION" ] || fail "cannot read kVersion from $VERSION_FILE"
printf '%s' "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' ||
    fail "kVersion '$VERSION' is not MAJOR.MINOR.PATCH"

if [ "${1:-}" = "--print" ]; then
    printf '%s\n' "$VERSION"
    exit 0
fi

# CMake must derive the project version, never pin its own copy.
grep -q 'project(anvil VERSION ${ANVIL_VERSION}' "$ROOT/CMakeLists.txt" ||
    fail "CMakeLists.txt does not derive the project version from version.hpp"

# The shell gets its version from CMake; it must not carry a literal.
if grep -q '"0\.[0-9]\+\.[0-9]\+"' "$ROOT/shell/src/main.cpp"; then
    fail "shell/src/main.cpp contains a hardcoded version literal"
fi

# UI manifest and lockfile (root entry and the empty-path package entry).
grep -q "\"version\": \"$VERSION\"" "$ROOT/apps/desktop/package.json" ||
    fail "apps/desktop/package.json is not at $VERSION"
COUNT="$(grep -c "\"version\": \"$VERSION\"" "$ROOT/apps/desktop/package-lock.json" || true)"
[ "$COUNT" -ge 2 ] || fail "apps/desktop/package-lock.json is not at $VERSION"

# Debian changelog: newest entry must be this upstream version, optionally
# with a pre-release suffix (0.0.2~pre1-1).
head -n 1 "$ROOT/packaging/debian/changelog" |
    grep -Eq "^anvil \(${VERSION}(~[^)]+)?-[0-9]+\) " ||
    fail "packaging/debian/changelog does not start with $VERSION"

# AppStream metadata should advertise the newest release.
grep -q "<release version=\"$VERSION\"" "$ROOT/packaging/debian/anvil.metainfo.xml" ||
    fail "packaging/debian/anvil.metainfo.xml has no <release version=\"$VERSION\">"

printf 'version %s is consistent across CMake, shell, UI, Debian and AppStream\n' "$VERSION"
