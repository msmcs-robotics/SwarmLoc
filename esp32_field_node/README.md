# esp32_field_node

ESP32-based field node for the [SwarmLoc](../README.md) positioning project.

## Hardware

- ESP32-WROOM-32D DevKit (CP2102 USB-UART → `/dev/ttyUSB0`)
- DSD Tech 0.96" SSD1306 OLED (I2C, 0x3C, SDA=GPIO21, SCL=GPIO22)
- (Future, M4) MPU6050 IMU on the same I2C bus

## Quick start

```bash
pio run                                            # build
pio run -t upload --upload-port /dev/ttyUSB0       # flash
pio device monitor --port /dev/ttyUSB0             # view serial
```

## Documentation

See [docs/README.md](docs/README.md) for the full doc index, or jump
straight to [docs/scope.md](docs/scope.md) for what's in/out of scope and
[docs/roadmap.md](docs/roadmap.md) for the M0 → M4 plan.

## Status

Initial bootstrap (M0). Hardware not yet wired; first flash pending.
