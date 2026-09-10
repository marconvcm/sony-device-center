# Sony Device Center — Execution Plan

## Objective

Evolve the existing `AmitRajput-Dev/SonyBridge` project into a robust, reusable cross-platform Sony audio-device SDK and desktop Device Center.

The existing SonyBridge implementation must be treated as a working protocol reference and starting point.

Do **not** rewrite the project from scratch.

The primary objective is to progressively extract and harden the existing Sony protocol implementation while preserving current behavior.

Target stack:

- C++20
- CMake
- Qt 6
- QML (Visual UI reference frames located in `.frames/`)
- Linux / BlueZ
- Windows Bluetooth RFCOMM
- macOS Bluetooth transport
- Unit and protocol-level tests
- Optional CLI for hardware validation

The architecture must eventually support:

- WH-1000XM3
- WH-1000XM4
- WH-1000XM5
- WH-1000XM6
- WF-1000XM4
- WF-1000XM5
- ULT Wear
- WH-CH720N
- LinkBuds
- future Sony devices

Do not assume all v1 or v2 devices have identical capabilities.

---

# Critical Rules

## 1. Preserve existing behavior

The Sony protocol is reverse-engineered and mistakes can cause unexpected hardware behavior.

Never casually modify protocol bytes.

In particular:

`0x22`

has different meanings depending on protocol generation:

- Sony protocol v1: POWER OFF
- Sony protocol v2: BATTERY request

There must be regression tests preventing v2 commands from being sent to v1 devices.

---

## 2. Do not perform a large rewrite

Refactor incrementally.

Every milestone must compile before continuing.

Existing functionality must continue working during the migration.

---

## 3. Add tests before major protocol refactoring

Before reorganizing protocol code, capture the current behavior in automated tests.

Use fake/in-memory transports where hardware is not required.

---

# Target Architecture

Restructure the repository toward:

```text
sony-device-center/
│
├── libs/
│
│   ├── sony-protocol/
│   │   ├── include/
│   │   └── src/
│   │
│   ├── sony-transport/
│   │   ├── include/
│   │   └── src/
│   │
│   └── sony-core/
│       ├── include/
│       └── src/
│
├── apps/
│   ├── sonyd/
│   ├── sonyctl/
│   └── device-center/
│
├── tests/
│   ├── protocol/
│   ├── core/
│   └── transport/
│
├── docs/
│
└── CMakeLists.txt
```

Do not move everything at once.

Migrate incrementally.

---

# Architecture Layers

The final dependency direction must be:

```text
Qt / QML UI
      ↓
Sony Core
      ↓
Sony Protocol
      ↓
Transport abstraction
      ↓
BlueZ / Windows / macOS
```

The protocol and core libraries must not depend on Qt.

---

# Phase 1 — Establish Baseline

Create branch:

```text
refactor/device-center-core
```

Inspect the complete existing repository before modifying code.

Document:

- current Bluetooth transports
- current v1 protocol implementation
- current v2 protocol implementation
- command framing
- ACK handling
- sequence handling
- device-state handling
- capability probing
- UI dependencies
- threading model

Create:

```text
docs/architecture-current.md
```

Include a dependency diagram.

Do not change runtime behavior during this phase.

Commit:

```text
docs: document current SonyBridge architecture
```

---

# Phase 2 — Introduce Test Infrastructure

Add a C++ testing framework.

Prefer:

```text
Catch2
```

or GoogleTest if already easier to integrate.

Add targets:

```text
sonybridge-tests
```

or preferably:

```text
sony-protocol-tests
sony-core-tests
```

Start with tests for `CommandSerializer`.

Required tests:

### Frame encoding

Verify:

- START marker
- END marker
- DATA_TYPE
- sequence number
- big-endian size
- payload
- checksum

### Escaping

Test payloads containing:

```text
0x3c
0x3d
0x3e
```

Verify escape and unescape round trips.

### Checksum

Test:

- valid checksum
- invalid checksum
- corrupted payload
- truncated frame

### Frame parsing

Test:

- one complete frame
- partial frame
- two frames in one receive buffer
- trailing bytes
- invalid START/END
- invalid declared length

Commit:

```text
test: add protocol framing regression coverage
```

---

# Phase 3 — Create Transport Interface

Rename or evolve:

```text
IBluetoothConnector
```

toward a more generic:

