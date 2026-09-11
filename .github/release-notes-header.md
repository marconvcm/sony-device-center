Sony Device Center is an independent open-source project, free for anyone to use,
study and modify. It is **not affiliated with, endorsed by, or connected to Sony
Corporation**. Sony and all product names are trademarks of their respective owners.
It talks to the headphones over a reverse-engineered protocol, for interoperability.

## Contents

- **`sony-device-center`** — Qt 6 / QML desktop app
- **`sonyd`** — background daemon with a local IPC socket
- **`sonyctl`** — scriptable CLI
- **`sony-core` / `sony-protocol` / `sony-transport`** — C++20 libraries

## Install

```bash
# Fedora / RHEL (dnf installs the RPM)
sudo dnf install ./sony-device-center-*-Linux.rpm

# Debian / Ubuntu
sudo apt install ./sony-device-center-*-Linux.deb
```

Windows: run the `.msi`, or unpack the `.zip`. The Windows packages bundle the Qt
and MSVC runtimes; the Linux packages depend on system Qt 6.

macOS (13+, Apple silicon and Intel): open the `.dmg` and drag **Sony Device
Center** to Applications. The build is not yet signed with a Developer ID, so
the first launch is right-click → **Open**. `sonyd` and `sonyctl` live inside
the bundle at `Contents/MacOS`.

## Known limitations

- **IPC is Unix-socket only.** On Windows `sonyd` cannot serve `sonyctl` or the GUI —
  it reports that it is listening while nothing is. Both clients fall back to a direct
  Bluetooth session, which does work.
- Protocol generation is selected from the Bluetooth device name, not from the SDP
  service UUID. Connecting by address alone to a Protocol V1 headset is not yet safe.
- The desktop app reads device state once at startup and does not refresh.

---

