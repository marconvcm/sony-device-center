# Packaging & Distribution Guide

Sony Device Center supports multi-platform packaging for Linux, macOS, and Windows.

## 1. Linux Packaging

### CPack (DEB, RPM, Tarball)
From the CMake build directory:

```bash
# Build release binaries
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Generate .deb package (Debian / Ubuntu)
cpack --config build/CPackConfig.cmake -G DEB

# Generate .rpm package (Fedora / RHEL / openSUSE)
cpack --config build/CPackConfig.cmake -G RPM

# Generate .tar.gz archive
cpack --config build/CPackConfig.cmake -G TGZ
```

### AppImage
Run the included build script with `linuxdeploy`:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

./packaging/linux/build-appimage.sh build
linuxdeploy-x86_64.AppImage --appdir build/AppDir --plugin qt --output appimage
```

### Flatpak
Build using `flatpak-builder`:
```bash
flatpak-builder --force-clean build-dir packaging/linux/com.github.sonybridge.sony-device-center.yml
flatpak-builder --run build-dir packaging/linux/com.github.sonybridge.sony-device-center.yml sony-device-center
```

### Systemd Service
To run the `sonyd` background daemon automatically at user login:
```bash
mkdir -p ~/.config/systemd/user
cp packaging/linux/sonyd.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now sonyd.service
```

---

## 2. Windows Packaging

### Portable ZIP & NSIS Installer
```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Generate portable ZIP
cpack --config build/CPackConfig.cmake -G ZIP -C Release

# Generate NSIS Installer
cpack --config build/CPackConfig.cmake -G NSIS -C Release
```

---

## 3. macOS Packaging

### Disk image
One target, one script. It runs `macdeployqt`, folds `sonyd` and `sonyctl` into the
bundle, signs, and writes the DMG with [dmgbuild](https://dmgbuild.readthedocs.io/)
(no Finder scripting, so it works on a headless runner).

```bash
pipx install dmgbuild
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build --parallel
cmake --build build --target dmg              # or: packaging/macos/build-dmg.sh build
packaging/macos/verify-dmg.sh build/*.dmg     # mounts it and checks what is inside
```

The release workflow builds it universal with `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`
on the Qt archive from `install-qt-action`; Homebrew Qt gives you the host
architecture only.

Files in `packaging/macos/`:

| File | Role |
| :--- | :--- |
| `build-dmg.sh` | The pipeline. Everything above in ~80 lines. |
| `verify-dmg.sh` | Mounts a DMG and asserts Qt, QML modules, daemon, CLI, signature. CI gate. |
| `dmgbuild.py` | Window geometry and icon positions for dmgbuild. |
| `gragen.py` | Paints `dmg-background.png` / `@2x` from the app's palette. Standard library only. |
| `Info.plist.in`, `AppIcon.icns` | Bundle metadata and icon, used by CMake. |

### Signing and notarization
Off by default. The script reads two environment variables and does the rest:

```bash
export SONY_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
export SONY_NOTARY_PROFILE="notary"    # from: xcrun notarytool store-credentials notary
cmake --build build --target dmg
```

With the identity set, the bundle is signed with hardened runtime and a secure
timestamp; with the profile set, the DMG is submitted to Apple and the ticket
stapled. To turn this on in CI, add the certificate and credentials as repository
secrets and export those two variables in the `Package` step of
`.github/workflows/release.yml`. Without them the bundle is ad-hoc signed, which
Gatekeeper blocks on first launch (right-click → Open, or
`xattr -d com.apple.quarantine "/Applications/Sony Device Center.app"`).

### Tarball
`cpack -G TGZ` still works but wraps the undeployed bundle; it is for developers, not users.
