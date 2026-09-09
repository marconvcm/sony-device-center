# Current SonyBridge architecture

## Scope and baseline

This is a source-level baseline for the incremental Device Center migration,
not a specification of every Sony device. The existing application remains the
working protocol reference. Phase 1 changes documentation only; Phase 2 adds
regression tests without moving or changing production sources. Qt, a new SDK,
transport interfaces, device profiles, and packaging are out of scope here.

The inspection covers the shared C++ sources, all three platform implementations,
SwiftUI/Objective-C++ bridge and Xcode project, CMake and workflows, resources,
font-generation support, README, and static documentation site. Bundled
third-party ImGui/stb_image and binary assets are dependencies, not new protocol
implementations.

The unmodified Linux application built successfully with GCC 13 and CMake 3.31
in Release mode, using the pinned ImGui submodule and the existing BlueZ, D-Bus,
GLFW and OpenGL dependencies. This is a compile/link check, not hardware
validation. Windows/MSVC and macOS/Xcode require their native build environments.

## Repository and dependency diagram

The build entry point is `Client/CMakeLists.txt`, not a root CMake project.
Shared implementation files are compiled directly into `SonyHeadphonesClient`.
There are no separately exported SDK libraries or Qt dependencies.

```text
Linux main / LinuxGUI                 Windows main / WindowsGUI
  GLFW + OpenGL                         Win32 + Direct3D11
             \                         /
              CrossPlatformGUI (Dear ImGui)
                |          |
                |          +---------------------+
                v                                v
             Headphones ----------------> BluetoothWrapper
                |                          |          |
                v                          v          v
          CommandSerializer <--------------+   IBluetoothConnector
                |                              /       |        \
          Constants / ByteMagic           Linux     Windows    macOS
                                           |           |         |
                                     BlueZ / D-Bus   Winsock   IOBluetooth

macOS AppDelegate / ViewController -> SwiftUI ContentView / HeadphonesModel
                                              |
                                      HeadphonesBridge (Obj-C++)
                                        |              |
                                        v              v
                                    Headphones   BluetoothWrapper
```

`CrossPlatformGUI` also owns async futures, UI-local copies of device state, and
a `TimedMessageQueue` for user-visible errors. `Constants.h` mixes protocol
constants, application naming and UI mapping arrays. `ByteMagic` mixes framing
integer conversion and Bluetooth-address conversion. These are future extraction
boundaries, not independent layers today.

## Bluetooth transports and discovery

`IBluetoothConnector` combines blocking connect/send/receive, disconnect,
connection status, discovery, and protocol-version reporting. Buffers are
`std::vector<char>` / mutable `char*`, with signed `int` byte counts. Only
`isConnected()` is required by the interface to be thread-safe.

| Platform | Transport | Discovery and generation selection |
| --- | --- | --- |
| Linux | Kernel RFCOMM socket through BlueZ/libbluetooth; authentication and encryption requested; 2.5-second receive timeout | D-Bus `ObjectManager.GetManagedObjects` enumerates `org.bluez.Device1`, then reads names/addresses. Despite the names `getConnectedDevices` and `dbus_list_adapters`, it does not filter `Connected` or `Paired`. SDP looks up the v1 UUID first, then v2 if no channel is found. |
| Windows | Winsock `AF_BTH`/RFCOMM; authentication and encryption requested; 2.5-second receive timeout | Bluetooth radio/device APIs enumerate connected devices. Connect tries the v1 service UUID, recreates the socket after failure, then tries v2. |
| macOS | `IOBluetoothRFCOMMChannel`; synchronous writes, asynchronous delegate receives queued behind a mutex/condition variable; 2.5-second receive wait | Connector enumeration returns paired devices, including disconnected ones. The UI bridge prefers a system-connected Sony-named device, otherwise opens the native picker. SDP chooses v1 first, falling back to v2 when its service record is absent. |

Evidence: `IBluetoothConnector.h`, `linux/DBusHelper.cpp:118–199`,
`linux/LinuxBluetoothConnector.cpp:33–150`,
`windows/WindowsBluetoothConnector.cpp:23–189`,
`macos/MacOSBluetoothConnector.mm:62–250`, and
`macos/HeadphonesBridge.mm:125–185` (paths below `Client/`).