```cpp
class ITransport
{
public:
    virtual ~ITransport() = default;

    virtual void connect(const DeviceAddress&) = 0;

    virtual void disconnect() noexcept = 0;

    virtual bool isConnected() const noexcept = 0;

    virtual size_t send(std::span<const std::byte>) = 0;

    virtual size_t receive(std::span<std::byte>) = 0;
};
```

Bluetooth-specific device discovery should live in a separate interface.

Example:

```cpp
class IDeviceDiscovery
{
public:
    virtual std::vector<DiscoveredDevice> discover() = 0;
};
```

Keep adapters for:

```text
LinuxBluetoothTransport
WindowsBluetoothTransport
MacOSBluetoothTransport
```

Do not break the existing Bluetooth implementations yet.

Commit:

```text
refactor: introduce transport abstraction
```

---

# Phase 4 — Create Fake Transport

Implement:

```text
FakeTransport
```

for tests.

It must support:

- queueing incoming frames
- recording outgoing frames
- simulating timeouts
- simulating disconnects
- simulating fragmented messages
- returning multiple frames in one read

Example API:

```cpp
FakeTransport transport;

transport.queueIncoming(...);

session.send(...);

EXPECT_EQ(
    transport.sentFrames(),
    expectedFrames
);
```

This layer will allow protocol development without real headphones.

Commit:

```text
test: add deterministic fake transport
```

---

# Phase 5 — Extract Frame Codec

Move framing responsibilities from `CommandSerializer` into:

```text
sony-protocol/FrameCodec
```

Example:

```cpp
struct SonyFrame
{
    DataType type;
    uint8_t sequence;
    std::vector<uint8_t> payload;
};
```

API:

```cpp
class FrameCodec
{
public:
    static std::vector<uint8_t> encode(const SonyFrame&);

    static SonyFrame decode(std::span<const uint8_t>);
};
```

The codec must know nothing about:

- ANC
- battery
- EQ
- Sony devices
- Bluetooth

Only Sony framing.

Keep all existing regression tests passing.

Commit:

```text
refactor: extract Sony frame codec
```

---

# Phase 5.1 — Background Daemon & IPC Architecture (`sonyd`)

Transform the backend connection manager into a background daemon service (`sonyd`) with an IPC interface to support fast, non-blocking CLI invocations (`sonyctl`) and desktop UI connections.

## Motivation

Bluetooth RFCOMM only permits a single active host connection to the headphones at any time. Without a daemon service:
- Running sequential CLI commands like `sonyctl anc on` or `sonyctl ambient 10` would force each invocation to discover devices, open RFCOMM, perform SDP negotiation, complete the protocol handshake, send the command, and disconnect (incurring multi-second latency on every call).
- CLI commands and GUI clients could not run concurrently.
- Unsolicited notifications (e.g., headset button presses, ambient changes, battery notifications) would be lost whenever no client process is actively running.

## Architecture & Responsibilities

1. **`sonyd` (Background Daemon)**:
   - Owns and maintains the persistent `ITransport` connection to the connected Sony device.
   - Maintains live `DeviceState` snapshots.
   - Continuously receives unsolicited notifications and updates cached state.
   - Hosts a low-latency local IPC server:
     - **Linux / macOS**: Unix Domain Socket (e.g. `$XDG_RUNTIME_DIR/sony-device-center.sock` or `~/.cache/sony-device-center.sock`).
     - **Windows**: Named Pipe (e.g. `\\.\pipe\sony-device-center`).
   - Supports auto-launch: if `sonyctl` or the GUI is invoked when `sonyd` is not running, it can automatically start in the background (or run via systemd user unit / launchd / Windows service).

2. **IPC Protocol**:
   - Lightweight JSON-RPC or binary command protocol over the local IPC channel.
   - Commands:
     - `get_state` (instant response from cached snapshot)
     - `set_noise_control` (ANC, Ambient, Off, Ambient Level)
     - `set_equalizer` (Preset, Manual bands, Clear Bass)
     - `set_dsee`
     - `subscribe_events` (stream state updates and unsolicited notifications)
   - Fast, low-latency execution (< 10ms for cached queries or direct command transmission).

3. **CLI Usage (`sonyctl`) via Daemon IPC**:
   ```bash
   sonyctl anc on
   sonyctl ambient 10
   sonyctl eq bass-boost
   sonyctl dsee auto
   ```
   If the daemon is active, `sonyctl` dispatches commands immediately via IPC without reconnecting Bluetooth.
   If the daemon is not running, `sonyctl` either auto-spawns `sonyd` or connects directly.

