# DWS1000_UWB - UWB Two-Way Ranging Project

Ultra-Wideband (UWB) distance measurement system using Qorvo PCL298336 (DWM3000) Arduino shields.

## Project Goal

Measure distance between two UWB radios with **5-10 cm accuracy** using Time-of-Flight (ToF) ranging.

## Hardware

- **Module**: Qorvo PCL298336 v1.3 Arduino Shield (DWM3000EVB)
- **Chip**: DWM3000 with DW3110 UWB IC
- **MCU**: 2x Arduino Uno (ATmega328P @ 16MHz)
- **Connection**: Shields plug directly into Arduino Uno headers

## ⚠️ CRITICAL: Hardware/Software Mismatch

**Your hardware uses DWM3000 chips, NOT DWM1000!**

The original code uses:
```cpp
#include <DW1000.h>  // ← WRONG LIBRARY FOR DWM3000 HARDWARE
```

This project migrates to the correct DWM3000 library.

## Status

**Phase 1 Complete ✓** - Research and Planning
- [x] Code review completed
- [x] Hardware identified: DWM3000 (not DWM1000)
- [x] Library research completed
- [x] Documentation created
- [x] Roadmap established

**Phase 2 Starting** - Library Integration and Setup

## Documentation

### Quick Start
- [📋 Summary](docs/findings/summary.md) - Quick overview of findings
- [🗺️ Roadmap](docs/roadmap.md) - Complete project plan

### Detailed Findings
- [🔍 Code Review](docs/findings/code-review.md) - Analysis of current implementation
- [🔧 Hardware Research](docs/findings/hardware-research.md) - DWM3000 specifications
- [🌐 Web Research](docs/findings/web-research.md) - Library options and community findings

## Architecture

Single PlatformIO project with two environments:

```
DWS1000_UWB/
├── platformio.ini          # Multi-environment config
├── src/
│   ├── initiator/main.cpp  # Device that starts ranging
│   └── responder/main.cpp  # Device that responds
├── lib/                    # DWM3000 library
├── docs/                   # Documentation
└── test_scripts/          # Automation scripts
```

## Development Strategy

### Primary: Arduino Uno
Attempt TWR on Arduino Uno (high risk - community reports TWR not working)

### Backup: ESP32 Migration
If Uno proves inadequate, migrate to ESP32 (proven working)

See [Roadmap](docs/roadmap.md) for details.

## Expected Accuracy

- **Arduino Uno**: ±20-50 cm (if TWR works)
- **ESP32**: ±5-10 cm (proven)

## Getting Started

### Prerequisites
- PlatformIO installed
- 2x Arduino Uno with PCL298336 shields
- USB cables

### Connected Ports
```
/dev/ttyACM0 → Initiator
/dev/ttyACM1 → Responder
```

### Build and Upload (Coming Soon)
```bash
# Upload to initiator
pio run -e initiator -t upload

# Upload to responder
pio run -e responder -t upload

# Monitor both devices
./test_scripts/monitor_both.sh
```

## Key Resources

### Libraries
- **Arduino Uno**: [emineminof/DWM3000-ATMega328p](https://github.com/emineminof/DWM3000-ATMega328p) (TWR broken)
- **ESP32**: [Fhilb/DW3000_Arduino](https://github.com/Fhilb/DW3000_Arduino) (proven working)

### Datasheets
- [DWM3000 Datasheet](https://download.mikroe.com/documents/datasheets/DWM3000_datasheet.pdf)
- [DWM3000EVB Product Page](https://www.qorvo.com/products/p/DWM3000EVB)

### Community
- [Qorvo Tech Forum](https://forum.qorvo.com/c/ultra-wideband/13)
- [Arduino DWM3000 Discussion](https://forum.arduino.cc/t/dwm-3000-collaborative-group/897672)

## Next Steps

1. Source DWM3000 library for ATmega328P
2. Set up PlatformIO project structure
3. Migrate code from `.ino` to PlatformIO
4. Test basic SPI communication
5. Debug TWR implementation

See [Roadmap](docs/roadmap.md) for complete development plan.

## License

MIT License (or specify your license)

## Contributing

This is a learning/development project. If you solve the Arduino Uno TWR challenges, please contribute back to the community!

---

**Note**: Arduino Uno TWR is unproven. Be prepared to migrate to ESP32 if needed.
