# DWS1000_UWB

UWB Two-Way Ranging system using Arduino Uno with DWS1000 (DW1000) shields for distance measurement.

## Overview

Measures distance between two UWB radios using Time-of-Flight (ToF) ranging with the DW1000 chip. Target accuracy is ±10-20 cm. Part of the SwarmLoc project for GPS-denied drone swarm positioning.

## Quick Start

```bash
# Build and upload TX test to device 0
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB
pio run -e uno -t upload --upload-port /dev/ttyACM0

# Monitor serial output
python3 scripts/capture_serial.py /dev/ttyACM0 -n 50
```

## Hardware

- **Module**: Qorvo PCL298336 v1.3 (DWS1000 Arduino Shield)
- **Chip**: DW1000 (Device ID: 0xDECA0130) — NOT DWM3000
- **MCU**: 2x Arduino Uno (ATmega328P @ 16MHz)
- **Config**: No J1 jumper, D8→D2 IRQ wire on each shield

## Current Status

- TX: Working 100% on both devices
- RX: Blocked by SPI corruption in RX mode (~55% error rate)
- PLL: Stable after LDO tuning fix from OTP

See [docs/todo.md](docs/todo.md) for current tasks.

## Project Structure

```
DWS1000_UWB/
├── docs/
│   ├── README.md        # Detailed project documentation
│   ├── scope.md         # Project boundaries
│   ├── roadmap.md       # Feature progress
│   ├── todo.md          # Current tasks
│   ├── findings/        # Research and technical findings
│   ├── features/        # Feature specifications
│   └── archive/         # Session summaries and old status reports
├── src/                 # Source code (initiator/responder)
├── tests/               # Test firmware and scripts
│   └── results/         # Test output data (gitignored)
├── lib/                 # Libraries (DW1000, DW1000-ng)
├── scripts/             # Utility scripts (serial capture, calibration)
└── platformio.ini       # Build configuration
```

## Documentation

- [docs/scope.md](docs/scope.md) — What this project is and isn't
- [docs/roadmap.md](docs/roadmap.md) — Feature progress and session history
- [docs/todo.md](docs/todo.md) — Current tasks and blockers

## Requirements

- PlatformIO
- 2x Arduino Uno with DWS1000 shields
- USB cables
- D8→D2 jumper wire on each shield (IRQ routing)

## License

MIT License