Commit:
```text
feat: introduce background daemon and IPC architecture for sonyctl
```

---

# Phase 6 — Introduce Sony Protocol Session

Replace the synchronous responsibilities currently concentrated in `BluetoothWrapper` with:

```text
SonyProtocolSession
```

Responsibilities:

```text
sequence management
ACK management
frame parsing
request/response matching
notification dispatch
timeouts
transport ownership
```

Target model:

```text
                  SonyProtocolSession
                         │
             ┌───────────┴───────────┐
             │                       │
        Writer Queue             Reader Loop
             │                       │
             └────────── Transport ──┘
                                     │
                                FrameCodec
```

The session must support unsolicited frames.

Do not discard unrelated messages.

Instead:

```cpp
session.onNotification(
    [](const SonyFrame& frame) {
        ...
    }
);
```

Required tests:

- ACK before response
- response before unrelated notification
- notification between ACK and response
- multiple notifications
- timeout
- disconnect during request
- duplicate frame
- invalid frame
- sequence rollover

Commit:

```text
refactor: introduce protocol session and message dispatcher
```

---

# Phase 7 — Separate Protocol V1 and V2

Create:

```text
ProtocolV1
ProtocolV2
```

Do not expose raw opcodes to the application.

Instead expose semantic commands.

Example:

```cpp
protocol.getBattery();
protocol.setNoiseControl(...);
protocol.getEqualizer();
```

Internally:

```text
ProtocolV1
    ↓
v1 opcodes

ProtocolV2
    ↓
v2 opcodes
```

No UI code may manually check raw Sony opcodes.

Add an explicit regression test:

```text
Given a ProtocolV1 session
When battery state is requested
Then opcode 0x22 must NOT be sent
```

And:

```text
Given ProtocolV2
Battery request may use opcode 0x22
```

Commit:

```text
refactor: isolate Sony protocol generations
```

---

# Phase 8 — Introduce Device Profiles

Create:

```cpp
enum class SonyModel
{
    Unknown,

    WH1000XM3,
    WH1000XM4,
    WH1000XM5,
    WH1000XM6,

    WF1000XM4,
    WF1000XM5,

    WHCH720N,
    ULTWear,
    LinkBudsS
};
```

Create:

```cpp
struct DeviceCapabilities
{
    bool battery = false;
    bool dualBattery = false;

    bool noiseCancelling = false;
    bool ambientSound = false;
    bool focusOnVoice = false;

    bool equalizer = false;
    bool clearBass = false;

    bool dsee = false;

    bool speakToChat = false;
    bool adaptiveVolume = false;

    bool autoPowerOff = false;

    bool firmwareInfo = false;
    bool codecInfo = false;

    bool wearSensor = false;

    bool multipoint = false;
};
```

Create:

```cpp
struct DeviceProfile
{
    SonyModel model;
    SonyProtocolVersion protocol;
    DeviceCapabilities capabilities;
};
```

Known devices should receive capabilities immediately.

Do not perform slow capability probes for features already known.

Unknown devices may still use probing.

Commit:

```text
feat: add Sony device profile registry
```

---

# Phase 9 — Improve Capability Discovery

Replace:

```text
probe every command and wait for timeout
```

with:

```text
known device
    → profile registry

unknown device
    → capability discovery
    → cache result
```

Capability discovery must run asynchronously.

The UI must become usable before optional probes finish.

Add persistent capability cache keyed by:

```text
device model
firmware version
```

Optional later improvement:

```text
Bluetooth address + model + firmware
```

Commit:

```text
perf: replace blocking capability probing
```

---

# Phase 10 — Device State Model

Replace scattered primitive fields in `Headphones` with explicit state structures.

Example:

```cpp
struct BatteryState
{
    std::optional<int> main;
    std::optional<int> left;
    std::optional<int> right;
    std::optional<int> caseBattery;
    bool charging = false;
};
```

```cpp
enum class NoiseControlMode
{
    Off,
    NoiseCancelling,
    Ambient
};
```

```cpp
struct NoiseControlState
{
    NoiseControlMode mode;
    int ambientLevel;
    bool focusOnVoice;
};
```

```cpp
struct EqualizerState
{
    int preset;
    int clearBass;
    std::array<int, 5> bands;
};
```

Create:

```cpp
struct DeviceState
{
    BatteryState battery;
    NoiseControlState noiseControl;
    EqualizerState equalizer;

    bool dsee;

    std::string firmware;
    std::string codec;
};
```

The public state exposed to applications should be immutable snapshots.

