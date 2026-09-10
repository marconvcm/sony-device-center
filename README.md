# Sony Device Center (Sony XM Device Bridge)

**An open-source desktop suite and C++20 SDK for Sony headphones and earbuds — Ambient Sound, Noise Cancelling, Equalizer, Clear Bass, DSEE, battery monitoring, and device control, without needing a mobile phone.**

---

## Overview

Sony locks headphone settings and telemetry behind their mobile-only apps (*Sony Headphones Connect* / *Sound Connect*).

**Sony Device Center** communicates directly with Sony headphones over Bluetooth RFCOMM using Sony's reverse-engineered binary protocol. It provides:

- **`sony-device-center`** — A modern, dark-themed **Qt 6 / QML** desktop companion application.
- **`sonyctl`** — A fast, scriptable **CLI tool** for instant terminal controls and scripting.
- **`sonyd`** — A lightweight **background daemon** managing the Bluetooth link and exposing a local IPC socket.
- **`sony-protocol` / `sony-transport` / `sony-core`** — Modular, decoupled **C++20 libraries** for integration into third-party tools and desktop environments.

---

## ✨ Features

- 🎚️ **Noise Control** — Active Noise Cancelling (ANC), Ambient Sound (levels 1–20), and Off modes.
- 🗣️ **Focus on Voice** — Toggle speech-priority voice passthrough while suppressing low-frequency noise.
- 🎛️ **Full Equalizer** — Switch between built-in presets (Bright, Excited, Vocal, Bass Boost, Treble Boost, etc.) or dial in custom 5-band frequencies and Clear Bass (-10 to +10).
- ✨ **DSEE Extreme** — Enable or disable Sony's AI-based audio upscaling for compressed audio.
- 🔋 **Live Battery & Charging State** — Real-time telemetry for over-ear models, plus individual Left, Right, and Case battery levels for True Wireless (TWS) earbuds.
- 🧩 **Advanced Audio Features** — Speak-to-Chat, Adaptive Volume, and Auto Power-Off timeouts (dynamically enabled based on device capability profiles).
- 🧬 **Dual Protocol Support** — Automatically detects and communicates with both **Protocol V1** (legacy models) and **Protocol V2** (modern models with alternating-bit Stop-and-Wait ARQ).
- 🔄 **Bidirectional Sync** — Headset hardware button presses (e.g. NC/AMB button) immediately update the daemon and UI state.
- 💻 **Flexible Architecture** — Run standalone via direct Bluetooth transport, or as a background daemon with CLI and GUI clients.

---

## 🎧 Supported Devices

| Status | Model Family | Tested & Supported Models |
| :--- | :--- | :--- |
| ✅ **Hardware Verified** | Modern V2 Over-Ear & TWS | **WH-1000XM5**, **WF-1000XM6**, WH-CH720N, Sony ULT WEAR (WH-ULT900N) |
| 🟢 **Expected / Compatible** | Modern V2 Family | WH-1000XM4 (v2 firmware), WF-1000XM5, WF-1000XM4, WH-XB910N, WH-CH520, LinkBuds S, WF-C700N |
| 🔵 **Legacy V1** | First-Generation Protocol | WH-1000XM3, WH-1000XM2, MDR-1000X, WH-XB900N, MDR-XB950BT, WI-1000X |

---

## 🚀 Building from Source

### Prerequisites

