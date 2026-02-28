# Calibration Workflow

## Overview

The UWB ranging system uses a multi-stage calibration workflow inspired by the floppi flight controller pattern. Each stage produces values that get written to `include/config.h`, with markers that track calibration progress.

## Important: Requires 2 Devices

Antenna delay calibration **requires two devices** running simultaneously. The antenna delay is only observable through Two-Way Ranging time-of-flight measurements — there is no way to calibrate a single radio in isolation.

The measured distance error reflects the **combined** antenna delay of both devices. The correction is split in half and applied equally to each device (both get the same `ANTENNA_DELAY` value).

## Workflow Stages

### Stage 1: Antenna Delay Calibration

Place radios at a **known distance** (minimum ~20 cm, ideally 50-100 cm).

1. Flash firmware to both devices:
   ```bash
   pio run -e uno_anchor -t upload --upload-port /dev/ttyACM0       # anchor (responder)
   pio run -e uno_calibration -t upload --upload-port /dev/ttyACM1   # calibration tag
   ```
   Or use the dev tool: `tools/dev.sh calibrate`

2. The calibration firmware:
   - Collects 200 TWR measurements
   - Computes mean, StdDev, min, max
   - Calculates per-device antenna delay adjustment
   - Displays results on serial (and OLED if connected)
   - Auto-iterates until error < 5 cm

3. Copy the calibrated antenna delay value to `include/config.h`:
   ```c
   #define CALIBRATED_ANTENNA_DELAY
   #define ANTENNA_DELAY  16405  // your calibrated value
   ```

4. Reflash live firmware:
   ```bash
   pio run -e uno_live -t upload --upload-port /dev/ttyACM0
   pio run -e uno_live -t upload --upload-port /dev/ttyACM1
   ```

### Stage 2: Multi-Distance Validation (optional)

Verify accuracy at several distances. Move radios to 2-3 different positions and record measurements vs known distance. If a linear bias exists, compute scale/offset correction.

```c
#define CALIBRATED_MULTI_DISTANCE
#define DISTANCE_SCALE   1.02f   // if readings consistently 2% short
#define DISTANCE_OFFSET  0.01f   // if readings consistently 1cm off
```

## Build Environments

| Environment | Purpose |
|-------------|---------|
| `uno_anchor` | Anchor/responder — flash to ACM0 |
| `uno_tag` | Tag/initiator — flash to ACM1 (default) |
| `uno_calibration` | Calibration tag with OLED (`-D CALIBRATION_MODE -D USE_OLED_DISPLAY`) |

## Config File Pattern

All calibration values live in `include/config.h` with stage markers:

```c
// Uncomment after completing each stage:
//#define CALIBRATED_ANTENNA_DELAY    // Stage 1
//#define CALIBRATED_MULTI_DISTANCE   // Stage 2

// Calibration values:
#define ANTENNA_DELAY  16405
```

The firmware reads these values at compile time. Uncommenting a `CALIBRATED_*` marker tells the firmware that stage is complete and the values are trusted.

## OLED Display

During calibration, an optional DSDTECH 0.91" OLED (SSD1306, 128x32, I2C) shows:
- Current measurement count
- Running mean and error
- Calibrated antenna delay value

Wiring: SDA → A4, SCL → A5, VCC → 3.3V, GND → GND.

## Serial Output

Calibration results are always printed to serial at 115200 baud, regardless of OLED. The serial output includes config.h-ready values that can be directly copied.

## Tools

- `tools/serial_monitor.py` — Serial monitor with command sending and pattern matching
- `tools/dev.sh` — Build, flash, and monitor workflow
- `scripts/upload_dual_and_capture.sh` — Flash both devices simultaneously