Commit:

```text
refactor: introduce immutable device state model
```

---

# Phase 11 — Thread Safety

Audit every current getter/setter.

Remove unsafe patterns where writes use mutexes but reads do not.

Prefer:

```text
single protocol worker thread
+
immutable DeviceState snapshots
+
event notifications
```

Avoid exposing mutable internal state.

Use:

```cpp
std::shared_ptr<const DeviceState>
```

or a properly synchronized state store.

Add ThreadSanitizer configuration when supported.

Commit:

```text
fix: harden device state concurrency
```

---

# Phase 12 — Event-Driven Updates

Remove unnecessary polling where Sony unsolicited notifications can provide state changes.

Create event types:

```cpp
struct BatteryChanged {};
struct NoiseControlChanged {};
struct EqualizerChanged {};
struct ConnectionChanged {};
struct DeviceStateChanged {};
```

Support:

```cpp
device.onStateChanged(...);
```

Polling may remain as a fallback for devices or commands where notifications are unavailable.

Do not assume all Sony devices emit all notifications.

Commit:

```text
feat: add device event dispatcher
```

---

# Phase 13 — Error Model

Replace silent patterns like:

```cpp
catch (...) {}
```

with typed errors.

Create:

```cpp
enum class SonyErrorCode
{
    Timeout,
    Disconnected,
    Unsupported,
    InvalidFrame,
    InvalidChecksum,
    InvalidResponse,
    TransportFailure,
    ProtocolViolation
};
```

Create:

```cpp
class SonyException : public std::runtime_error
{
public:
    SonyErrorCode code() const noexcept;
};
```

Capability probing may intentionally suppress `Unsupported` and `Timeout`, but it should log them.

Commit:

```text
refactor: introduce typed Sony errors
```

---

# Phase 14 — Logging

Add structured logging.

Suggested categories:

```text
sony.transport
sony.protocol
sony.session
sony.device
sony.capabilities
sony.state
```

Add developer logging mode capable of showing:

```text
TX  3e ...
RX  3e ...

ACK seq=1

BATTERY_GET
BATTERY_RET level=87

NCASM_SET mode=Ambient level=10
```

Never log unnecessary personal information.

Commit:

```text
feat: add protocol diagnostics logging
```

---

# Phase 15 — Extract Libraries

CMake should eventually provide:

```cmake
add_library(sony-protocol)
add_library(sony-transport)
add_library(sony-core)
```

Expected dependency graph:

```text
sony-core
   ↓
sony-protocol
   ↓
sony-transport
```

Applications:

```cmake
add_executable(sonyd)
add_executable(sonyctl)
add_executable(sony-device-center)
```

Commit:

```text
build: split SonyBridge into reusable libraries
```

---

# Phase 16 — Build sonyctl

Before creating the Qt UI, build:

```text
sonyctl
```

Commands:

```bash
sonyctl devices
```

```bash
sonyctl info
```

```bash
sonyctl battery
```

```bash
sonyctl anc on
```

```bash
sonyctl ambient 10
```

```bash
sonyctl ambient off
```

```bash
sonyctl eq get
```

```bash
sonyctl eq preset bright
```

```bash
sonyctl dsee on
```

Example output:

```text
Sony WH-1000XM5

Protocol: v2
Firmware: 2.3.1
Codec: LDAC
Battery: 87%

Noise Control:
  Noise Cancelling

Capabilities:
  ANC
  Ambient Sound
  Equalizer
  Clear Bass
  DSEE
  Speak-to-Chat
```

This tool should become the primary hardware-validation tool.

Commit:

```text
feat: add sonyctl diagnostic CLI
```

---

# Phase 17 — Hardware Validation Matrix

Create:

```text
docs/device-matrix.md
```

Columns:

```text
Device
Protocol
Connection
Battery
ANC
Ambient
EQ
DSEE
Firmware
Codec
Speak-to-Chat
Auto Power-Off
Tested Firmware
Tester
```

Never mark devices as fully supported based only on protocol similarity.

Use:

```text
Verified
Partially Verified
Expected
Unknown
```

---

# Phase 18 — Qt 6 / QML Device Center

Only start this after `sonyctl` and the core libraries are stable.

Create:

```text
apps/device-center
```

Use:

```text
Qt 6
Qt Quick
QML
```

The Qt application must communicate only with `sony-core`.

No raw Bluetooth code in QML.

No Sony protocol bytes in QML.

Suggested UI:

