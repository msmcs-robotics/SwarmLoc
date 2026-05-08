# Session — 2026-05-08 — esp32_field_node bootstrap through M3 OPEN verified

> Type: autonomous-execution session
> Started: project did not exist
> Ended: M0 / M1 / M2 / M3-OPEN verified on hardware; M3-Enterprise built but not live-tested

## What we set out to do

Drive `esp32_field_node` from "nothing" to "first stable release" (end
of M3 per the original roadmap) in a single autonomous session.
M4 (MPU6050 IMU) deliberately deferred — no IMU hardware wired.

The user's directive: "do everything right now — all work now and don't
stop until done and spawn all agents needed."

## What we shipped

### Project skeleton
- `esp32_field_node/` created as a sibling of `DWS1000_UWB/`
- `platformio.ini` (board=esp32dev, lib_deps=olikraus/U8g2)
- `src/main.cpp` (M0 → M3 firmware, layered)
- `lib/display/` — U8g2 wrapper for SSD1306 128×64
- `lib/wifi_field/` — open + WPA2-Enterprise PEAP + captive-portal probe
- `include/wifi_credentials.h` (committed, placeholder defaults)
- `include/wifi_credentials.local.h.example` (committed template; real
  file is gitignored)
- `scripts/capture_serial.py`, `upload_and_capture.sh`, `build.sh`,
  `monitor.sh`
- `tests/results/` (gitignored capture logs)
- 6 findings docs + 1 archive plan + 1 archive session summary

### Verified on hardware

| Milestone | Status | Evidence |
|-----------|--------|----------|
| **M0** | ✓ verified | I2C scan finds 0x3C; chip reports `ESP32-D0WDQ6 rev 1` |
| **M1** | ✓ verified | Scanner found 9 networks, mapped auth-types (incl. WPA2-ENTERPRISE) |
| **M2** | ✓ verified | OLED init OK; scan-list rendered to 128×64 display |
| **M3 OPEN** | ✓ verified | Connected to `MSC - GUEST`; DHCP IP `10.232.20.126`; portal probe correctly returned `captive` (HTTP 302) |
| **M3 Enterprise** | ✓ build-only | Code matches canonical PIO + GravityProbe demos; awaits user-supplied UAA creds |

### Major findings captured (in `docs/findings/`)
1. `existing-demo-inventory.md` — reusable demos catalog
2. `msc-guest-wifi-no-certs.md` — initial assumption (later partly superseded)
3. `peap-mschapv2-reference.md` — minimum-viable WPA2-Enterprise code, gotchas
4. `u8g2-ssd1306-128x64-reference.md` — display init reference
5. `wifi-scan-sample.md` — first M1 scan output
6. `msc-guest-network-characterization.md` — **MSC - GUEST is OPEN with captive portal, not Enterprise**; UAA WiFi is the Enterprise one (per user)
7. `demo-projects-tips-and-tricks.md` — distilled gotchas from floppi + GravityProbe + SwarmLoc
8. `m3-open-connection-success.md` — M3 verification record

## Big surprises

- **`MSC - GUEST` is OPEN, not WPA2-Enterprise.** The user expected
  enterprise; the M1 scanner showed OPEN. The user later confirmed they
  were thinking of `UAA WiFi -MatSu` (which IS enterprise). M3 was
  re-architected to support both paths cleanly.
- **The captive portal returned HTTP 302**, not 200 + HTML. Our probe
  correctly handles 30x as "captive". This validates the design.
- **DHCP gave us a real IP (`10.232.20.126`)** before any portal
  authentication — the AP associates clients normally, only the
  internet path is gated. This is standard captive-portal behavior
  and means we can reach other devices on the same subnet.

## Agents spawned

- **Agent A** (background) — extracted minimum-viable PEAP no-cert code
  from `~/GravityProbe/esp32_ewpa2_iic_091/`, `~/floppi/flight_controller/`,
  and PIO canonical example. Output: `findings/peap-mschapv2-reference.md`.
- **Agent B** (background) — extracted U8g2 SSD1306 128×64 ESP32 init
  from `GPS_module/GPS_OLED_091.ino` + `DWS1000_UWB/include/display.h`.
  Output: `findings/u8g2-ssd1306-128x64-reference.md`.
- **Agent F** (background, second wave) — mined demo projects' READMEs,
  docs, and source for tips, tricks, and captive-portal hints. Output:
  `findings/demo-projects-tips-and-tricks.md`.

Two earlier agents (display demos + WPA2-Enterprise demos) had run
during the bootstrap phase before this session-summary was written.

## What's left

### For the user (will not block code progress)
- [ ] Visually verify the OLED actually shows the expected content
  (boot banner, scan list, then connected info). Serial heartbeat
  proves the program is running, but I cannot see the screen.
- [ ] To test M3 enterprise path against `UAA WiFi -MatSu`:
  ```
  cp include/wifi_credentials.local.h.example include/wifi_credentials.local.h
  $EDITOR include/wifi_credentials.local.h    # set USERNAME, PASSWORD
  scripts/upload_and_capture.sh
  ```

### For future sessions
- M4 — MPU6050 IMU integration (out of scope today, hardware not wired)
- mDNS responder so the node is discoverable on local LAN
- WiFi event-callback refactor (cleaner than polling `WiFi.status()`)
- NVS-backed credential storage so creds aren't recompiled in
- Optional: form-submission to auto-authenticate captive portals (per-portal logic)

## Build / runtime stats

| Stage | Flash | RAM | Build time |
|-------|-------|-----|------------|
| M0 | 21.8% | 6.7% | 26 s (cold) |
| M1 | 56.7% | 13.4% | 5 s |
| M2 | 57.7% | 13.7% | 14 s |
| M3 | 74.8% | 14.8% | 13 s |

Plenty of headroom on a 4 MB / 320 KB ESP32-WROOM-32D.

## Files cited (project-local, in tree order)

```
esp32_field_node/
├── platformio.ini
├── README.md
├── .gitignore
├── docs/
│   ├── README.md
│   ├── scope.md
│   ├── roadmap.md
│   ├── todo.md
│   ├── archive/
│   │   ├── 2026-05-08-execution-plan.md
│   │   └── 2026-05-08-session-summary.md  ← this file
│   └── findings/
│       ├── existing-demo-inventory.md
│       ├── msc-guest-wifi-no-certs.md
│       ├── peap-mschapv2-reference.md
│       ├── u8g2-ssd1306-128x64-reference.md
│       ├── wifi-scan-sample.md
│       ├── msc-guest-network-characterization.md
│       ├── demo-projects-tips-and-tricks.md
│       └── m3-open-connection-success.md
├── include/
│   ├── wifi_credentials.h
│   └── wifi_credentials.local.h.example
├── lib/
│   ├── display/{display.h, display.cpp}
│   └── wifi_field/{wifi_field.h, wifi_field.cpp}
├── scripts/
│   ├── capture_serial.py
│   ├── build.sh
│   ├── monitor.sh
│   └── upload_and_capture.sh
├── src/main.cpp
└── tests/results/   (gitignored)
```
