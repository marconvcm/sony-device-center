# IPC and connection lifecycle

The current applications use `sony-transport` for platform Bluetooth,
`sony-protocol` for framing/session/commands, and `sony-core` for device state,
connection policy, and IPC. The legacy ImGui and Swift clients remain separate.

## Ownership and recovery

`sonyd` acquires its IPC endpoint before making Bluetooth connections. A socket
polling thread handles at most 16 clients; a separate executor serializes commands
and maintenance. Commands waiting for execution are bounded to 16. Completed
requests close their connection, matching the existing CLI client. Pipelined
requests are rejected. Individual hardware operations can delay later commands;
they do not block socket acceptance or the GUI thread.

`DeviceService::startAutoConnect()` enables discovery and retries. Its `tick()`
method runs on the owning executor, using an injectable steady clock. Discovery
prefers system-connected devices, then stable address ordering. An explicit address
restricts attempts to that device. Failures retry after 1, 2, 4, 8, 16, then 30
seconds. Success resets the delay. Manual disconnect suspends retries; connect
resumes them. EOF/fatal session errors mark the session disconnected; an ordinary
receive timeout does not. The owner joins the reader before reconnecting.

Known connected sessions rotate settings reads in separate maintenance steps,
nominally within five seconds when the device answers promptly. Battery refreshes
every 30 seconds. Unsupported profile features are not probed. Timeouts extend
these intervals rather than accumulating overlapping requests. V1 does not send
the V2 battery opcode; it reads battery, noise control and equalizer through its
own legacy opcodes. Protocol similarity is not hardware verification.

`sonyctl --direct` refuses to run while a daemon is reachable at the selected socket.
This also applies to a disconnected daemon that may be reconnecting. Stop the daemon
before starting a direct session. Separate custom endpoints and third-party Bluetooth
clients are not coordinated by this check.

## Linux control socket safety

The native RFCOMM connector closes its previous socket before reconnecting and
releases sockets after setup failures. Disconnect is idempotent. Sockets use
close-on-exec, authenticated/encrypted RFCOMM, and 2.5-second send/receive timeouts.
Interrupted reads/writes retry; receive timeouts remain recoverable. Writes use
`MSG_NOSIGNAL` so a closed control channel cannot terminate the process via SIGPIPE.
These changes affect the Sony control channel, not BlueZ audio profiles or PipeWire.
SDP discovery and connection setup are still synchronous; the I/O timeouts do not
provide a deadline for the entire connection attempt. Hardware playback continuity
and reconnect behavior require a physical headset test. IPC accept retries back off
for 250 ms after descriptor exhaustion (`EMFILE`/`ENFILE`), continuing to service
existing clients and shutdown without spinning. Discovery checks pairing before
reading optional device names.

## Endpoint protection

The default Unix socket is `$XDG_RUNTIME_DIR/sony-device-center.sock`. If that variable
is unset, Linux uses `/tmp/sony-device-center-<uid>/sony-device-center.sock`; macOS uses
`/private/tmp/sony-device-center-<uid>/sony-device-center.sock`.

The immediate parent must be a real, user-owned directory with mode `0700`.
Symlink components, foreign endpoints, non-socket files, and overlong paths are
rejected. A missing final directory can be created by the server. The socket is
`0600`, and both peers verify their effective user IDs. Custom `--socket` paths
follow the same rules. An invalid configured runtime directory fails explicitly
rather than selecting a less protected location.

A persistent sibling `.lock` file prevents duplicate daemons and concurrent stale
socket reclamation. It is deliberately retained after shutdown. Startup also
checks for older listeners that do not take the lock. Shutdown only removes the
socket inode created by that server.

Limits: 16 KiB request lines; 256 KiB responses; five-second absolute read/write
deadlines; 30-second queued-command deadline. JSON nesting is limited to 32.
Broken pipes and partial writes are handled without process-wide signal changes.
Windows IPC remains unsupported and Windows applications use direct Bluetooth.

## Structured protocol, version 1

Requests and responses are one UTF-8 JSON object per line. JSON is handled by the
vendored MIT-licensed nlohmann/json v3.12.0 header, without a Qt dependency in core.
The existing textual CLI commands and response encoding remain supported.

```json
{"version":1,"id":42,"method":"ambient","params":{"level":12,"focusOnVoice":true}}
```

Responses echo the ID and version. Success returns `{"ok":true,"data":...}`;
failure returns `{"ok":false,"error":{"code":"...","message":"..."}}` alongside
those fields. IDs are integers or strings. Unsupported versions return
`VersionMismatch`. Syntax/type/range failures return `InvalidRequest`; device
errors preserve `SonyErrorCode` names. No automatic retry of a failed write occurs.

