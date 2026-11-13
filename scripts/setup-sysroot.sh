#!/usr/bin/env bash
# Build a local header/library sysroot for environments where the developer
# cannot run sudo (for example a restricted container). On a normal Ubuntu
# 24.04 machine, installing the Build-Depends from packaging/debian/control is
# simpler and is the recommended path.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSROOT="$ROOT/.sysroot"
DEBS="$SYSROOT/debs"
PREFIX="$SYSROOT/root"

mkdir -p "$DEBS" "$PREFIX"

PACKAGES=(
    libwebkit2gtk-4.1-dev libjavascriptcoregtk-4.1-dev libgtk-3-dev
    libsoup-3.0-dev libcairo2-dev libpango1.0-dev libgdk-pixbuf-2.0-dev
    libatk1.0-dev libatk-bridge2.0-dev libepoxy-dev libharfbuzz-dev
    libfontconfig1-dev libfreetype-dev libpng-dev libgraphite2-dev
    libfribidi-dev libthai-dev libdatrie-dev
    libxcb-shm0-dev libx11-dev libxext-dev libxrender-dev libxcb1-dev
    libpixman-1-dev libxau-dev libxdmcp-dev libxfixes-dev libxi-dev
    libxrandr-dev libxcursor-dev libxinerama-dev libxcomposite-dev
    libxdamage-dev libatspi2.0-dev libdbus-1-dev libxtst-dev
    libsysprof-capture-4-dev libpsl-dev libnghttp2-dev libbrotli-dev
    clangd-18 libclang-common-18-dev
    libgrpc++1.51t64 libgrpc29t64 libprotobuf32t64 libabsl20220623t64
)

cd "$DEBS"
apt-get download "${PACKAGES[@]}" 2>&1 | tail -5
for package in *.deb; do
    dpkg-deb -x "$package" "$PREFIX"
done

# Rewrite pkg-config prefixes to the extracted tree. System .pc files remain
# untouched, so glib/freetype and other installed dev packages keep working.
find "$PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig" -name '*.pc' -print0 |
    xargs -0 sed -i \
        -e "s|^prefix=/usr|prefix=$PREFIX/usr|" \
        -e "s|^libdir=.*|libdir=$PREFIX/usr/lib/x86_64-linux-gnu|"

# Dev packages ship .so symlinks that point to runtime libraries. Repair them
# from the host when the runtime package is already installed, or from the
# extracted tree for packages we downloaded.
for link in $(find "$PREFIX/usr/lib/x86_64-linux-gnu" -maxdepth 1 -xtype l); do
    target="$(basename "$(readlink "$link")")"
    if [ -e "/usr/lib/x86_64-linux-gnu/$target" ]; then
        ln -sf "/usr/lib/x86_64-linux-gnu/$target" "$link"
    elif [ -e "$PREFIX/usr/lib/x86_64-linux-gnu/$target" ]; then
        ln -sf "$PREFIX/usr/lib/x86_64-linux-gnu/$target" "$link"
    fi
done

cat > "$SYSROOT/env.sh" <<ENV
export PKG_CONFIG_PATH="$PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig:\${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="$PREFIX/usr/lib/x86_64-linux-gnu:\${LD_LIBRARY_PATH:-}"
ENV

echo "sysroot ready: $PREFIX"
echo "source $SYSROOT/env.sh before configuring cmake."
