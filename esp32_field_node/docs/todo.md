# esp32_field_node — Todo

> Last updated: 2026-05-08, end of bootstrap-through-M3 session

## In Progress

_None — M0 → M3 (open path) complete; awaiting next-session direction._

## Blocked

- M3 enterprise live-test — needs user-supplied UAA WiFi credentials
  (see `docs/findings/m3-open-connection-success.md` for steps)

## Up Next

- [ ] **User**: visually confirm the OLED shows the expected content
  (boot banner → scan list → connected info: SSID, IP, MAC, RSSI, "captive")
- [ ] **User** (optional): test M3 enterprise path against `UAA WiFi -MatSu`:
  ```
  cp include/wifi_credentials.local.h.example include/wifi_credentials.local.h
  $EDITOR include/wifi_credentials.local.h    # set USERNAME, PASSWORD
  scripts/upload_and_capture.sh
  ```
- [ ] M4 — MPU6050 IMU integration (hardware not yet wired — defer until breakout is on the breadboard)

## Backlog

- [ ] mDNS responder so the node is discoverable on the local LAN (works even behind captive portal)
- [ ] Refactor WiFi state to use `WiFi.onEvent()` callbacks instead of polling
- [ ] NVS / `Preferences` for runtime credential storage
- [ ] (much later) Auto-completing the captive-portal form — vendor-specific, fragile, deliberately deferred
- [ ] Optional second build env for an `esp32dev_imu` variant once IMU is wired (mirror DWS1000_UWB's multi-env pattern)

## Recently Completed (this session)

- [x] 2026-05-08 — Project scaffold created at `/home/devel/SwarmLoc/esp32_field_node/`
- [x] 2026-05-08 — Switched lib_deps to `olikraus/U8g2` (matches repo convention)
- [x] 2026-05-08 — Spawned 5 parallel Explore agents over the session (display demo hunt, WPA2-Enterprise demo hunt, PEAP code extraction, U8g2 ref extraction, demo-projects tips-and-tricks mining); all results captured in `docs/findings/`
- [x] 2026-05-08 — **M0 verified** on hardware (boot banner, chip ID, I2C 0x3C found)
- [x] 2026-05-08 — **M1 verified** on hardware (WiFi scanner found 9 networks; encryption-string mapping correct)
- [x] 2026-05-08 — **Big finding**: MSC - GUEST is OPEN with captive portal, NOT WPA2-Enterprise. UAA WiFi -MatSu is the enterprise one (per user)
- [x] 2026-05-08 — **M2 verified** on hardware (OLED init OK; boot banner + scan-list rendered)
- [x] 2026-05-08 — **M3 OPEN path verified** end-to-end: connected to MSC - GUEST, DHCP IP `10.232.20.126`, captive portal detected via HTTP 302
- [x] 2026-05-08 — M3 enterprise path built (compiles clean), awaits user creds
- [x] 2026-05-08 — Helper scripts: `build.sh`, `monitor.sh`, `upload_and_capture.sh`, `capture_serial.py`
- [x] 2026-05-08 — Findings docs: existing-demo-inventory, msc-guest-wifi-no-certs, peap-mschapv2-reference, u8g2-ssd1306-128x64-reference, wifi-scan-sample, msc-guest-network-characterization, demo-projects-tips-and-tricks, m3-open-connection-success
- [x] 2026-05-08 — Session record: `docs/archive/2026-05-08-session-summary.md`
- [x] 2026-05-08 — Roadmap and todo updated

## Notes

- MSC - GUEST: open, captive portal, DHCP gives a real LAN IP (10.232.20.x). The portal blocks the wider internet but not LAN-local traffic.
- UAA WiFi -MatSu: WPA2-Enterprise PEAP/MSCHAPv2, no certs needed (per user); code is ready in `lib/wifi_field/`, awaits credentials.
- Hardware: ESP32-WROOM-32D, STA MAC `0C:B8:15:C1:39:B8`, on `/dev/ttyUSB0`.
