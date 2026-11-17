#!/bin/bash
# Build GU605CPU.kext with clang + MacKernelSDK. Usage: kext/GU605CPU/build.sh <MacKernelSDK dir> <out dir> [version]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; SDK="${1:?}"; OUT="${2:?}"; VER="${3:-0.1.0}"; OBJ="$OUT/obj-cpu"; mkdir -p "$OBJ"
CFLAGS=(-target x86_64-apple-macos10.12 -O2 -g -fno-builtin -fno-common -fno-stack-protector -mkernel -fapple-kext -nostdinc
        -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -D__MACH__ -DCPU_VERSION="\"$VER\"" -I "$SDK/Headers"
        -Wall -Wextra -Wno-unused-parameter -Werror)
xcrun clang++ "${CFLAGS[@]}" -nostdinc++ -fno-rtti -fno-exceptions -std=gnu++17 -c "$HERE/GU605CPU.cpp" -o "$OBJ/GU605CPU.o"
xcrun clang "${CFLAGS[@]}" -Wno-unused-command-line-argument -std=gnu11 -c "$HERE/kmod_info.c" -o "$OBJ/kmod_info.o"
K="$OUT/GU605CPU.kext"; rm -rf "$K"; mkdir -p "$K/Contents/MacOS"
xcrun clang++ -target x86_64-apple-macos10.12 -fapple-kext -mkernel -nostdlib -Xlinker -kext -Xlinker -no_adhoc_codesign \
    "$OBJ/GU605CPU.o" "$OBJ/kmod_info.o" -L "$SDK/Library/x86_64" -lkmod -o "$K/Contents/MacOS/GU605CPU"
sed "s/@VERSION@/$VER/g" "$HERE/Info.plist" > "$K/Contents/Info.plist"; plutil -lint "$K/Contents/Info.plist"
nm -u "$K/Contents/MacOS/GU605CPU" | sed 's/^/  /'
