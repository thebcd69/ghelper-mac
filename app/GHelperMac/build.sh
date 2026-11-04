#!/bin/bash
# Build GHelperMac.app (x86_64, macOS 13+) with swiftc, no Xcode project.
# Usage: app/GHelperMac/build.sh <out dir> [version]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?output dir}"
VER="${2:-0.1.0}"
SDK="$(xcrun --sdk macosx --show-sdk-path)"
APP="$OUT/GHelperMac.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
xcrun swiftc -O -swift-version 5 -parse-as-library -module-name GHelperMac \
    -target x86_64-apple-macos13.0 -sdk "$SDK" \
    "$HERE"/Sources/*.swift -o "$APP/Contents/MacOS/GHelperMac"
sed "s/@VERSION@/$VER/g" "$HERE/Info.plist" > "$APP/Contents/Info.plist"
plutil -lint "$APP/Contents/Info.plist"
codesign --force --sign - --timestamp=none "$APP"
codesign -dv "$APP" 2>&1 | head -5
file "$APP/Contents/MacOS/GHelperMac"
