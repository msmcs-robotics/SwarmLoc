# Session — 2026-05-08 (continued) — M4: MPU6050 + web server, dual-core

> Type: continuation of the same autonomous-execution session
> Predecessor: `2026-05-08-session-summary.md` (covered M0 → M3 OPEN)
> Trigger: User wired the MPU6050, asked for "very basic and minimal"
>          web server on port 80 with an IMU monitor; one core for IMU,
>          one core for web. No IMU data on the OLED (that's for the
>          flight controller project).

## What we shipped

### New project files

```
esp32_field_node/
├── lib/
│   ├── MPU6050/                 ← copied from ~/floppi/flight_controller/lib/MPU6050/
│   │   ├── LICENSE              (i2cdevlib MIT)
│   │   ├── README.md
│   │   ├── keywords.txt
│   │   └── src/
│   │       ├── I2Cdev.{h,cpp}
│   │       ├── MPU6050.{h,cpp}
│   │       ├── MPU6050_6Axis_MotionApps20.{h,cpp}
│   │       ├── MPU6050_6Axis_MotionApps612.{h,cpp}
│   │       ├── MPU6050_9Axis_MotionApps41.{h,cpp}
│   │       └── helper_3dmath.h
│   ├── imu/                     ← thin wrapper around MPU6050
│   │   ├── imu.h                (ImuReading struct + 3 functions)
│   │   └── imu.cpp
│   └── web/                     ← minimal HTTP server, port 80
│       ├── web.h                (web_init, web_handle, web_set_imu)
│       └── web.cpp              (embedded HTML, /imu JSON, mutex)
└── docs/
    ├── findings/
    │   ├── mpu6050-wiring.md             ← M4 prep
    │   └── m4-imu-web-success.md         ← live verification
    └── archive/
        └── 2026-05-08-session-m4-summary.md   ← this file
```

`src/main.cpp` was refactored from M3 → M4: dual-core architecture,
global I2C mutex (RAII `I2CLock` guard), IMU task pinned to Core 0,
HTTP server on port 80 in the Arduino loop on Core 1. New serial
commands: `imu` (single read), `web` (print URL).

### Verified on hardware

| Step | Status | Evidence |
|------|--------|----------|
| MPU6050 detected at 0x68 | ✓ | I2C scan |
| `MPU6050::testConnection()` | ✓ | `[imu] MPU6050 detected and configured` |
| Gravity vector | ✓ | `az=+1.01..1.03 g` (chip flat on bench) |
| Gyro at rest | ✓ | All axes < 4 °/s |
| Temperature | ✓ | 24.3 → 24.6 °C |
| Dual-core split | ✓ | `[boot] running on core 1` + `[imu-task] running on core 0` |
| HTTP server up | ✓ | `[web] listening on http://10.232.20.126/`, `web=1` in heartbeat |
| OLED unchanged | ✓ | No regression on the M3 SSID / MAC / IP layout |
| No mutex deadlocks / I2C corruption over 30 s | ✓ | clean log, no errors |

### Build / runtime

- Flash: 78.3% (1.03 MB / 1.31 MB)
- RAM: 15.0% (49 KB / 327 KB)
- Build time: ~15 s
- IMU task: 4 KB stack, priority 2, 20 Hz

## Architecture decisions

- **Reused floppi's vendored MPU6050 lib** instead of pulling
  Adafruit_MPU6050 from the registry. The library is self-contained
  (I2Cdev bundled in the same folder) and compiles on ESP32 because
  Arduino-ESP32 shims `<avr/pgmspace.h>`. Zero new lib_deps.
- **Built-in `WebServer.h`** instead of ESPAsyncWebServer. Per the user's
  "very basic and minimal" requirement. ~10 KB smaller; synchronous
  request handling is fine at 10 Hz polling load.
- **One global I2C mutex** instead of two (one per device). Simpler;
  contention is low because IMU runs at 50 ms cadence and OLED
  refreshes every 10 s.
- **IMU task on Core 0, Arduino loop on Core 1**. Matches the floppi
  pattern documented in `findings/demo-projects-tips-and-tricks.md`.
  Core 1 owns WiFi + web + display + serial CLI; Core 0 is dedicated
  to deterministic IMU sampling.
- **Display still shows only SSID / MAC / IP** — user explicitly
  reserved IMU-on-display for the flight controller project.

## What's left (low priority)

- **Browser test** — user needs to hit `http://10.232.20.126/` from a
  device on MSC - GUEST. Captive-portal client isolation may block
  this; if so, the WPA2-Enterprise path against UAA WiFi -MatSu is the
  fallback test.
- **M3 enterprise live test** — same as before; fill in
  `wifi_credentials.local.h` and re-flash.
- **mDNS** so `swarmloc-node.local` works without IP lookup.
- **Web → WebSocket** if 10 Hz polling becomes a bottleneck.

## Why this is the project's "complete" state

The original scope (`docs/scope.md`) named four end goals:
- WiFi scanning + characterization → ✓ M1
- WiFi connection (open + Enterprise) → ✓ M3 (open live; ent build-only)
- Display status on OLED → ✓ M2 + M3 (SSID/MAC/IP)
- IMU readout → ✓ M4 (MPU6050)

Plus the in-flight ask: "very basic and minimal web server on port 80
with an IMU monitor, dual-core" — ✓ M4.

The roadmap's "first stable release" definition was end-of-M3; we
exceeded it with M4. The project is functionally complete relative to
the stated scope. Future work is enhancement, not delivery.
