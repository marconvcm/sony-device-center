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
Center** to Applications. `sonyd` and `sonyctl` live inside the bundle at
`Contents/MacOS`.

### macOS: “Apple could not verify” warning

Builds distributed without Developer ID signing and Apple notarization trigger
Gatekeeper ([#57](https://github.com/marconvcm/sony-device-center/issues/57)).
If you trust this release, try opening the installed app once, click **Done**,
then go to **System Settings → Privacy & Security → Open Anyway** and confirm.
On macOS 15 Sequoia and later, right-click → Open does not override this warning.
See [Apple's instructions](https://support.apple.com/en-us/102445).

If Open Anyway is unavailable, the workaround reported in #57 is:

```bash
xattr -dr com.apple.quarantine "/Applications/Sony Device Center.app"
```

This bypasses the quarantine check for this app and its bundled files; it does
not verify their safety or notarize them. Only use it for a release you trust.
The recursive `-r` is required for quarantined files inside the bundle.

## Known limitations

- **IPC is Unix-socket only.** On Windows `sonyd` cannot serve `sonyctl` or the GUI —
  both clients use direct Bluetooth sessions. The daemon reports IPC as unsupported.
- Protocol generation and capabilities are selected from the device profile.
  Unknown models fall back to V1 with limited capabilities; a Bluetooth connection
  alone does not establish model support.
- Original macOS hardware crash/stall reports and WF-1000XM4 audible transitions
  and non-default wind-control preservation still need physical acceptance testing.
- Linux SDP discovery and connection setup remain synchronous. Separate Bluetooth
  applications or direct sessions can compete for the headset's control channel.

---
