#!/bin/bash

# This is an MSYS2 bash script that we use at Pololu to generate the official
# installer for Windows.  To use it:
#
# 1. Copy app.ico (the Pololu logo), setup_banner_wix.bmp and
# setup_welcome_wix.bmp to this direcotry.  We don't include these files in
# source control because they contain the Pololu logo.  Note:
#    - setup_banner_wix.bmp should be 493x58
#    - setup_welcome_wix.bmp should be 493x312
# 2. From your CMake build directory, run:
#    make && sh ../windows-installer/release.sh

set -ue

MSBUILD="/c/Program Files (x86)/MSBuild/14.0/Bin/MSBuild.exe"
SIGNTOOL="/c/Program Files (x86)/Windows Kits/10/bin/x64/signtool.exe"
STAGINGDIR="msi_staging"
SIGNFLAGS="-fd sha256 -tr http://timestamp.globalsign.com/?signature=sha2 -td sha256"

if [ "$MSYSTEM" == "MINGW32" ]; then
    MINGW_PREFIX="/mingw32"
else
    # We don't make installers for 64-bit software.
    echo "Invalid value for MSYSTEM: $MSYSTEM"
    exit 1
fi

rm -rf "$STAGINGDIR"
mkdir -p "$STAGINGDIR"

# Helps us call SetupCopyOEMInf to install the INF files.
cp "$MINGW_PREFIX/bin/libusbp-install-helper-1.dll" "$STAGINGDIR"

# These are the files needed for the CLI.
cp "$MINGW_PREFIX/bin/libwinpthread-1.dll" "$STAGINGDIR" # MIT license + custom
cp "$MINGW_PREFIX/bin/libstdc++-6.dll" "$STAGINGDIR"     # GPLv3+
cp "$MINGW_PREFIX/bin/libgcc_s_dw2-1.dll" "$STAGINGDIR"  # GPLv3+
cp "$MINGW_PREFIX/bin/libusbp-1.dll" "$STAGINGDIR"       # our MIT license
cp "p-load.exe" "$STAGINGDIR"

"$SIGNTOOL" sign -n "Pololu Corporation" $SIGNFLAGS "$STAGINGDIR/p-load.exe"

"$MSBUILD" -t:rebuild -p:Configuration=Release -p:TreatWarningsAsErrors=True \
           windows-installer/p-load.wixproj

cp windows-installer/bin/Release/en-us/*.msi .

"$SIGNTOOL" sign -n "Pololu Corporation" $SIGNFLAGS -d "Pololu USB Bootloader Utility Setup" *.msi
