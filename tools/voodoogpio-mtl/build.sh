#!/bin/bash
# Build VoodooGPIO.kext (with Meteor Lake-P) using clang + MacKernelSDK, no Xcode.
# Usage: build.sh <VoodooGPIO src dir (the inner VoodooGPIO/)> <MacKernelSDK> <out dir> <installed VoodooGPIO.kext (for Info.plist)>
set -euo pipefail
S="$1"; SDK="$2"; OUT="$3"; OLD="$4"; HERE="$(cd "$(dirname "$0")" && pwd)"; OBJ="$OUT/obj-gpio"; rm -rf "$OBJ"; mkdir -p "$OBJ"
VER=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$OLD/Contents/Info.plist")
CF=(-target x86_64-apple-macos10.12 -O2 -fno-builtin -fno-common -fno-stack-protector -mkernel -fapple-kext -nostdinc
    -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -D__MACH__ -I "$SDK/Headers" -I "$S" -w)
for f in "$S"/*.cpp "$S"/*/*.cpp; do
  xcrun clang++ "${CF[@]}" -nostdinc++ -fno-rtti -fno-exceptions -std=gnu++17 -c "$f" -o "$OBJ/$(basename "${f%.cpp}").o"
done
xcrun clang "${CF[@]}" -DKEXT_VERSION="\"$VER\"" -std=gnu11 -c "$HERE/kmod_info.c" -o "$OBJ/kmod_info.o"
K="$OUT/VoodooGPIO.kext"; rm -rf "$K"; mkdir -p "$K/Contents/MacOS"
xcrun clang++ -target x86_64-apple-macos10.12 -fapple-kext -mkernel -nostdlib -Xlinker -kext -Xlinker -no_adhoc_codesign \
    "$OBJ"/*.o -L "$SDK/Library/x86_64" -lkmod -o "$K/Contents/MacOS/VoodooGPIO"
# Info.plist: the installed one (same bundle id, version, libraries) + the Meteor Lake-P personality
cp "$OLD/Contents/Info.plist" "$K/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Delete :IOKitPersonalities:VoodooGPIOMeteorLakeP" "$K/Contents/Info.plist" 2>/dev/null || true
/usr/libexec/PlistBuddy \
  -c "Add :IOKitPersonalities:VoodooGPIOMeteorLakeP dict" \
  -c "Add :IOKitPersonalities:VoodooGPIOMeteorLakeP:CFBundleIdentifier string org.coolstar.VoodooGPIO" \
  -c "Add :IOKitPersonalities:VoodooGPIOMeteorLakeP:IOClass string VoodooGPIOMeteorLakeP" \
  -c "Add :IOKitPersonalities:VoodooGPIOMeteorLakeP:IONameMatch array" \
  -c "Add :IOKitPersonalities:VoodooGPIOMeteorLakeP:IONameMatch:0 string INTC1083" \
  -c "Add :IOKitPersonalities:VoodooGPIOMeteorLakeP:IONameMatch:1 string INTC105E" \
  -c "Add :IOKitPersonalities:VoodooGPIOMeteorLakeP:IOProviderClass string IOService" "$K/Contents/Info.plist"
plutil -lint "$K/Contents/Info.plist"
echo "== symbols only in old (missing in new):"
comm -23 <(nm -gU "$OLD/Contents/MacOS/VoodooGPIO" | awk '{print $3}' | sort) <(nm -gU "$K/Contents/MacOS/VoodooGPIO" | awk '{print $3}' | sort) | head -30
echo "== symbols only in new:"
comm -13 <(nm -gU "$OLD/Contents/MacOS/VoodooGPIO" | awk '{print $3}' | sort) <(nm -gU "$K/Contents/MacOS/VoodooGPIO" | awk '{print $3}' | sort) | head -30
echo "== undefined:"; nm -u "$K/Contents/MacOS/VoodooGPIO" | wc -l
shasum -a 256 "$K/Contents/MacOS/VoodooGPIO" | cut -c1-16
