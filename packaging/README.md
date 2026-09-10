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

### Application Bundle & DMG
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Generate Drag-and-Drop DMG
cpack --config build/CPackConfig.cmake -G DragNDrop -C Release

# Generate tarball
cpack --config build/CPackConfig.cmake -G TGZ -C Release
```
