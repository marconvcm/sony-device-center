# Remaining technical debt

Recorded after implementing IPC hardening, connection recovery, and the Qt worker
and structured-state interface. These items are intentionally left for follow-up;
no hardware verification or release publication is implied by the automated tests.

## Highest priority

1. **Hardware acceptance and protocol gaps (#8–12).** Test reconnect, physical NC
   button changes, battery refresh, and commands on XM3/XM5 hardware. V1 battery,
   NC and EQ readback is decoded and verified on an XM4; the XM3 still needs a
   re-test on the same path, and V1 DSEE / auto power-off remain unimplemented.
   XM6 EQ/read timeouts and MDR-1000X connectivity remain unresolved. Decode the
   reported captures and add literal packet fixtures before changing model claims.
   ACK receipt still cannot establish that an EQ change had an audible effect.
2. **Sanitizer validation.** The local ASan build cannot link because
   `/usr/lib64/libasan.so.8.0.0` is missing. Re-run ASan/UBSan in CI or after fixing
   the local compiler runtime. Normal builds and regression tests are separate
   evidence; they do not replace sanitizer coverage.
3. **Native platform checks.** Linux build, Qt worker tests, and the offscreen
   simulated daemon/CLI/QML smoke test pass. Windows/macOS builds and physical
   Bluetooth have not been run locally. CI now installs/requires Qt on Windows.
   Windows IPC is still unsupported; socket tests explicitly skip it.
4. **Bluetooth ownership across endpoints/processes (#29).** `--direct` refuses a
   daemon at its configured endpoint. Two different custom endpoints, a direct GUI
   already running when sonyd starts, or third-party software can still compete
   for RFCOMM. Add a per-device process lock or an ownership broker before claiming
   global exclusion.

## Service and IPC follow-up

- **Long device calls still delay queued operations.** Socket polling is independent,
  but discovery, connect, and maintenance share one serialized executor. A slow
  connection or inquiry delays subsequent status snapshots and writes. Publish a
  separate immutable service-status cache and make native connection attempts
  cancellable. Shutdown currently waits for the active native operation to return.
- **SDK concurrency contract.** Applications serialize access, but the service still
  exposes `activeDevice()` as a raw pointer and uses a coarse recursive mutex. Move
  callers to typed service operations and immutable complete snapshots before
  advertising arbitrary concurrent SDK use.
- **Client resource/failure testing.** Existing tests exercise idle, oversized,
  disappearing, malformed, and duplicate clients. Add deterministic injected partial
  writes, queue saturation, slow readers, absolute slowloris deadlines, actual
  cross-UID peer rejection, and concurrent startup stress. The bounded queue may
  return a legacy-format busy/error response to a JSON client; make all transport
  errors preserve the request's structured envelope.
- **Compatibility negotiation.** New GUI clients require version 1 structured IPC
  and tell users to upgrade old daemons; legacy CLI requests remain supported.
  Add explicit negotiation and compatibility fixtures before the next wire version.
- **Endpoint portability.** Secure paths reject symlink components, including custom
  paths through common macOS aliases. Document canonical paths in native packaging.
  Consider directory-descriptor-relative operations for stronger protection against
  path replacement in exotic, writable custom ancestor directories.
- **Discovery metadata.** Linux exposes paired/connected properties. Other platform
  adapters currently leave missing hints unknown. Improve native discovery adapters
  and bound D-Bus calls; consolidate repeated property queries into one snapshot.
- **Notification/polling efficiency.** Direct sessions consume notifications;
  daemon clients poll cached snapshots once per second. Add IPC subscriptions only
  after backpressure and per-client event limits are designed. Rotating hardware
  refreshes take longer than five seconds when requests time out.
- **Typed CLI output.** The GUI has stopped parsing prose. Legacy human-readable
  formatting still lives in IpcProtocol for compatibility; move presentation to
  sonyctl when introducing a structured CLI mode. Some optional controls exist only
  in the structured API, not the legacy CLI parser.

## UI, packaging, and maintenance

- Add interactive QML tests for switch/slider rollback, rapid actions, device
  switching, and daemon disappearance/version mismatch. Current Qt tests cover
  controller/worker behavior, and the smoke test checks QML loading.
- Improve per-feature unknown/stale/error presentation beyond the current metadata,
  battery/noise/codec labels, feature descriptions, and error footer. Show separate
  left/right/case batteries and richer read-error details where useful.
- Device actions currently allow one pending operation at a time. Coalesce slider
  drags to the final desired value rather than dropping intermediate actions while
  busy. Keep confirmed state separate from pending input.
- Wire controller error dismissal into the UI and improve lifecycle translations;
  new diagnostic/pending strings currently use English.
- Core JSON uses a vendored, pinned nlohmann/json v3.12.0 header. Document/update its
  provenance when upgrading and add an installation/export rule if SDK distribution
  is introduced. No new package-manager dependency is required for current builds.
- Split the implementation into reviewable commits/PRs before merging. The changes
  are currently local workspace edits; no PRs or releases were published.
- Release signing/attestation (#7; the macOS DMG has the hooks, see packaging/README.md), Main.qml extraction
  (#21), AUR packaging (#22), and cosmetic work remain deferred.

## Local test instructions

Build and test:

```sh
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

For a real-device test, stop the installed daemon first to avoid mixing versions,
then start the freshly built daemon and GUI in separate terminals:

```sh
systemctl --user stop sonyd.service
./build/apps/sonyd/sonyd -v
```

```sh
./build/apps/device-center/sony-device-center
./build/apps/sonyctl/sonyctl status
```

Start with headphones off, turn them on, then test power cycling and the physical
NC/ambient button. Check that failures are visible and the UI stays responsive.
For hardware-free testing use `sonyd --simulated` instead of `sonyd -v`.
The socket path now requires a private directory; the default handles this.
