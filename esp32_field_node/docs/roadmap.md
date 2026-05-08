# esp32_field_node — Roadmap

> Last updated: 2026-05-08

## Overview

Milestone-driven roadmap. Each milestone corresponds to a stable, demo-able
state of the firmware. M1 (scanner) preceded any auth work — that approach
worked: the scanner revealed `MSC - GUEST` is OPEN with a captive portal
(not WPA2-Enterprise as initially assumed), which directly shaped the M3
design.

---

## M0 — Bootstrap ✓

- [x] Create project structure (src, docs, tests, lib, include + doc subfolders)
- [x] Write `platformio.ini` for `esp32dev` / Arduino framework
- [x] Boot stub: serial banner + I2C scan to confirm OLED is wired
- [x] Wire OLED to ESP32 and verify the I2C scan finds 0x3C — verified 2026-05-08
- [x] First successful upload over /dev/ttyUSB0 + serial monitor confirms heartbeat — `tests/results/m0-boot-2026-05-08.txt`
- [x] Inventory existing demos on filesystem — see `findings/existing-demo-inventory.md`

## M1 — WiFi scanner ✓

- [x] `WiFi.scanNetworks()` over Serial: SSID, RSSI, channel, encryption
- [x] Map encryption int → human string (OPEN, WEP, WPA-PSK, WPA2-PSK, WPA/WPA2-PSK, WPA2-ENTERPRISE, WPA3-PSK, WPA2/WPA3-PSK, WAPI-PSK, UNKNOWN)
- [x] On-demand re-scan via serial command (`scan`)
- [x] Identify "MSC guest" auth type — see `findings/msc-guest-network-characterization.md` (`MSC - GUEST` is OPEN with captive portal; `UAA WiFi -MatSu` is the WPA2-ENTERPRISE network the user previously connected to)

## M2 — OLED status display ✓

- [x] U8g2 SSD1306 128×64 init via `U8G2_SSD1306_128X64_NONAME_F_HW_I2C` — see `lib/display/`
- [x] Render boot banner ("SwarmLoc node" / "M2 boot")
- [x] Render top-5 WiFi scan results with RSSI bars
- [x] Reuse `GPS_module/GPS_OLED_091.ino` rendering pattern, swapped constructor for 128×64

## M3 — WiFi connection ✓ (open path) / ☑ build-only (enterprise path)

- [x] Open-network connect path — `WiFi.begin(ssid)` for `MSC - GUEST`
- [x] Captive-portal probe via `http://connectivitycheck.gstatic.com/generate_204` (HTTP/204 = online, 200 / 30x = captive, otherwise error)
- [x] WPA2-Enterprise PEAP/MSCHAPv2 (no certs) connect path — `lib/wifi_field/wifi_field.cpp::wifi_field_connect_enterprise()`. Built clean, awaits user-supplied UAA credentials for live test
- [x] Display SSID + IP + MAC + RSSI + portal status on OLED — `lib/display/display.cpp::display_connected()`
- [x] Verified live: `MSC - GUEST` → IP `10.232.20.126`, MAC `0C:B8:15:C1:39:B8`, portal=captive (HTTP 302) — see `findings/m3-open-connection-success.md`

## M4 — MPU6050 IMU (next session)

- [ ] Adafruit_MPU6050 init over I2C (same bus as the OLED)
- [ ] Stream accel + gyro to serial
- [ ] Render real-time roll/pitch on OLED
- [ ] **Hardware required**: MPU6050 breakout wired to 3.3V / GND / SDA=GPIO21 / SCL=GPIO22

---

## First stable release ✓ (open path)

End of M3 was: ESP32 boots, scans, auto-connects to MSC - GUEST, shows
live SSID / IP / MAC / RSSI / portal-status on OLED, refreshes every
10 s. Achieved 2026-05-08. M4 (IMU) is a feature add on top.

---

## Nice to have *(later)*

- [ ] User-driven test of M3 enterprise path against `UAA WiFi -MatSu` once credentials are filled into `include/wifi_credentials.local.h`
- [ ] mDNS responder for local LAN discovery (works behind captive portal)
- [ ] WiFi event-callback refactor (cleaner than polling `WiFi.status()` — see `findings/demo-projects-tips-and-tricks.md`)
- [ ] NVS-backed credential storage so creds aren't recompiled in (Preferences API)
- [ ] NTP time sync (only meaningful once portal is authenticated)
- [ ] OTA firmware update over WiFi
- [ ] MQTT publish of telemetry
- [ ] Web UI for live status

---

## Script infrastructure ✓

Per [llm-project-bootstrap PROJECT_SCRIPTS.md](../../../llm-project-bootstrap/guides/PROJECT_SCRIPTS.md):

- [x] **build** — `scripts/build.sh` wraps `pio run -d <project>`
- [x] **deploy** — `scripts/upload_and_capture.sh PORT DURATION` — pio upload + serial capture
- [x] **monitor** — `scripts/monitor.sh PORT DURATION` — serial capture only (resets via RTS)
- [x] **capture engine** — `scripts/capture_serial.py` — reusable Python helper used by both wrappers
- [ ] **install** — `scripts/install.sh` — verify PlatformIO + run `pio platform install espressif32`. Defer until first cold-machine setup is needed.
- [ ] **test** — pytest-based modular tests under `tests/`. Defer until we have firmware whose behavior warrants automated testing (currently each milestone is verified by capture-and-visual-check).

---

## Notes

- "Reuse before rebuild" was load-bearing: the user had already fought the WPA2-Enterprise cert flow on a prior project, and the captive-portal logic could have been a rabbit hole. Mining the existing demos (~/floppi, ~/GravityProbe, sibling SwarmLoc projects) saved hours.
- For research-heavy tasks (WPA2-Enterprise API surface, ESP32 quirks, U8g2 init), parallel Explore agents proved much faster than serial reading.