| Method | Parameters | Result |
| --- | --- | --- |
| `snapshot` | none | Complete cached state |
| `devices` | none | Discovery list with nullable paired/systemConnected hints |
| `connect` | address, optional name | State after connection |
| `disconnect` | none | State after explicit disconnect |
| `anc`, `dsee`, `speakToChat`, `adaptiveVolume` | enabled: boolean | State after acknowledged write |
| `ambient` | level: 1–20, optional focusOnVoice | State after acknowledged write |
| `eqPreset` | preset: supported numeric code | State after acknowledged write |
| `eqCustom` | clearBass: −10–10, bands: five integers −10–10 | State after acknowledged write |
| `autoPowerOff` | index: 0–5 | State after acknowledged write |

Snapshots include connection state, selected address/model, profile capabilities,
battery main/left/right/case and charging, noise mode/level/voice focus, EQ preset and
bands, audio settings, codec, and firmware. Per-feature metadata records
`availability` (`unknown`, `unsupported`, `valid`, `stale`), `lastSuccessMs` (Unix
milliseconds, zero if never confirmed), and `error`. Missing battery values are
JSON null. A transport ACK confirms acceptance, not an independently verified
audio effect; model-specific protocol issues must still be tested on hardware.

## Qt worker and presentation

`DeviceBackend` owns IPC or direct service access on a dedicated QThread.
`DeviceCenterController` contains GUI-thread caches only. Startup renders
immediately. Direct notifications are coalesced and queued to the worker; daemon
snapshots are polled once per second. State locks are released before event
listeners run. Shutdown removes subscriptions and joins the reader and worker.

Device actions show pending state and keep their previous confirmed value until
success. Errors remain visible until another action or explicit dismissal.
Generation numbers discard obsolete queued snapshots. Unknown battery is shown
as a dash/Unknown; unknown noise mode is not rendered as Off; codec is never guessed.
An unavailable or incompatible daemon does not trigger a competing direct session.

## Validation

Catch2 tests cover endpoint protection, malformed/idle clients, concurrent client
service, reconnect timing/selection, JSON escaping and versioning, and V1 safety.
Qt tests inject services to cover slow I/O, failed actions, notifications, and
shutdown. A Linux offscreen smoke test starts the simulated daemon, exercises the
actual CLI and structured socket, checks the direct-mode guard, and loads Main.qml.
CI requires Qt on Linux and Windows; Unix socket tests explicitly skip Windows.

Run `cmake --build build --parallel` followed by
`ctest --test-dir build --output-on-failure`. Unix integration tests need permission
to bind local sockets. Physical-headset, native Windows, and native macOS validation
must be recorded separately; the simulator does not establish model compatibility.

## macOS RFCOMM ownership and cancellation

Each native session owns its device, channel, delegate and run loop on one
thread. Opening uses `openRFCOMMChannelAsync` directly, with a ten-second
application deadline and success gated by `rfcommChannelOpenComplete`. Cached
Sony V1/V2 service records are required; an absent service fails explicitly and
should be investigated by pairing/reconnecting in Bluetooth settings. Discovery
still preserves Sony OUI/name filtering and paired audio/BLE entries.

Writes use `writeAsync`, respect the channel MTU and wait at most 2.5 seconds for
native completion. Immediate errors, completion errors, remote close and timeout
are propagated. The native run loop is serviced continuously rather than once
per second. Callbacks only touch shared session state; they never join a worker
or call a connector that may have been destroyed. Cancellation wakes pending
open, write and receive waits. Teardown detaches the delegate and closes the
channel on its owner thread, with native callback objects retained across a main
run-loop turn to tolerate callbacks already queued by IOBluetooth.
The headless macOS daemon also services its main run loop so deferred native
releases are drained after reconnects instead of accumulating for its lifetime.

The GUI publishes startup before maintenance starts and requests cancellation
before joining its backend at shutdown. The daemon and protocol session also
request cancellation before joining their workers/readers. Cancellation is a
thread-safe request, not cross-thread native resource destruction. Other native
transports retain their existing connection behavior.

## WF-1000XM4 noise control and batteries

The WF-1000XM4 profile selects NCASM subtype `0x15`, including its wind-control
byte, for both GET and SET. Before changing ANC/Ambient/Off the implementation
reads and preserves that byte; malformed or missing readback prevents the write.
Notifications use the same decoder as queried state. The WH-1000XM6 keeps subtype
`0x19` readback and `0x17` writes; other profiles retain their existing layouts.

Only the WF-1000XM4 opts into collecting left/right (`22 09`) and case (`22 0a`)
battery records even after an aggregate (`22 00`) reply succeeds. A missing case
reply does not discard valid earbud readings. Individual values are exposed in
structured IPC, CLI `battery`/`info`, and the GUI overview, where unknown/stale
values are hidden or shown as unknown. No IPC version change is required: these
battery fields already existed in structured snapshots.

Regression tests use the firmware 2.3.1 packet layouts reported in issue #56.
Native macOS tests use injected device/channel doubles, not a physical headset.
Hardware verification is still required for the original crash/stall reports,
WF-1000XM4 audible behavior and preservation of non-default wind settings.
