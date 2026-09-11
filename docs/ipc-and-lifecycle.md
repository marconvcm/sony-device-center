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
the V2 battery opcode, and V1's undecoded noise-control readback stays unknown until
a successful write. Protocol similarity is not hardware verification.

`sonyctl --direct` refuses to run while a daemon is reachable at the selected socket.
This also applies to a disconnected daemon that may be reconnecting. Stop the daemon
before starting a direct session. Separate custom endpoints and third-party Bluetooth
clients are not coordinated by this check.

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
