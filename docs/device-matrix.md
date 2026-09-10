# Sony Audio Device Compatibility & Hardware Validation Matrix

This document tracks hardware-level verification and protocol capability support across known Sony headphones and earbuds for the Sony Device Center SDK and desktop applications.

> **Validation Policy**: Devices must never be marked as *Verified* based solely on protocol similarity. Real-world testing with physical hardware and specific firmware versions is required.

## Support Status Legend

- **Verified**: Fully tested and confirmed working with physical hardware on the noted firmware version.
- **Partially Verified**: Core controls confirmed working; specific secondary features pending verification.
- **Expected**: Supported according to reverse-engineered protocol generation and profile registry, but awaiting physical device validation.
- **Unknown**: Protocol compatibility unconfirmed or untested.

---

## Hardware Matrix

| Device | Protocol | Connection | Battery | ANC | Ambient | EQ | DSEE | Firmware | Codec | Speak-to-Chat | Auto Power-Off | Tested Firmware | Tester |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **WH-1000XM3** | V1 | RFCOMM | Verified | Verified | Verified | N/A (V1) | N/A (V1) | Unknown | SBC, AAC, LDAC, aptX | N/A | N/A | 4.5.2 | Community |
| **WH-1000XM4** | V1 | RFCOMM | Verified | Verified | Verified | N/A (V1) | N/A (V1) | Unknown | SBC, AAC, LDAC | Partially Verified | Partially Verified | 2.5.0 | Baseline |
| **WH-1000XM5** | V2 | RFCOMM | Verified | Verified | Verified | Verified | Verified | Verified | SBC, AAC, LDAC | Verified | Verified | 2.3.1 | Core Dev |
| **WH-1000XM6** | V2 | RFCOMM | Expected | Expected | Expected | Expected | Expected | Expected | Expected | Expected | Expected | — | Unreleased |
| **WF-1000XM4** | V2 | RFCOMM | Expected (Dual+Case) | Expected | Expected | Expected | Expected | Expected | SBC, AAC, LDAC | Expected | Expected | — | Awaiting HW |
| **WF-1000XM5** | V2 | RFCOMM | Expected (Dual+Case) | Expected | Expected | Expected | Expected | Expected | SBC, AAC, LDAC | Expected | Expected | — | Awaiting HW |
| **WH-CH720N** | V2 | RFCOMM | Expected | Expected | Expected | Expected | Expected | Expected | SBC, AAC | N/A | Expected | — | Awaiting HW |
| **ULT WEAR** | V2 | RFCOMM | Expected | Expected | Expected | Expected | Expected | Expected | SBC, AAC, LDAC | Expected | Expected | — | Awaiting HW |
| **LinkBuds S** | V2 | RFCOMM | Expected (Dual+Case) | Expected | Expected | Expected | Expected | Expected | SBC, AAC, LDAC | Expected | Expected | — | Awaiting HW |

---

## Protocol Differences Summary

### Protocol V1 (e.g. WH-1000XM3, WH-1000XM4 legacy)
- Fixed command lengths without variable payloads.
- Single battery level query.
- **Opcode `0x22` is POWER OFF** — must NEVER be transmitted to a V1 device to query battery.
- Equalizer and DSEE control handled via separate legacy control messages or not exposed in standard V1 profile.

### Protocol V2 (e.g. WH-1000XM5, WF-1000XM4/M5, LinkBuds, ULT WEAR)
- Extended variable-length payload structures.
- **Opcode `0x22` is BATTERY QUERY** (returns `0x23`).
- Subtypes for dual battery channels (left, right) and charging case.
- Dynamic ANC and 20-step Ambient sound level control (`0x66` / `0x67` / `0x68`).
- 5-band graphic equalizer with Clear Bass (`0x56` / `0x57` / `0x58`).
- DSEE Extreme toggle (`0xe6` / `0xe7` / `0xe8`).
- Speak-to-Chat toggle (`0xf6` / `0xf7` / `0xf8`).
- Auto Power-Off configuration (`0x26` / `0x27` / `0x28`).

---

## Hardware Validation Instructions with `sonyctl`

To validate a newly connected physical device:

1. **Check Bluetooth pairing**:
   ```bash
   bluetoothctl devices Connected
   ```

2. **Run device discovery**:
   ```bash
   sonyctl devices
   ```

3. **Verify profile and capabilities identification**:
   ```bash
   sonyctl info
   ```

4. **Verify battery reporting**:
   ```bash
   sonyctl battery
   ```

5. **Test Noise Control toggles**:
   ```bash
   sonyctl anc on
   sonyctl ambient 10
   sonyctl anc off
   ```

6. **Test Equalizer (V2 devices)**:
   ```bash
   sonyctl eq get
   sonyctl eq preset bass-boost
   sonyctl eq custom 5 1 2 3 4 5
   ```

7. **Test DSEE & Audio features**:
   ```bash
   sonyctl dsee on
   sonyctl dsee off
   ```
