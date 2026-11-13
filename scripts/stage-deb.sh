#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:?usage: stage-deb.sh <target-root>}"
BUILD="$ROOT/build/release"
UI="$ROOT/apps/desktop/dist"

if [ ! -x "$BUILD/shell/anvil" ] || [ ! -x "$BUILD/core/anvil-core" ]; then
    echo "error: release binaries are missing; run scripts/build.sh release" >&2
    exit 1
fi
if [ ! -f "$UI/index.html" ] || [ ! -f "$UI/assets/index.js" ]; then
    echo "error: UI bundle is missing; run npm run build in apps/desktop" >&2
    exit 1
fi

rm -rf "$TARGET"
install -d "$TARGET/usr/bin" \
           "$TARGET/usr/lib/anvil" \
           "$TARGET/usr/share/anvil/ui" \
           "$TARGET/usr/share/applications" \
           "$TARGET/usr/share/metainfo" \
           "$TARGET/usr/share/icons/hicolor/256x256/apps" \
           "$TARGET/usr/share/icons/hicolor/512x512/apps" \
           "$TARGET/usr/share/icons/hicolor/scalable/apps" \
           "$TARGET/usr/share/doc/anvil/adr" \
           "$TARGET/etc/apparmor.d"

install -Dm755 "$BUILD/shell/anvil" "$TARGET/usr/bin/anvil"
install -Dm755 "$BUILD/core/anvil-core" "$TARGET/usr/lib/anvil/anvil-core"
strip --strip-unneeded "$TARGET/usr/bin/anvil" "$TARGET/usr/lib/anvil/anvil-core" 2>/dev/null || true

cp -a "$UI/." "$TARGET/usr/share/anvil/ui/"
chmod -R u=rwX,go=rX "$TARGET/usr/share/anvil/ui"

install -Dm644 "$ROOT/assets/anvil-256.png" "$TARGET/usr/share/icons/hicolor/256x256/apps/anvil.png"
install -Dm644 "$ROOT/assets/anvil-512.png" "$TARGET/usr/share/icons/hicolor/512x512/apps/anvil.png"
install -Dm644 "$ROOT/assets/anvil.svg" "$TARGET/usr/share/icons/hicolor/scalable/apps/anvil.svg"
install -Dm644 "$ROOT/packaging/debian/anvil.desktop" "$TARGET/usr/share/applications/anvil.desktop"
install -Dm644 "$ROOT/packaging/debian/anvil.metainfo.xml" "$TARGET/usr/share/metainfo/dev.anvil.Anvil.metainfo.xml"
install -Dm644 "$ROOT/packaging/debian/anvil-bwrap.apparmor" "$TARGET/etc/apparmor.d/anvil-bwrap"

install -Dm644 "$ROOT/packaging/debian/copyright" "$TARGET/usr/share/doc/anvil/copyright"
install -Dm644 "$ROOT/LICENSE" "$TARGET/usr/share/doc/anvil/LICENSE"
install -Dm644 "$ROOT/README.md" "$TARGET/usr/share/doc/anvil/README.md"
install -Dm644 "$ROOT/docs/ARCHITECTURE.md" "$TARGET/usr/share/doc/anvil/ARCHITECTURE.md"
install -Dm644 "$ROOT/docs/BUILD.md" "$TARGET/usr/share/doc/anvil/BUILD.md" 2>/dev/null || true
install -Dm644 "$ROOT/docs/benchmarks.md" "$TARGET/usr/share/doc/anvil/benchmarks.md" 2>/dev/null || true
install -Dm644 "$ROOT/docs/RELEASE-CHECKLIST.md" "$TARGET/usr/share/doc/anvil/RELEASE-CHECKLIST.md" 2>/dev/null || true
install -Dm644 "$ROOT/docs/security/threat-model.md" "$TARGET/usr/share/doc/anvil/threat-model.md"
install -Dm644 "$ROOT/docs/adr/0001-native-cpp-shell.md" "$TARGET/usr/share/doc/anvil/adr/0001-native-cpp-shell.md"
gzip -9 -c "$ROOT/packaging/debian/changelog" > "$TARGET/usr/share/doc/anvil/changelog.Debian.gz"
chmod 0644 "$TARGET/usr/share/doc/anvil/changelog.Debian.gz"

if [ -f "$ROOT/dist/sbom.json" ]; then
    install -Dm644 "$ROOT/dist/sbom.json" "$TARGET/usr/share/doc/anvil/sbom.json"
fi

echo "staged package tree at $TARGET"
