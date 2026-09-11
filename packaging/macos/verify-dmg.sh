#!/usr/bin/env bash
#
# Mount a DMG and assert it is the thing we meant to ship: a bundle that
# carries its own Qt, the daemon and the CLI, a valid signature, and the
# Applications shortcut. v0.1.0 on Windows shipped an app that could not start;
# this is the macOS answer to that.
#
#   packaging/macos/verify-dmg.sh build/sony-device-center-0.1.4-macOS.dmg [arm64 x86_64]
#
# Extra arguments are architectures the main binary must contain.

set -euo pipefail

dmg=$1; shift
mount=$(mktemp -d)
trap 'hdiutil detach "$mount" -quiet || true; rmdir "$mount" 2>/dev/null || true' EXIT

hdiutil attach -nobrowse -readonly -mountpoint "$mount" "$dmg" >/dev/null
app="$mount/Sony Device Center.app"

fail() { echo "::error::$dmg: $*" >&2; exit 1; }

[ -d "$app" ] || fail "no Sony Device Center.app"
[ -L "$mount/Applications" ] || fail "no Applications symlink"

for path in \
    Contents/MacOS/sony-device-center \
    Contents/MacOS/sonyd \
    Contents/MacOS/sonyctl \
    Contents/Frameworks/QtCore.framework \
    Contents/Frameworks/QtQuick.framework \
    Contents/Frameworks/QtQuickControls2.framework \
    Contents/Resources/qml/QtQuick/Controls \
    Contents/Resources/qml/QtQuick/Layouts \
    Contents/Resources/qml/QtQuick/Shapes \
    Contents/PlugIns/platforms/libqcocoa.dylib \
    Contents/Resources/AppIcon.icns
do
    [ -e "$app/$path" ] || fail "missing $path"
done

for arch in "$@"; do
    lipo -archs "$app/Contents/MacOS/sony-device-center" | grep -qw "$arch" || fail "not built for $arch"
done

codesign --verify --deep --strict "$app" || fail "signature does not verify"

# The binaries must find their libraries on a Mac that has no Qt installed.
# Nothing in the bundle may reference the build machine's Qt prefix.
if otool -L "$app/Contents/MacOS/sony-device-center" | grep -E '/(opt/homebrew|usr/local|Users/runner)/' ; then
    fail "main binary links Qt from the build machine, macdeployqt did not run"
fi

"$app/Contents/MacOS/sonyctl" --help >/dev/null || fail "sonyctl does not run"

echo "$dmg: ok ($(du -h "$dmg" | cut -f1))"
