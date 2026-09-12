#!/usr/bin/env bash
#
# Turn a CMake build into a drag-to-Applications disk image.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
#   cmake --build build --parallel
#   packaging/macos/build-dmg.sh build       # -> build/sony-device-center-<version>-macOS.dmg
#
# One bundle, three binaries: the Qt app, plus sonyd and sonyctl inside
# Contents/MacOS so a single signature covers everything. macdeployqt copies the
# Qt frameworks and the QML modules Main.qml imports; dmgbuild lays the window
# out without touching Finder, which is why it works on a headless CI runner.
#
# Signing is off by default and a matter of two environment variables:
#
#   SONY_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
#       Sign with hardened runtime and a timestamp. Unset: ad-hoc signature,
#       which is what a Mac needs to remember the Bluetooth permission grant.
#   SONY_NOTARY_PROFILE="notary"
#       Also submit to Apple and staple the ticket. The profile comes from
#       `xcrun notarytool store-credentials`.
#
# Needs: macdeployqt (from Qt), dmgbuild (pip install dmgbuild).

set -euo pipefail

build_dir=$(cd "${1:-build}" && pwd)
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

version=$(sed -n 's/^project(SonyDeviceCenter VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")
built_app=$(find "$build_dir/apps/device-center" -maxdepth 1 -name '*.app' | head -n 1)
[ -n "$built_app" ] || { echo "no .app under $build_dir/apps/device-center; build first" >&2; exit 1; }

macdeployqt=${MACDEPLOYQT:-$(command -v macdeployqt || true)}
if [ -z "$macdeployqt" ]; then
    for qmake in qmake6 qmake; do
        if command -v "$qmake" >/dev/null; then
            macdeployqt="$("$qmake" -query QT_INSTALL_BINS)/macdeployqt"
            break
        fi
    done
fi
[ -x "${macdeployqt:-}" ] || { echo "macdeployqt not found; set MACDEPLOYQT or put Qt's bin on PATH" >&2; exit 1; }
command -v dmgbuild >/dev/null || { echo "dmgbuild not found; pip3 install dmgbuild" >&2; exit 1; }

# Stage a fresh copy. The target is called sony-device-center, but what lands
# in /Applications should read like an app.
staging="$build_dir/dmg-staging"
app="$staging/Sony Device Center.app"
rm -rf "$staging"
mkdir -p "$staging"
cp -R "$built_app" "$app"
cp "$build_dir/apps/sonyd/sonyd" "$build_dir/apps/sonyctl/sonyctl" "$app/Contents/MacOS/"

echo "==> Deploying Qt into $(basename "$app")"
"$macdeployqt" "$app" -qmldir="$root/apps/device-center/qml" -always-overwrite

# macdeployqt ships every plugin of every framework it touches. The QML
# import scan reaches QtSql through QtQuick.LocalStorage, and with it the
# database drivers, whose Postgres/ODBC links then fail to resolve. There is
# no database in this app.
rm -rf "$app/Contents/PlugIns/sqldrivers" \
       "$app/Contents/Frameworks/QtSql.framework" \
       "$app/Contents/Resources/qml/QtQuick/LocalStorage"

# main.cpp pins the Basic style, so the other Controls styles are 20 MB the
# app never loads. verify-dmg.sh launches the result to prove that.
for style in Fusion Imagine Material Universal FluentWinUI3 macOS iOS Windows; do
    rm -rf "$app/Contents/Frameworks/QtQuickControls2${style}.framework" \
           "$app/Contents/Frameworks/QtQuickControls2${style}StyleImpl.framework" \
           "$app/Contents/Resources/qml/QtQuick/Controls/${style}"
done

echo "==> Signing"
if [ -n "${SONY_CODESIGN_IDENTITY:-}" ]; then
    codesign --force --deep --options runtime --timestamp \
        --sign "$SONY_CODESIGN_IDENTITY" "$app"
else
    codesign --force --deep --sign - "$app"
fi
codesign --verify --deep --strict "$app"

dmg="$build_dir/sony-device-center-$version-macOS.dmg"
echo "==> Writing $(basename "$dmg")"
rm -f "$dmg"
(cd "$here" && dmgbuild -s dmgbuild.py -D app="$app" "Sony Device Center" "$dmg")

if [ -n "${SONY_NOTARY_PROFILE:-}" ]; then
    echo "==> Notarizing"
    xcrun notarytool submit "$dmg" --keychain-profile "$SONY_NOTARY_PROFILE" --wait
    xcrun stapler staple "$dmg"
fi

echo "$dmg"
