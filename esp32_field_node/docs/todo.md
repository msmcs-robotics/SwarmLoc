# esp32_field_node — Todo

> Last updated: 2026-05-08, end of M4

## In Progress

_None — project is at end-of-M4 (functionally complete vs. original scope)._

## Blocked

- **M3 enterprise live-test** — needs user-supplied UAA credentials in
  `include/wifi_credentials.local.h`.
- **Browser visit of `http://10.232.20.126/`** — depends on whether
  MSC - GUEST captive portal isolates clients. If isolated, switch to UAA
  WiFi (with creds) instead.

## Up Next

- [ ] **User**: hit `http://10.232.20.126/` from a device on MSC - GUEST
  and confirm the IMU monitor page renders + values update at ~10 Hz.
  Tilt the breakout and watch the numbers change.
- [ ] **User** (optional): `cp include/wifi_credentials.local.h.example
  include/wifi_credentials.local.h`, set USERNAME / PASSWORD, then
  `scripts/upload_and_capture.sh` to live-test the WPA2-Enterprise path
  against `UAA WiFi -MatSu`.
- [ ] mDNS responder (`swarmloc-node.local`) — small, useful, no creds
  required. Reasonable next-session feature.

## Backlog

- [ ] WebSocket replacement for HTTP polling (lower latency)
- [ ] WiFi `onEvent()` refactor (cleaner than polling `WiFi.status()`)
- [ ] NVS / `Preferences` for runtime credential storage
- [ ] OTA firmware update over WiFi
- [ ] NTP time sync (after captive portal authenticated)
- [ ] More I2C peripherals — the bus is wide open (BME280 humidity / temp,
  VL53L0X distance, INA219 current sense — all auto-labelled by
  `i2cDeviceName()` if added)

## Recently Completed (this autonomous session, 2026-05-08)

- [x] M0 — project scaffold, platformio.ini, boot stub
- [x] M1 — WiFi scanner verified: 9 networks visible, encryption-string mapping correct
- [x] **Big finding**: `MSC - GUEST` is OPEN + captive portal (NOT WPA2-Enterprise);
  `UAA WiFi -MatSu` is the user's actual Enterprise network
- [x] M2 — U8g2 SSD1306 128×64 OLED rendering verified
- [x] M3 — OPEN-path WiFi connect verified live (DHCP, captive-portal detection),
  WPA2-Enterprise path build-verified (awaits user creds)
- [x] OLED 3-row redesign (`u8g2_font_helvB12_tf`) — user confirmed the
  thin-stroke font was illegible; bold proportional fixed it
- [x] **M4** — MPU6050 detected, IMU task on Core 0, web server on port 80
  serving live JSON, OLED preserved as SSID/MAC/IP only
- [x] Vendored MPU6050 lib from `~/floppi/flight_controller/`
- [x] 5 parallel Explore agents over the session
- [x] 9 findings docs + execution plan + 2 session records
- [x] Helper scripts (build, monitor, upload_and_capture, capture_serial.py)
- [x] Two git commits

## Notes

- The whole project went from "doesn't exist" to "M4 functionally complete"
  in one autonomous session.
- Build budget: 78.3% flash, 15.0% RAM — plenty of room for OTA + NTP +
  MQTT + a couple more sensors before getting tight.
- Hardware: ESP32-WROOM-32D, OLED + MPU6050 sharing I2C, STA MAC
  `0C:B8:15:C1:39:B8`, on `/dev/ttyUSB0`. Connects to MSC - GUEST.
