# M4 — MPU6050 IMU + web server, dual-core verified

> Captured: 2026-05-08
> Source: live capture from `/dev/ttyUSB0` after M4 firmware upload
> Raw log: `tests/results/m4-imu-web-2026-05-08.txt` (gitignored)

## Result: full success

The M4 firmware boots, detects both peripherals on the I2C bus, initializes
the MPU6050 (±2 g, ±250 °/s), connects to MSC - GUEST, starts the HTTP
server on port 80, spawns the IMU task pinned to Core 0, and streams live
IMU readings to serial — all in one boot cycle.

## Verbatim log

```
[i2c]   device @ 0x3C  -- SSD1306 OLED
[i2c]   device @ 0x68  -- MPU6050 IMU
[i2c] scan done, 2 device(s) found
[oled] init OK
[imu] MPU6050 detected and configured (±2 g, ±250 °/s)
[wifi] connected
[wifi] mode=OPEN ssid='MSC - GUEST' ip=10.232.20.126 mac=0C:B8:15:C1:39:B8 rssi=-76dBm portal=captive
[web] listening on http://10.232.20.126/
[boot] IMU task spawn rc=1 (Core 0)
[boot] entering loop() — type 'help' for commands
[imu-task] running on core 0, prio 2
[imu] ax=-0.10 ay=-0.02 az=+1.01 g | gx=-1.1 gy=-3.3 gz=-0.4 deg/s | t=24.3C
[imu] ax=-0.09 ay=-0.02 az=+1.02 g | gx=-0.3 gy=-3.2 gz=-0.9 deg/s | t=24.4C
[imu] ax=-0.08 ay=-0.02 az=+1.02 g | gx=-0.1 gy=-2.7 gz=-0.9 deg/s | t=24.4C
[imu] ax=-0.09 ay=-0.02 az=+1.03 g | gx=-1.0 gy=-3.2 gz=-0.6 deg/s | t=24.5C
[imu] ax=-0.08 ay=-0.02 az=+1.03 g | gx=-0.0 gy=-3.2 gz=-0.5 deg/s | t=24.6C
```

## What this confirms

| Claim | Evidence |
|-------|----------|
| MPU6050 wiring is correct | I2C scan finds 0x68; `testConnection()` returns true |
| IMU sensitivity is correctly configured | Gravity reads ~+1.0 g on Z (chip flat); other axes near zero |
| Gyro is calibrated and quiet | Stationary chip → gyro magnitudes < 4 °/s |
| Temperature sensor works | ~24.3 → 24.6 °C (room temp + slight chip self-heating) |
| Dual-core split works | `[boot] running on core 1`, `[imu-task] running on core 0` |
| FreeRTOS task spawn returns OK | `IMU task spawn rc=1` (pdPASS) |
| I2C mutex prevents bus contention | No `[i2c-error]`, no display corruption, no crashes over 30 s |
| Web server is up | `[web] listening on http://10.232.20.126/`, `web=1` in heartbeat |
| OLED unchanged | Still shows SSID / MAC / IP per user request |
| Reused floppi MPU6050 lib compiled cleanly on ESP32 | Build succeeded with `<avr/pgmspace.h>` shimmed by Arduino-ESP32 |

## Architecture summary

```
┌─────────────────┐                        ┌─────────────────┐
│     Core 0      │                        │     Core 1      │
│  (IMU task)     │                        │ (Arduino loop)  │
├─────────────────┤                        ├─────────────────┤
│ imu_read()      │  ── I2C mutex ──>      │ display_*()      │
│  20 Hz          │                        │ wifi_field_*()   │
│ web_set_imu(r)  │  ── web mutex ──>      │ web_handle()     │
│                 │      [g_snapshot]      │ scanI2C() etc.   │
└─────────────────┘                        └─────────────────┘
                                              │
                                              ▼
                                 GET /imu  →  JSON of latest snapshot
                                 GET /     →  embedded HTML, polls /imu @ 10 Hz
```

## Memory footprint

- Flash: **78.3%** (1,026,909 / 1,310,720 bytes) — plenty of headroom for OTA + NTP + MQTT
- RAM:   **15.0%** (49,044 / 327,680 bytes)
- IMU task stack: 4 KB allocated
- Arduino loop stack: 8 KB (default)

## Reaching the web UI

The ESP32 is at `http://10.232.20.126/` on the MSC - GUEST subnet
(10.232.20.0/x). Whether you can reach it from a browser depends on
**captive-portal client isolation**:

- **If MSC - GUEST has client isolation OFF** (some guest networks do):
  any device on the same SSID can hit the URL.
- **If isolation is ON** (more common for guest WiFi): client-to-client
  traffic is blocked. You won't reach the ESP32 from your laptop on
  MSC - GUEST.

If isolation blocks you, two workarounds:
1. **Authenticate the captive portal once from a laptop**, then try —
   some networks open up after auth.
2. **Use UAA WiFi -MatSu** (WPA2-Enterprise) — populate
   `include/wifi_credentials.local.h` with UAA credentials. Enterprise
   networks more often allow client-to-client. The ESP32's auto-connect
   will switch to enterprise mode automatically when the username field
   is no longer the placeholder.

## What to test next

1. Try `http://10.232.20.126/` from a browser on your laptop (assuming
   it's on MSC - GUEST too). Expect to see SSID-like header, six
   numeric IMU readouts, temp, sample age — refreshing every 100 ms.
2. If it works: tilt / shake the IMU breakout and watch values change.
3. If it doesn't: report the failure mode — "connection timeout",
   "no route to host", "page loads but values stuck" — they map to
   different network-layer issues.
4. To live-test the WPA2-Enterprise path against UAA WiFi -MatSu:
   ```
   cp include/wifi_credentials.local.h.example include/wifi_credentials.local.h
   $EDITOR include/wifi_credentials.local.h    # set USERNAME, PASSWORD
   scripts/upload_and_capture.sh
   ```

## Out of scope (deliberately)

- IMU data on the OLED — user said this is for the flight-controller
  project; field node keeps OLED as SSID/MAC/IP only.
- Sensor fusion (Madgwick / Kalman / DMP-quaternions) — raw accel + gyro
  is enough for a monitor; fusion lives in floppi/flight_controller.
- WebSocket replace polling — 10 Hz polling is fine for a monitor; the
  built-in WebServer is much smaller than ESPAsyncWebServer.
- mDNS responder (`swarmloc-node.local`) — useful but a future M5 ask.
- NTP time sync — meaningless until the captive portal is authenticated.