- **Compiler**: C++20 compliant compiler (`gcc` 11+, `clang` 14+, or MSVC 2022)
- **Build System**: CMake 3.16+ and Ninja or Make
- **GUI Framework**: Qt 6 (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`)
- **Bluetooth Stack**:
  - **Linux**: BlueZ (`libbluetooth-dev`, `libdbus-1-dev`)
  - **Windows**: Native Winsock & Bluetooth (`ws2_32.lib`, `bthprops.lib` included with Windows SDK)
  - **macOS**: `IOBluetooth` and `Foundation` frameworks

### Linux (Ubuntu, Debian, Fedora, Arch)

1. **Install dependencies**:
   ```bash
   # Ubuntu / Debian
   sudo apt update && sudo apt install -y \
       build-essential cmake git \
       libbluetooth-dev libdbus-1-dev \
       qt6-base-dev qt6-declarative-dev qml6-module-qtquick-controls

   # Fedora
   sudo dnf install -y \
       gcc-c++ cmake git \
       bluez-libs-devel dbus-devel \
       qt6-qtbase-devel qt6-qtdeclarative-devel
   ```

2. **Configure and compile**:
   ```bash
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel
   ```

3. **Run tests**:
   ```bash
   ctest --test-dir build --output-on-failure
   ```

### Windows

From a **Developer Command Prompt for VS 2022** (or PowerShell with MSVC):
```cmd
cmake -B build -S . -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

---

## 💻 Usage & Applications

### 1. Modern Desktop GUI (`sony-device-center`)

Launch the Qt 6 application directly:
```bash
./build/apps/device-center/sony-device-center
```
If `sonyd` is running, it connects over local IPC. If not, it opens a direct Bluetooth session to your connected headset automatically.

### 2. Command-Line Interface (`sonyctl`)

`sonyctl` provides instant control from your shell or keyboard shortcuts:

```bash
# Telemetry and status
sonyctl status                  # Connection status and model
sonyctl info                    # Full device specs, firmware, codec, and capabilities
sonyctl battery                 # Battery percentage and charging indicator
sonyctl devices                 # List discovered paired Sony devices

# Noise control
sonyctl anc on                  # Enable Noise Cancelling
sonyctl anc off                 # Turn off Noise Cancelling
sonyctl ambient 10              # Set Ambient Sound level (1 to 20)
sonyctl ambient off             # Turn off Ambient Sound

# Equalizer and sound tuning
sonyctl eq get                  # Display current preset and 5-band EQ
sonyctl eq bass-boost           # Switch to preset (bright, excited, vocal, bass-boost, etc.)
sonyctl eq custom 5 0 1 2 1 0   # Custom Clear Bass (+5) and bands: 400Hz, 1kHz, 2.5kHz, 6.3kHz, 16kHz
sonyctl dsee on                 # Enable DSEE audio upscaling
sonyctl apo 3                   # Set Auto Power-Off preset (0=Off, 1=5m, 2=15m, 3=30m, 4=1h, 5=3h)

# Standalone execution (without sonyd)
sonyctl --direct battery
sonyctl --direct anc on
```

### 3. Background Daemon (`sonyd`)

Run headless in the background or configure as a systemd user service:
```bash
# Run foreground/background
./build/apps/sonyd/sonyd &

# Run with custom socket path
./build/apps/sonyd/sonyd -s /tmp/sony-device-center.sock
```

To run as a systemd service:
```bash
cp packaging/linux/sonyd.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now sonyd.service
```

---

## 🔬 Architecture & Technical Details

### Framing & Serialization

Packets transmitted over Bluetooth RFCOMM follow the Sony MDR framed format:

```
[START 0x3E] ESCAPE( [TYPE 1B] [SEQ 1B] [LENGTH 4B Big-Endian] [PAYLOAD ...] [CHECKSUM 1B] ) [END 0x3C]
```

- **Escaping**: Special bytes `0x3C`, `0x3D`, `0x3E` are escaped with `0x3D` followed by their byte subtracted by `0x10`.
- **Alternating-Bit ARQ**: The protocol uses a 1-bit sequence counter (`0` or `1`). Incoming ACK packets specify the sequence number expected for the subsequent transmission, ensuring reliable in-order transport.
- **Service Discovery**:
  - **Protocol V1**: Service UUID `96CC203E-50F8-4944-9C22-EDCB48F6216F`
  - **Protocol V2**: Service UUID `956C7B26-B496-4447-AD7B-3A47900D8C86`

### Automated Test Suite

The test suite runs 106 Catch2 unit and integration tests without requiring Bluetooth hardware:
- Frame framing, escaping, checksums, and corruption recovery
- Fragmentation handling and multi-frame stream parsing
- V1 vs. V2 command byte layouts and safe opcode handling
- Transport simulation with fault injection, simulated timeouts, and disconnects
- IPC protocol serialization roundtrips and end-to-end Unix domain socket client/server tests

```bash
ctest --test-dir build --output-on-failure
```

---

## 📚 References & Prior Art

This project builds upon the foundational reverse-engineering work of the open-source audio community:

- **[SonyHeadphonesClient](https://github.com/Plutoberth/SonyHeadphonesClient)** by Plutoberth, Mr-M33533K5 & contributors — The original cross-platform client that decoded Sony's first-generation Bluetooth protocol.
- **[Gadgetbridge](https://codeberg.org/Freeyourgadget/Gadgetbridge)** — Open-source Android companion app providing extensive protocol specifications and reverse-engineering insights for modern Sony devices.
- **[SonyBridge](https://github.com/AmitRajput-Dev/SonyBridge)** by Amit Rajput — Early exploration into second-generation V2 command layouts.

---

## ⚠️ Disclaimer

This project is **not affiliated with, endorsed by, or connected to Sony Corporation**. All product names, logos, and brands are property of their respective owners. It is an independent, open-source clean-room implementation developed for hardware interoperability.

---

## 📄 License

Distributed under the [MIT License](LICENSE).