Generation is selected from the service connection, not from a capability
profile or a validated model registry. The UUIDs are
`96CC203E-5068-46ad-B32D-E316F5E069BA` (v1) and
`956C7B26-D49A-4BA8-B03F-B17D393CB6E2` (v2).
Sharing a UUID does not establish feature or hardware support.

## Protocol generations and command construction

Both generations share `BluetoothWrapper`, `CommandSerializer`, `Constants`,
and `Headphones`; there are no `ProtocolV1` / `ProtocolV2` classes.

* **v1:** `serializeNcAndAsmSetting` emits the existing eight-byte NC/ambient
  layout beginning `68 02`. The legacy level range is 0–19; 0/1 select dual/single
  NC, and the disabled path uses the existing char sentinel. VPT and sound
  positioning use `48` and are sent only on the v1 `setChanges` path.
  `requestAmbientState` sends `66 02`, drains the `67` reply, and deliberately
  does not decode it into state because the reply layout is unverified.
* **v2:** NC/ambient uses a distinct seven-byte layout beginning `68 17 01`.
  `initDevice` sends `00 00`, waits for opcode `01`, but does not validate an
  eight-byte handshake response. Battery uses `22`/`23` (single, dual-earbud,
  and case subtypes); EQ uses `56`/`57`/`58`; DSEE uses `e6`/`e7`/`e8`.
  Firmware, codec, auto power-off, speak-to-chat and adaptive-volume commands
  are assembled in `Headphones`, not in a separate protocol object.

**Safety boundary:** `0x22` means POWER OFF on v1 but BATTERY request on v2.
The current protection is mostly in the UI/bridge, not the public C++ methods:
`Headphones::requestBattery()` itself does not check the protocol version.
The macOS refresh path explicitly guards v2 inquiries; the ImGui path guards
battery/EQ/DSEE and optional probing. Its initial `initDevice()` call is
unconditional, unlike the macOS bridge. Do not infer safety from method names,
and do not call v2 methods directly on v1 devices.

Evidence: `CommandSerializer.cpp:171–225`, `Headphones.cpp:78–120,245–453`,
`CrossPlatformGUI.cpp:180–245`, `macos/HeadphonesBridge.mm:195–228`.
The README contains differing model/protocol and verification statements;
these must not be treated as a hardware-validation matrix.

## Framing, checksums and stream parsing

`CommandSerializer::packageDataForBt` builds:

```text
3e ESCAPE(type | sequence | payload length, 4-byte big-endian | payload | sum) 3c
```

The checksum is the low eight bits of the sum of the **unescaped** type,
sequence, length and payload bytes, excluding delimiters and the checksum
itself. Escaping applies to the entire body, including header and checksum:
`3c -> 3d 2c`, `3d -> 3d 2d`, `3e -> 3d 2e`.
The complete escaped frame must not exceed 2048 bytes.

`unpackBtMessage` accepts an **escaped body without START/END**, not a wire
frame or receive stream. It rejects incomplete/unknown escape pairs, bodies
shorter than seven unescaped bytes, declared lengths exceeding the available
data, and checksum mismatches. It returns type, one-byte sequence and payload.
It currently accepts extra bytes after the declared payload/checksum and does
not validate the data-type enum. These are baseline limitations, not a proposed
strict codec contract.

`BluetoothWrapper::_readMessage` owns delimiter scanning and calls the body
decoder. It assembles partial reads until END and preserves bytes after that
END in `_leftoverBytes`, replaying them before receiving again. A second START
inside a frame throws `RecoverableException`. A missing START/END is not
necessarily rejected immediately: receiving continues until a connector error
or timeout. Noise before START in the same read is skipped; noise in an earlier
read can enter the accumulated body. There is no overall frame-size bound on
receive and no handling of a zero-byte socket read as EOF.

Evidence: `CommandSerializer.cpp:11–168`, `BluetoothWrapper.cpp:99–161`.

## ACKs, sequences and response matching

* Sending serializes with `_seqNumber++` under `_connectorMtx`.
* `sendCommand` reads one frame and adopts its sequence; `_waitForAck` does
  **not** check that the frame is actually an ACK.
