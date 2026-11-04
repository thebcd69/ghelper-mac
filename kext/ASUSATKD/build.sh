#!/bin/bash
# Build ASUSATKD.kext without an Xcode project (clang + MacKernelSDK).
# Usage: kext/ASUSATKD/build.sh <MacKernelSDK dir> <out dir> [version]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="${1:?MacKernelSDK path}"
OUT="${2:?output dir}"
VER="${3:-0.1.0}"
OBJ="$OUT/obj"
mkdir -p "$OBJ"
CFLAGS=(-target x86_64-apple-macos10.12 -O2 -g -fno-builtin -fno-common -fno-stack-protector
        -mkernel -fapple-kext -nostdinc -nostdinc++ -fno-rtti -fno-exceptions
        -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -D__MACH__
        -DATKD_VERSION="\"$VER\"" -I "$SDK/Headers" -Wall -Wextra -Wno-unused-parameter -Werror)
xcrun clang++ "${CFLAGS[@]}" -std=gnu++17 -c "$HERE/ASUSATKD.cpp" -o "$OBJ/ASUSATKD.o"
xcrun clang++ "${CFLAGS[@]}" -std=gnu++17 -c "$HERE/ASUSATKDUserClient.cpp" -o "$OBJ/ASUSATKDUserClient.o"
xcrun clang   "${CFLAGS[@]}" -std=gnu11  -c "$HERE/kmod_info.c" -o "$OBJ/kmod_info.o"
KEXT="$OUT/ASUSATKD.kext"
rm -rf "$KEXT"
mkdir -p "$KEXT/Contents/MacOS"
xcrun clang++ -target x86_64-apple-macos10.12 -fapple-kext -mkernel -nostdlib \
    -Xlinker -kext -Xlinker -no_adhoc_codesign \
    "$OBJ/ASUSATKD.o" "$OBJ/ASUSATKDUserClient.o" "$OBJ/kmod_info.o" \
    -L "$SDK/Library/x86_64" -lkmod -o "$KEXT/Contents/MacOS/ASUSATKD"
sed "s/@VERSION@/$VER/g" "$HERE/Info.plist" > "$KEXT/Contents/Info.plist"
plutil -lint "$KEXT/Contents/Info.plist"
file "$KEXT/Contents/MacOS/ASUSATKD"
otool -hv "$KEXT/Contents/MacOS/ASUSATKD" | tail -1
echo "Undefined symbols (must all be kernel KPI exports):"
nm -u "$KEXT/Contents/MacOS/ASUSATKD" | sed 's/^/  /'
