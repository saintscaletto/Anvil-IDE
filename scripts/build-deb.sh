#!/usr/bin/env bash
# Build the Anvil 0.0.1 Debian package, SBOM and a signed local APT repository.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="0.0.1-1"
UPSTREAM="0.0.1"
ARCH="amd64"
DIST="$ROOT/dist"
STAGE="$ROOT/build/deb/root"
DEB="$DIST/anvil_${VERSION}_${ARCH}.deb"
REPO="$DIST/apt"
GNUPG="$DIST/gnupg"

mkdir -p "$DIST"

echo "== tests =="
"$ROOT/scripts/test.sh"

echo "== release build =="
"$ROOT/scripts/build.sh" release

echo "== stage =="
"$ROOT/scripts/stage-deb.sh" "$STAGE"
"$ROOT/scripts/gen-sbom.py" "$STAGE" "$DIST/sbom.json"
install -Dm644 "$DIST/sbom.json" "$STAGE/usr/share/doc/anvil/sbom.json"

echo "== dependencies =="
DEPWORK="$ROOT/build/deb/depwork"
rm -rf "$DEPWORK"
mkdir -p "$DEPWORK/debian"
cat > "$DEPWORK/debian/control" <<'CONTROL'
Source: anvil
Section: devel
Priority: optional
Maintainer: Anvil IDE Authors <anvil@localhost>
Package: anvil
Architecture: amd64
Description: dependency probe
CONTROL
DEPS="$(cd "$DEPWORK" && dpkg-shlibdeps -O \
    "$STAGE/usr/bin/anvil" \
    "$STAGE/usr/lib/anvil/anvil-core" 2>/dev/null |
    sed -n 's/^shlibs:Depends=//p')"
if [ -z "$DEPS" ]; then
    echo "warning: dpkg-shlibdeps produced no output; using the verified fallback set" >&2
    DEPS="libc6, libstdc++6, libgcc-s1, libgtk-3-0t64, libwebkit2gtk-4.1-0, libjavascriptcoregtk-4.1-0, libglib2.0-0t64, libspdlog1.12-fmt9, libfmt9"
fi
INSTALLED_SIZE="$(du -sk "$STAGE/usr" | awk '{print $1}')"

mkdir -p "$STAGE/DEBIAN"
cat > "$STAGE/DEBIAN/control" <<CONTROL
Package: anvil
Version: $VERSION
Section: devel
Priority: optional
Architecture: $ARCH
Depends: $DEPS
Recommends: bubblewrap, clangd-18 | clangd, gdb | lldb, git, cmake, ninja-build
Suggests: clangd, rust-analyzer, python3-pylsp
Installed-Size: $INSTALLED_SIZE
Maintainer: Anvil IDE Authors <anvil@localhost>
Homepage: https://anvil.dev
Description: Fast, lightweight C++ IDE with a native core
 Anvil is a C++-first integrated development environment combining
 clangd-powered intelligence with a native C++ core for filesystem
 access, language-server management, and security policy.
 .
 The editor UI is built with React and Monaco inside the system
 WebKitGTK view. Language servers run in a bubblewrap sandbox with
 seccomp, without network access and with a read-only workspace.
 .
 This 0.0.1 release is experimental. Build, debug, terminal, Git write
 operations, plugins, and AI are scheduled for later phases.
CONTROL

install -m755 "$ROOT/packaging/debian/postinst" "$STAGE/DEBIAN/postinst"
install -m755 "$ROOT/packaging/debian/prerm" "$STAGE/DEBIAN/prerm"
install -m755 "$ROOT/packaging/debian/postrm" "$STAGE/DEBIAN/postrm"
cat > "$STAGE/DEBIAN/conffiles" <<'CONFFILES'
/etc/apparmor.d/anvil-bwrap
CONFFILES

# Ensure metadata files were not accidentally made executable.
chmod 0644 "$STAGE/usr/share/applications/anvil.desktop" \
           "$STAGE/usr/share/metainfo/dev.anvil.Anvil.metainfo.xml" \
           "$STAGE/usr/share/doc/anvil/copyright"

rm -f "$DEB"
dpkg-deb --build --root-owner-group "$STAGE" "$DEB"

echo "== package =="
dpkg-deb --info "$DEB"
ls -lh "$DEB"
sha256sum "$DEB" | tee "$DIST/SHA256SUMS"

echo "== apt repository =="
rm -rf "$REPO/dists" "$REPO/pool"
mkdir -p "$REPO/pool/main/a/anvil" "$REPO/dists/noble/main/binary-amd64"
cp "$DEB" "$REPO/pool/main/a/anvil/"
(
    cd "$REPO"
    dpkg-scanpackages --multiversion pool/main/a/anvil \
        > dists/noble/main/binary-amd64/Packages
    gzip -9 -kf dists/noble/main/binary-amd64/Packages
    apt-ftparchive \
        -o APT::FTPArchive::Release::Origin="Anvil" \
        -o APT::FTPArchive::Release::Label="Anvil IDE" \
        -o APT::FTPArchive::Release::Suite="noble" \
        -o APT::FTPArchive::Release::Codename="noble" \
        -o APT::FTPArchive::Release::Architectures="amd64" \
        -o APT::FTPArchive::Release::Components="main" \
        release dists/noble > dists/noble/Release
)

mkdir -p "$GNUPG"
chmod 0700 "$GNUPG"
if ! GNUPGHOME="$GNUPG" gpg --batch --list-secret-keys --with-colons 2>/dev/null | grep -q '^sec'; then
    echo "generating a demonstration repository signing key"
    GNUPGHOME="$GNUPG" gpg --batch --pinentry-mode loopback --passphrase '' \
        --quick-generate-key "Anvil Test Signing Key <anvil-test@localhost>" ed25519 sign 2y
fi

GNUPGHOME="$GNUPG" gpg --batch --yes --armor --detach-sign \
    -o "$REPO/dists/noble/Release.gpg" "$REPO/dists/noble/Release"
GNUPGHOME="$GNUPG" gpg --batch --yes --clearsign \
    -o "$REPO/dists/noble/InRelease" "$REPO/dists/noble/Release"
GNUPGHOME="$GNUPG" gpg --armor --export \
    "Anvil Test Signing Key" > "$DIST/anvil-test-key.asc"
GNUPGHOME="$GNUPG" gpg --export > "$DIST/anvil-test-key.gpg"

GNUPGHOME="$GNUPG" gpg --batch --verify \
    "$REPO/dists/noble/Release.gpg" "$REPO/dists/noble/Release"
GNUPGHOME="$GNUPG" gpg --batch --verify "$REPO/dists/noble/InRelease"
gpgv --keyring "$DIST/anvil-test-key.gpg" \
    "$REPO/dists/noble/Release.gpg" "$REPO/dists/noble/Release"

echo "== verify =="
"$ROOT/scripts/verify-deb.sh" "$DEB"

cat <<SUMMARY

Built:
  $DEB
  $DIST/sbom.json
  $REPO
  $DIST/anvil-test-key.asc

The repository signing key is a demonstration key and must not be used for
public distribution. Debian signs repository metadata; individual .deb files
are verified through the Release/Packages hashes.

Install on Ubuntu 24.04:
  sudo apt install ./dist/anvil_${VERSION}_${ARCH}.deb
SUMMARY