* `sendCommandAndReadResponse` reads up to 16 frames. ACKs replace the host
  sequence. A data frame matches by first payload opcode and optionally the
  second-byte subtype, not by request sequence.
* Every received `DATA_MDR` frame is ACKed with `1 - deviceSequence`,
  including unrelated notifications, on both protocol paths.
* Unrelated frames are consumed and discarded, not dispatched to observers.
  Responses arriving after the matching response remain buffered.
* Sequence storage is an unsigned integer, serialization narrows it to one
  byte, while ACK logic expects a one-bit toggle. There is no explicit
  duplicate detection, rollover policy or retransmission queue.
* Disconnect resets sequence and leftover bytes. Connector receive timeouts
  bound individual reads, not an entire request or a continuous partial frame.

Evidence: `BluetoothWrapper.cpp:27–96,153–159`.

## Device state, capability probing and threading

`Headphones` owns current/desired `Property<T>` values for NC/ambient, voice,
VPT and positioning. `setChanges` writes to hardware then fulfills desired
values. Other settings use primitive members and immediate request/set methods.
Battery defaults to -1; dual/case levels, EQ bands, DSEE, firmware, codec and
optional-feature flags live on the same mutable object. There are no immutable
snapshots, per-feature pending/error states or subscription APIs.

Most writes lock `_propertyMtx`, but getters and parts of `isChanged` /
`setChanges` read without it. Background I/O and UI reads can overlap; waiting
for one future does not synchronize other in-flight operations. Custom EQ also
clamps bytes sent to hardware but stores the unclamped input values.

Optional discovery is already partly asynchronous: ImGui completes initial
reads before starting a background probe future; macOS publishes the initial
read completion before probing on a global queue. Probes themselves are
sequential GETs, use timeouts as absence-of-support, and swallow exceptions.
There is no model registry or persistent model/firmware cache. UI controls
combine generation checks with live `has*` flags; battery/EQ/DSEE support is
largely assumed for v2. A timeout is not proof that a feature is unsupported.

The ImGui renderer uses several independent `SingleInstanceFuture` objects
backed by `std::async(std::launch::async)`. Commands are serialized only while
holding the wrapper's I/O mutex, including its blocking reads. A slow probe
can therefore delay user commands even though rendering continues.
NC/ambient polling is frame-count based (120 frames, nominally two seconds).

macOS has a serial GCD queue for user commands, global queues for status reads,
main-queue UI completions, and a connector thread pumping an NSRunLoop for
RFCOMM callbacks. Background blocks capture raw `Headphones*` while disconnect
resets ownership; lifetime coordination needs review. The connector's delegate
close callback can call disconnect, which joins the connector thread.
The Swift model schedules periodic dynamic refreshes. There is no dedicated
shared protocol reader thread on any platform.

`RecoverableException` carries a user-facing string and a `shouldDisconnect`
flag, not a typed error code. Silent catches make timeout, malformed response
and disconnection difficult to distinguish. macOS has conditional
`SHC_DEBUG_PROTOCOL` hex logging; Linux prints connection details. There is no
shared structured logging facility.

## UI baseline and Linux-first direction

Linux uses GLFW/OpenGL and Windows uses Win32/Direct3D11 to host Dear ImGui.
The shared UI uses fixed styling, nominal 460×660 dimensions, a dark palette,
embedded Cascadia Code at a fixed 16-pixel size, and product PNGs loaded with
stb_image. It does **not** inherit the Linux desktop's font/theme preferences.
`Fonts/encodeFont.py` is a manual generation utility; the generated font data
is already committed, not generated by CMake.

macOS uses a SwiftUI `ContentView` / observable `HeadphonesModel`, hosted by
the storyboard's view controller and backed by `HeadphonesBridge`. It uses
system-font families with explicit sizes, custom dark colors, SF Symbols and
asset-catalog product images. It is not a Qt frontend.

For the later Device Center, usability and Linux desktop integration come
first: respect system fonts, scaling and accessibility settings, with clear
device status and capability-driven controls. Nothing Phone and Logitech
desktop apps are visual/interaction references, not reasons to override user
font settings. No UI redesign or Qt implementation is part of this baseline.

