# DWS1000_UWB

UWB Two-Way Ranging system using Arduino Uno with DWS1000 (DW1000) shields for distance measurement. Achieves ±4.4 cm precision at 9.4 Hz after antenna delay calibration. Part of the SwarmLoc project for GPS-denied drone swarm positioning.

## Quick Start

```bash
cd /path/to/SwarmLoc/DWS1000_UWB

# Flash anchor (responder) to device 0
scripts/upload_and_capture.sh /dev/ttyACM0 10 /tmp/anchor.txt "Listening"

# Flash tag (initiator) to device 1
scripts/upload_and_capture.sh /dev/ttyACM1 30 /tmp/tag.txt "R#1"

# Or flash both simultaneously
scripts/upload_dual_and_capture.sh tests/test_twr_anchor.cpp /dev/ttyACM0 \
    tests/test_twr_tag.cpp /dev/ttyACM1 60
```

## Hardware Setup

### Components

- 2x Arduino Uno (ATmega328P @ 16MHz)
- 2x Qorvo PCL298336 v1.3 shields (DWS1000 — contains DW1000 chip, NOT DWM3000)
- 2x USB cables
- 2x short jumper wires (for D8-to-D2 on each shield)

### Required Wiring (per shield)

Both shields need two modifications before they will work:

**1. J1 Jumper — REQUIRED for RX**

The J1 header is a 3-pin header near the DC-DC converter on the DWS1000 shield. Install a jumper on **pins 1-2** (the two pins closest to the board edge / DC-DC converter side).

```
  J1 Header (3 pins):
  [1]--[2]  [3]
   ^^^^^
   Jumper here (pins 1-2)
```

This connects the on-board 3.3V DC-DC converter output to the DWM1000 module's power rail. **Without this jumper, the DWM1000 is not powered properly** — it floats on parasitic voltage through SPI ESD protection diodes, causing PLL lock failure and zero RX capability.

**2. D8 to D2 Wire — IRQ Routing**

The DWS1000 shield routes the DW1000 interrupt output to Arduino pin D8, but the firmware expects it on D2 (INT0, hardware interrupt capable). Run a short jumper wire from D8 to D2 on the shield headers.

```
  Arduino Uno pin headers:
  ... [D7] [D8]----wire----[D2] [D1] ...
```

### Pin Map

| Arduino Pin | Function | Notes |
|------------|----------|-------|
| D2 | DW1000 IRQ | Wired from D8 (INT0) |
| D7 | DW1000 RST | Reset line |
| D8 | Shield IRQ out | Bridge to D2 |
| D10 (SS) | DW1000 SPI CS | SPI chip select |
| D11 (MOSI) | DW1000 SPI MOSI | SPI data out |
| D12 (MISO) | DW1000 SPI MISO | SPI data in |
| D13 (SCK) | DW1000 SPI SCK | SPI clock |
| A4 (SDA) | OLED display | I2C data (optional) |
| A5 (SCL) | OLED display | I2C clock (optional) |

### Device Roles

| Port | Role | LDO Tuning | Notes |
|------|------|------------|-------|
| /dev/ttyACM0 | Anchor (responder) | 0x88 | Better RX performance |
| /dev/ttyACM1 | Tag (initiator) | 0x28 | Initiates ranging |

## Current Status

| Function | Status | Performance |
|----------|--------|-------------|
| TWR Ranging | Working | 9.4 Hz, ±4.4 cm StdDev |
| Accuracy | Calibrated | +4.6 cm mean error (target: ±10-20 cm) |
| Antenna delay | 16405 | Calibrated from default 16436 |
| TX | 100% | Both devices |
| RX | 78-100% | ACM0=100%, ACM1=78% |

## Radio Configuration

- Data rate: 850 kbps
- PRF: 16 MHz
- Channel: 5 (6.5 GHz)
- Preamble: 256 symbols
- Preamble code: 3
- Library: DW1000-ng (local, in `lib/DW1000-ng/`)

## Project Structure

```
DWS1000_UWB/
├── include/
│   └── config.h            # Calibration values and feature flags
├── src/main.cpp             # Active firmware (copied from tests/)
├── tests/
│   ├── test_twr_anchor.cpp  # TWR anchor (responder) firmware
│   ├── test_twr_tag.cpp     # TWR tag (initiator) firmware
│   └── test_calibration_tag.cpp  # Antenna delay calibration
├── lib/
│   ├── DW1000-ng/           # UWB transceiver library (local, modified)
│   └── DW1000/              # Legacy library (thotro, deprecated)
├── tools/                   # Serial monitor, calibration scripts
├── scripts/                 # Upload and capture scripts
├── docs/
│   ├── scope.md             # Project boundaries
│   ├── roadmap.md           # Feature progress
│   ├── todo.md              # Current tasks
│   └── findings/            # Technical findings and session logs
└── platformio.ini           # Build configuration
```

## Build Environments

| Environment | Command | Use |
|-------------|---------|-----|
| `uno_ng` | `pio run -e uno_ng` | Default build (DW1000-ng library) |
| `uno` | `pio run -e uno` | Legacy (thotro library, deprecated) |

## Documentation

- [docs/scope.md](docs/scope.md) — Project scope and requirements
- [docs/roadmap.md](docs/roadmap.md) — Progress and session history
- [docs/todo.md](docs/todo.md) — Current tasks
- [docs/findings/](docs/findings/) — Technical investigations and results

## Requirements

- PlatformIO CLI
- 2x Arduino Uno + DWS1000 shields (with J1 jumper and D8-D2 wire)
- USB cables
- Python 3 (for serial tools)

## License

MIT License