```text
┌─────────────────────────────────────────┐
│ Sony Device Center                      │
├───────────────┬─────────────────────────┤
│ Devices       │ WH-1000XM5              │
│               │                         │
│ WH-1000XM5    │ Battery      87%        │
│               │                         │
│               │ Noise Control           │
│               │                         │
│               │ [ Noise Cancelling ]    │
│               │ [ Ambient ]             │
│               │ [ Off ]                 │
│               │                         │
│               │ Ambient Level           │
│               │ ─────────●────          │
│               │                         │
│               │ Equalizer               │
│               │                         │
└───────────────┴─────────────────────────┘
```

Views should be capability-driven.

Example:

```qml
Visible {
    visible: device.capabilities.speakToChat
}
```

Do not hard-code UI based solely on model names.

## UI Design & Interaction Reference (`.frames/`)

Visual reference frames demonstrating how the UI should look, feel, and function are located under the `.frames/` directory:

```text
.frames/
├── logitech_options_video_0pct.jpg
├── logitech_options_video_5pct.jpg
...
└── logitech_options_video_95pct.jpg
```

Use these frames as the primary design guide for the modern Device Center interface:

1. **Modern Companion App Layout**:
   - Clean, dark-mode first design inspired by modern desktop device companions (Logitech Options+ style).
   - Prominent central device hero visualization showcasing product renders with high aesthetic fidelity.
   - Elegant peripheral layout: live battery levels, connection status, and model badge integrated cleanly around the device graphic.

2. **Control & Feature Presentation**:
   - Card-based modular controls for Noise Control (ANC / Ambient Sound / Off), Ambient Level slider, Equalizer presets/bands, DSEE, and feature toggles.
   - Smooth animated transitions when switching views, adjusting sliders, or toggling features.
   - Non-blocking responsive interaction with subtle hover states and fluid feedback.

3. **Multi-Device & Connection Flow**:
   - Seamless switcher for multiple paired Sony devices as demonstrated in the reference sequence.
   - Polished empty, disconnected, and connecting states with clear visual indicators.

Commit:

```text
feat: introduce Qt QML Sony Device Center
```

---

# Phase 19 — Packaging

Target:

Linux:

```text
AppImage
Flatpak
RPM
DEB
```

Windows:

```text
MSI
portable ZIP
```

macOS:

```text
.app
DMG
```

Later:

```text
Homebrew
Winget
Flathub
```

---

# Phase 20 — CI

CI must build:

```text
Ubuntu
Windows
macOS
```

Run:

```text
unit tests
protocol tests
core tests
```

Prefer sanitizers on Linux:

```text
ASan
UBSan
TSan where practical
```

Do not require real headphones for CI.

Hardware tests remain manual.

---

# PR Strategy

Do not produce one enormous PR.

Create small, reviewable PRs approximately in this sequence:

```text
PR 1
Protocol regression tests

PR 2
Transport abstraction + FakeTransport

PR 3
FrameCodec

PR 4
SonyProtocolSession

PR 5
ProtocolV1 / ProtocolV2 separation

PR 6
DeviceProfile + Capabilities

PR 7
DeviceState + threading cleanup

PR 8
Event dispatcher

PR 9
sonyctl

PR 10
Qt/QML application skeleton
```

Each PR must:

- compile on all supported platforms
- keep existing features working
- contain tests where applicable
- document architectural changes
- avoid unrelated changes

---

# Definition of Done — Core Architecture

The core refactoring is complete when this is possible:

```cpp
auto devices = SonyDeviceManager::discover();

auto device = SonyDeviceManager::connect(devices.front());

std::cout << device->info().model << '\n';

device->setNoiseControl(
    NoiseControlMode::NoiseCancelling
);

device->setAmbientLevel(10);

device->onStateChanged(
    [](const DeviceState& state)
    {
        std::cout
            << "Battery: "
            << state.battery.main.value_or(-1)
            << "%\n";
    }
);
```

No application should need to know:

```text
Sony opcodes
frame layouts
RFCOMM channels
v1/v2 command bytes
ACK sequence logic
```

Those details belong entirely inside the SDK.

---

# First Task

Start only with Phase 1 and Phase 2.

Do not start implementing Qt.

Do not reorganize the entire repository yet.

First:

1. inspect the repository
2. document the current architecture
3. introduce tests for framing and serialization
4. verify the current application still builds

After completing those tasks, report:

- files added
- files changed
- tests added
- architectural risks discovered
- recommended next refactor

Then stop before beginning Phase 3.