## Build and validation boundaries

Before Phase 2, CMake and Xcode build the application but there is no unit-test
framework or CTest target. The CMake workflow builds Linux and Windows for PRs
to main/master; the separate Xcode workflow only covers master. CMake does not
configure the macOS application backend; use the existing Xcode project there.
No repository linter configuration was found.

### Phase 2 regression boundary

`tests/CMakeLists.txt` adds Catch2/CTest and `sony-protocol-tests`. It can be
configured on its own without GUI or Bluetooth dependencies, or included from
the existing application build. It compiles the unchanged serializer,
byte-conversion helpers and wrapper directly; it does not extract libraries.
Catch2 is resolved from an installed v3 package or a version/hash-pinned source
archive. `BUILD_TESTING=OFF` leaves the application-only dependency set intact.
The existing Linux/Windows and macOS workflows now invoke the tests, without
changing their branch triggers or application build paths.

| Area | Regression coverage |
| --- | --- |
| Encoding | Literal START/END, data type, sequence, big-endian size, payload and checksum; empty ACK; size boundary including escape expansion |
| Escaping | `3c`, `3d`, `3e` literal pairs; all 256 byte values; header/checksum escaping; malformed escape pairs |
| Body decoding | Valid and invalid checksum, corrupted payload, every truncated prefix, oversized/undersized declared length, permissive trailing bytes |
| Stream assembly | Complete frame, every two-part split (including escape pairs), byte-at-a-time reads, two frames per receive, trailing partial frame, ACK plus response, invalid delimiters and body |
| Command serialization | Separate literal v1/v2 NC/ambient layouts and legacy VPT/positioning payloads |

The wrapper tests use a file-local scripted implementation of the **existing**
`IBluetoothConnector`, because delimiter parsing does not live in
`CommandSerializer`. It supplies preset reads, records writes, and throws
immediately when input runs out instead of sleeping or contacting headphones.
This is not the Phase 3 abstraction or the general-purpose Phase 4 fake.

The passing baseline contains 31 test cases (183 assertions, including
sections/generators). It passes GCC Release, standalone Clang Debug with
ASan/UBSan, and an offline standalone build using the distribution's Catch2
package. Both Linux application build modes pass; their Release executables
are byte-identical to the baseline. Native Windows/macOS builds and real
headphones were not exercised locally.

Tests labeled `baseline` deliberately record existing permissive behavior;
they are not requirements for a future strict codec. Missing delimiters
currently cause further reads, so those tests expect the scripted transport
error rather than inventing an immediate parser rejection. The tests do not
add expected-failure cases or pretend that the unsafe direct v1 battery-call
path has been fixed. That boundary remains an explicit risk below.

## Risks to preserve explicitly during migration

1. **Generation safety:** UI-only v2 gating is not an SDK safety boundary.
   The future semantic protocol layer needs a regression proving a v1 battery
   request never sends `0x22`; framing tests alone cannot guarantee this.
2. **Parser contract:** body decoding and stream assembly are separate today.
   Preserve working fragmentation/coalescing behavior and clearly label
   permissive malformed-input behavior before tightening it.
3. **Session correctness:** ACK identity, request matching, duplicate handling,
   notification loss and bounded receive/disconnect handling need dedicated tests.
4. **Concurrency/lifetime:** mixed unlocked reads, async work and disconnect
   ownership are unsafe foundations for exposing a reusable SDK.
5. **Capability uncertainty:** generation similarity and successful framing
   tests do not verify model features or firmware behavior.
6. **Transport contracts:** Linux/Windows return zero on EOF, writes may be
   partial, and macOS reports the requested write length without checking the
   OS result. Normalize these deliberately, not during codec characterization.

The next bounded refactor is an adapter around the existing connector contract
plus a deterministic test transport, with discovery kept separate. Keep the
current platform connectors and command bytes intact; extract framing only
after these baseline tests are in place.

## Phase 3 & 4: Transport Abstraction and Deterministic Fake Transport

As specified in Phase 3 and Phase 4 of the execution plan, the transport layer has been
formalized into a clean, reusable library (`libs/sony-transport`) that abstracts away
the underlying Bluetooth connections without depending on Qt or higher-level Sony protocol logic.

