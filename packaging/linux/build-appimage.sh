#!/usr/bin/env bash
set -euo pipefail

# AppImage builder script for Sony Device Center using linuxdeploy
BUILD_DIR="${1:-build}"
APP_DIR="${BUILD_DIR}/AppDir"

echo "==> Preparing AppDir at ${APP_DIR}..."
rm -rf "${APP_DIR}"
mkdir -p "${APP_DIR}/usr/bin"
mkdir -p "${APP_DIR}/usr/share/applications"
mkdir -p "${APP_DIR}/usr/share/icons/hicolor/scalable/apps"
mkdir -p "${APP_DIR}/usr/share/icons/hicolor/512x512/apps"
mkdir -p "${APP_DIR}/usr/share/metainfo"

echo "==> Installing binaries and assets..."
cp "${BUILD_DIR}/apps/device-center/sony-device-center" "${APP_DIR}/usr/bin/"
cp "${BUILD_DIR}/apps/sonyd/sonyd" "${APP_DIR}/usr/bin/"
cp "${BUILD_DIR}/apps/sonyctl/sonyctl" "${APP_DIR}/usr/bin/"
cp packaging/linux/sony-device-center.desktop "${APP_DIR}/usr/share/applications/"
cp packaging/linux/sony-device-center.svg "${APP_DIR}/usr/share/icons/hicolor/scalable/apps/"
cp packaging/linux/sony-device-center.png "${APP_DIR}/usr/share/icons/hicolor/512x512/apps/"
cp packaging/linux/com.github.sonybridge.sony-device-center.metainfo.xml "${APP_DIR}/usr/share/metainfo/"

# Root symlinks for AppImage runtime
ln -sf usr/share/icons/hicolor/scalable/apps/sony-device-center.svg "${APP_DIR}/sony-device-center.svg"
ln -sf usr/share/applications/sony-device-center.desktop "${APP_DIR}/sony-device-center.desktop"
ln -sf usr/bin/sony-device-center "${APP_DIR}/AppRun"

echo "==> AppDir prepared successfully."
echo "To package with linuxdeploy:"
echo "  linuxdeploy-x86_64.AppImage --appdir \"${APP_DIR}\" --plugin qt --output appimage"
