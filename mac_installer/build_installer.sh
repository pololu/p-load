#!/bin/bash

set -u -e

VERSION=1.0.0

make

rm -Rf tmpb
mkdir tmpb
cp mac_installer/README.txt tmpb
cp src/p-load tmpb
ln -s /usr/bin tmpb/usrbin
hdiutil create mac_installer/p-load-$VERSION.dmg -volname "p-load" \
  -fs HFS+ -srcfolder tmpb