### 1. `ITransport` Interface

```text
       ┌───────────────────────────────┐
       │          ITransport           │
       ├───────────────────────────────┤
       │ + connect(DeviceAddress)      │
       │ + disconnect() noexcept       │
       │ + isConnected() const noexcept│
       │ + send(span<const byte>)      │
       │ + receive(span<byte>)         │
       └──────────────▲────────────────┘
                      │
       ┌──────────────┴───────────────┬───────────────────────────────┐
       │                              │                               │
┌──────────────┐       ┌─────────────────────────────┐       ┌──────────────────┐
│FakeTransport │       │ BluetoothConnectorTransport │       │ Platform Adapters│
└──────────────┘       └──────────────┬──────────────┘       └──────────────────┘
                                      │ (adapts)
                       ┌──────────────▼──────────────┐
                       │     IBluetoothConnector     │
                       │(Linux / Windows / macOS)    │
                       └─────────────────────────────┘
```

The `ITransport` interface provides:
- Clean C++20 standard library types (`std::span`, `std::byte`, `std::size_t`).
- Pure non-blocking or bounded I/O operations without platform Bluetooth headers.
- Strong typing for endpoints: `DeviceAddress` wraps device identifiers and MAC strings with comparison and conversion operators.

### 2. Separate Discovery Interface

Bluetooth device discovery is separated from transport transmission via `IDeviceDiscovery`:
- `discover()` returns `std::vector<DiscoveredDevice>`.
- `DiscoveredDevice` pairs the user-friendly device name with its strongly-typed `DeviceAddress`.

### 3. Bidirectional Connector Adapters

To preserve existing functionality without breaking legacy code:
- **`BluetoothConnectorTransport`**: Adapts any existing `IBluetoothConnector` (Linux, Windows, macOS) into an `ITransport`.
- **`TransportBluetoothConnector`**: Adapts any modern `ITransport` (including `FakeTransport`) into an `IBluetoothConnector`.
  This allows legacy consumers like `BluetoothWrapper` and `Headphones` to run directly on top of `FakeTransport` in unit and integration tests.
- Platform aliases (`LinuxBluetoothTransport`, `WindowsBluetoothTransport`, `MacOSBluetoothTransport`) are provided as explicit adapters.

### 4. Typed Sony Error Model

Introduced `SonyErrorCode` and `SonyException` (Phase 13 preview):
- Error codes: `Timeout`, `Disconnected`, `Unsupported`, `InvalidFrame`, `InvalidChecksum`, `InvalidResponse`, `TransportFailure`, `ProtocolViolation`.
- Transports throw `SonyException` with typed error codes on disconnection, timeout, or I/O failure.

### 5. Deterministic `FakeTransport`

`FakeTransport` enables complete, deterministic protocol testing without hardware:
- **Queueing incoming frames**: `queueIncoming(...)` accepts single frames, multiple frames, raw byte spans, and nested byte vectors.
- **Recording outgoing frames**: `sentFrames()`, `sentCount()`, `lastSentFrame()`, and `allSentBytes()` allow exact verification of sent packets.
- **Simulating timeouts**: Configurable timeout counts on send and receive via `simulateTimeoutOnReceive()` and `simulateTimeoutOnSend()`.
- **Simulating disconnects**: `simulateDisconnect()` sets connected status to false and triggers `SonyErrorCode::Disconnected` on I/O.
- **Simulating fragmented messages**: `setMaxReceiveChunkSize(size)` forces `receive()` to deliver data in constrained slices.
- **Multiple frames in one read**: Delivers concatenated queued frames when the receive buffer capacity allows.
- **Thread safety**: Internal mutex protection guarantees safe operation across reader/writer threads.

### 6. Test Coverage and Verification

- 13 new unit and integration tests added in `tests/transport/` (`FakeTransportTests.cpp` and `TransportAdapterTests.cpp`).
- All 44 tests pass with 100% success rate across `sony-protocol-tests` and `sony-transport-tests`.
- Both `Client/build` and root `build` configure, build, link, and test cleanly with zero warnings.

