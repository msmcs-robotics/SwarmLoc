# esp32_field_node — Documentation

ESP32-WROOM-32D field node firmware for the SwarmLoc system. Provides WiFi
probing/connectivity, an SSD1306 OLED status display, and (later) an MPU6050
IMU readout.

## Status

- **Phase**: M0 — Bootstrap (just initialized 2026-05-08)
- **Hardware**: ESP32 DevKit (WROOM-32D, CP2102) on `/dev/ttyUSB0`
- **Display**: DSD Tech 0.96" SSD1306 OLED (I2C, 0x3C) — not yet wired
- **MSC guest WiFi**: auth type **unknown** — to be characterized in M1

## Where to look

| Document | Purpose |
|----------|---------|
| [scope.md](scope.md) | What this project IS and IS NOT |
| [roadmap.md](roadmap.md) | M0 → M4 milestones, definition of first stable release |
| [todo.md](todo.md) | Active and upcoming tasks |
| [features/](features/) | Feature specs (populated as features land) |
| [findings/](findings/) | Research and discoveries |
| [archive/](archive/) | Session summaries |

## Quick start

```bash
cd /home/devel/SwarmLoc/esp32_field_node
pio run                                            # build
pio run -t upload --upload-port /dev/ttyUSB0       # flash
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

Expected output on first boot (M0):

```
=== esp32_field_node — bootstrap (M0) ===
CPU: ESP32-D0WD-V3 rev 3, 2 core(s) @ 240 MHz, flash 4096KB
[i2c] scanning bus...
[i2c]   device @ 0x3C  <- expected SSD1306
[i2c] scan done, 1 device(s) found
[boot] entering loop()
[heartbeat] 1 — uptime 2s
[heartbeat] 2 — uptime 4s
...
```

If the I2C scan reports 0 devices, the OLED is not wired correctly — check
`SDA=GPIO21`, `SCL=GPIO22`, `VCC=3.3V`, `GND`, and that the display has not
been damaged.
