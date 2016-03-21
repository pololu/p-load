#!/bin/bash

# Run this from your CMake build directory to create a Mac OS X flat package
# (installer).

set -ue

VERSION=$(cat version.txt)
STAGINGDIR="mac_release"
RESDIR="mac_resources"
APPDIR="$STAGINGDIR/Pololu USB Bootloader Utility"
BINDIR="$APPDIR/bin"
PATHDIR="$STAGINGDIR/path"
PKG="p-load-$VERSION-mac.pkg"

PATH=$PATH:`dirname $0`  # so we can run other scripts in the same directory

rm -rf "$RESDIR"
mkdir -p "$RESDIR"

cp mac-installer/welcome.html "$RESDIR"

rm -rf "$STAGINGDIR"
mkdir -p "$BINDIR"

cp `dirname $0`/../LICENSE.html "$APPDIR"
cp p-load "$BINDIR"
fix_dylibs.rb "$BINDIR/p-load"

rm -rf "$PATHDIR"
mkdir -p "$PATHDIR"
echo "/Applications/Pololu USB Bootloader Utility/bin" > "$PATHDIR/99-p-load"

pkgbuild \
  --identifier com.pololu.p-load.app \
  --version "$VERSION" \
  --root "$STAGINGDIR" \
  --install-location /Applications \
  app.pkg

pkgbuild \
  --identifier com.pololu.p-load.path \
  --version "$VERSION" \
  --root "$PATHDIR" \
  --install-location /etc/paths.d \
  path.pkg

productbuild \
  --identifier com.pololu.p-load \
  --version "$VERSION" \
  --resources "$RESDIR" \
  --distribution mac-installer/distribution.xml \
  "$PKG"

echo created "$PKG"
