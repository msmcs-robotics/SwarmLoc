# esp32_field_node — Roadmap

> Last updated: 2026-05-08, end of M4

## Overview

Milestone-driven roadmap. M0 → M4 all completed in a single autonomous
session on 2026-05-08.

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
- [x] Render boot banner ("SwarmLoc node" / "Mx boot")
- [x] Render top-5 WiFi scan results with RSSI bars
- [x] Reuse `GPS_module/GPS_OLED_091.ino` rendering pattern, swapped constructor for 128×64
- [x] 3-row final layout (SSID / MAC / IP) using `u8g2_font_helvB12_tf` for legibility — `display_connected()`

## M3 — WiFi connection ✓ (open live) / ☑ build-only (enterprise)

- [x] Open-network connect path — `WiFi.begin(ssid)` for `MSC - GUEST`
- [x] Captive-portal probe via `http://connectivitycheck.gstatic.com/generate_204` (HTTP/204 = online, 200 / 30x = captive, otherwise error)
- [x] WPA2-Enterprise PEAP/MSCHAPv2 (no certs) connect path — `lib/wifi_field/wifi_field.cpp::wifi_field_connect_enterprise()`. Built clean, awaits user-supplied UAA credentials for live test
- [x] Display SSID + MAC + IP on OLED — `lib/display/display.cpp::display_connected()`
- [x] Verified live: `MSC - GUEST` → IP `10.232.20.126`, MAC `0C:B8:15:C1:39:B8`, portal=captive (HTTP 302) — see `findings/m3-open-connection-success.md`

## M4 — MPU6050 IMU + web server (dual-core) ✓

- [x] Vendor MPU6050 lib from `~/floppi/flight_controller/lib/MPU6050/` (i2cdevlib classic, self-contained)
- [x] `lib/imu/` — thin wrapper exposing `ImuReading {accel,gyro,temp,millis_when}`
- [x] `lib/web/` — minimal HTTP server on port 80 using built-in `WebServer.h`; embedded HTML page polls `/imu` JSON @ 10 Hz
- [x] Dual-core: IMU task pinned to Core 0 @ 20 Hz; Arduino loop on Core 1 handles WiFi / display / web / serial
- [x] Global I2C mutex with RAII `I2CLock` guard around all Wire transactions
- [x] OLED unchanged (SSID / MAC / IP only — IMU data deliberately NOT shown; that's for the flight controller project)
- [x] New serial commands: `imu` (single read) and `web` (print URL)
- [x] Verified live: gravity vector ~1 g on Z, gyro stationary ~0 °/s, temp ~24 °C, web up at `http://10.232.20.126/` — see `findings/m4-imu-web-success.md`

---

## First stable release ✓ — exceeded

End of M3 was the original "first stable release" target. We finished
M4 in the same session, so the project is at end-of-M4 (functionally
complete relative to the original scope).

---

## Future / nice-to-have

- [ ] User-driven test of M3 enterprise path against `UAA WiFi -MatSu` once credentials are filled into `include/wifi_credentials.local.h`
- [ ] Browser test of the web monitor at `http://10.232.20.126/` (may be blocked by captive-portal client isolation; switch to UAA in that case)
- [ ] mDNS responder (`swarmloc-node.local`) so the node is discoverable on local LAN without knowing the IP
- [ ] WebSocket telemetry instead of HTTP polling (lower latency, half the bandwidth) — only if 10 Hz polling becomes a bottleneck
- [ ] Refactor WiFi state to use `WiFi.onEvent()` callbacks instead of polling
- [ ] NVS / `Preferences` for runtime credential storage (no recompile to change creds)
- [ ] OTA firmware update over WiFi
- [ ] NTP time sync (only meaningful once captive portal is authenticated)
- [ ] Optional: form-submission to auto-authenticate captive portals (vendor-specific, fragile, deliberately deferred)
- [ ] Sensor fusion (Madgwick / Kalman / DMP) if we ever want orientation rather than raw accel/gyro — most likely lives in the floppi flight controller project, not here

---

## Script infrastructure ✓

- [x] **build** — `scripts/build.sh` wraps `pio run -d <project>`
- [x] **deploy** — `scripts/upload_and_capture.sh PORT DURATION`
- [x] **monitor** — `scripts/monitor.sh PORT DURATION`
- [x] **capture engine** — `scripts/capture_serial.py`
- [ ] **install** — `scripts/install.sh` — defer until first cold-machine setup
- [ ] **test** — pytest-based modular tests under `tests/` — defer until firmware behavior warrants automated testing

---

## Notes

- "Reuse before rebuild" was load-bearing throughout: M2 reused
  GPS_OLED_091's U8g2 pattern, M3 reused GravityProbe + floppi PEAP
  patterns, M4 reused floppi's MPU6050 library wholesale and the floppi
  dual-core architecture pattern.
- Five Explore agents ran across the session: SSD1306-demo hunt,
  WPA2-Enterprise demo hunt, PEAP code extraction, U8g2 reference
  extraction, and tips-and-tricks mining of all three sibling projects.
- Markdown-lint warnings on docs are template-style choices that match
  the sibling DWS1000_UWB project — leave them.
