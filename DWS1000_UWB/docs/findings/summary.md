# Summary of Findings - DWS1000_UWB Project

**Date**: 2026-01-08

## Critical Discoveries

### 1. Hardware Mismatch: WRONG LIBRARY!

**Your Hardware:**
- Qorvo PCL298336 v1.3 Arduino Shield
- Contains: **DWM3000** module with **DW3110** chip

**Your Current Code:**
```cpp
#include <DW1000.h>  // ← INCOMPATIBLE WITH YOUR HARDWARE!
```

**Required:**
- DWM3000/DW3000 library (NOT DW1000)

**Impact:**
- Current code will NOT initialize your hardware
- All communication will fail
- Must switch to DWM3000 library

---

### 2. Arduino Uno Compatibility Challenges

**Your MCU:** Arduino Uno (ATmega328P @ 16MHz)

**Challenge:** DWM3000 libraries are designed for ESP32 (240MHz)

**Status of Arduino Uno Support:**
| Feature | Status |
|---------|--------|
| SPI Communication | ✓ Working |
| Basic TX/RX | ✓ Working |
| TWR Protocol | ✗ **NOT WORKING** (confirmed by community) |
| Distance Measurement | ✗ **BROKEN** |

**Root Cause:**
- 16MHz CPU too slow for precise timing
- Interrupt latency affects timestamp accuracy
- Memory constraints (2KB RAM, 32KB Flash)

**Evidence:**
- [emineminof/DWM3000-ATMega328p](https://github.com/emineminof/DWM3000-ATMega328p) - TWR examples broken
- [Fhilb/DW3000_Arduino](https://github.com/Fhilb/DW3000_Arduino) - Explicitly states NOT compatible with Uno
- Multiple community reports of failed Arduino Uno attempts

---

### 3. Available Library: ATmega328P Port

**Repository:** [emineminof/DWM3000-ATMega328p](https://github.com/emineminof/DWM3000-ATMega328p)

**Status:**
- ✓ Basic TX/RX working
- ✗ TWR initiator BROKEN
- ✗ TWR responder BROKEN
- ⚠️ Early development (5 commits only)

**This will be our starting point**, but significant debugging required.

---

### 4. Code Issues Found

#### Critical Issues in Current Implementation:

1. **Incomplete TWR Protocol**
   - Only 2 of 4 required messages implemented
   - Missing RANGE and RANGE_REPORT messages

2. **No Timestamp Capture**
   - Timestamps declared but NEVER populated from DW1000
   - All values remain zero
   - Distance calculation uses uninitialized variables

3. **Broken Interrupt Handlers**
   - `receivedAck` flag never set to true
   - Main loop waits forever for flag that never changes
   - No message type discrimination

4. **Missing Error Handling**
   - No SPI verification
   - No chip ID check
   - No timeout mechanisms

**See full details:** [code-review.md](code-review.md)

---

### 5. Connected Hardware Detected

**Arduino Boards Found:**
```
/dev/ttyACM0  (Arduino Uno #1)
/dev/ttyACM1  (Arduino Uno #2)
```

**Port Assignment:**
- `/dev/ttyACM0` → Initiator
- `/dev/ttyACM1` → Responder

---

## Development Strategy

### Primary Path: Arduino Uno (High Risk)

**Attempt** to make TWR work on Arduino Uno using DWM3000 library port.

**Expected Challenges:**
- TWR currently not working in community libraries
- Significant debugging required
- May hit insurmountable CPU/timing limitations

**Decision Point:**
- If TWR cannot be made reliable → Migrate to ESP32

### Backup Path: ESP32 Migration

**Hardware Required:**
- 2x ESP32 DevKit boards (~$10-20 total)
- Breadboards and jumper wires

**Wiring:**
Since PCL298336 is an Arduino shield, manual connections required:
```
Shield Pin → ESP32 Pin
───────────────────────
MOSI → GPIO 23
MISO → GPIO 19
SCK  → GPIO 18
CS   → GPIO 5
IRQ  → GPIO 4
RST  → GPIO 16
3.3V → 3.3V
GND  → GND
```

**Library:** [Fhilb/DW3000_Arduino](https://github.com/Fhilb/DW3000_Arduino) - Proven working

**Outcome:** Achieves target 10cm accuracy reliably

---

## PlatformIO Project Structure

**Single project, multiple environments:**

```
DWS1000_UWB/
├── platformio.ini        # Two environments: initiator, responder
├── src/
│   ├── initiator/
│   │   └── main.cpp
│   └── responder/
│       └── main.cpp
├── lib/                  # DWM3000 library
├── docs/                 # Documentation (this folder)
└── test_scripts/        # Automation scripts
```

**Benefits:**
- Upload to different ports: `pio run -e initiator -t upload`
- Shared library management
- Separate serial monitors per device

---

## Next Steps

### Phase 2: Library Integration (Starting Now)

1. **Source DWM3000 Library**
   - Clone [emineminof/DWM3000-ATMega328p](https://github.com/emineminof/DWM3000-ATMega328p)
   - Extract library code
   - Add to PlatformIO `lib/` folder

2. **Set Up PlatformIO Project**
   - Create `platformio.ini` with two environments
   - Configure ports: `/dev/ttyACM0`, `/dev/ttyACM1`
   - Set up library dependencies

3. **Migrate Code**
   - Convert `.ino` files to `.cpp`
   - Update includes: `DW1000.h` → `DWM3000.h` (or equivalent)
   - Fix API calls for new library

4. **Test Basic Communication**
   - Verify SPI communication
   - Read chip ID (should be `0xDECA0302`)
   - Test basic TX/RX

5. **Debug TWR Implementation**
   - Implement complete 4-message protocol
   - Fix timestamp capture
   - Test distance calculation

---

## Expected Accuracy

### With Arduino Uno (if successful)
- **Target:** 5-10 cm
- **Realistic:** 20-50 cm (due to timing limitations)
- **Uncertainty:** Interrupt latency (±1-3m) is major factor

### With ESP32 (backup plan)
- **Target:** 10 cm
- **Achievable:** 5-10 cm with calibration
- **Proven:** Multiple community examples working

---

## Key Resources

### Documentation
- [Roadmap](../roadmap.md) - Complete project plan
- [Code Review](code-review.md) - Detailed code analysis
- [Hardware Research](hardware-research.md) - DWM3000 specifications
- [Web Research](web-research.md) - Library options and community findings

### Libraries
- **Arduino Uno:** [emineminof/DWM3000-ATMega328p](https://github.com/emineminof/DWM3000-ATMega328p)
- **ESP32:** [Fhilb/DW3000_Arduino](https://github.com/Fhilb/DW3000_Arduino)

### Datasheets
- [DWM3000 Datasheet](https://download.mikroe.com/documents/datasheets/DWM3000_datasheet.pdf)
- [DWM3000EVB Product Page](https://www.qorvo.com/products/p/DWM3000EVB)

### Tutorials (ESP32-based)
- [CircuitDigest: UWB Indoor Positioning](https://circuitdigest.com/microcontrollers-projects/diy-indoor-uwb-positioning-system-using-esp32-and-qorvo-dwm3000)
- [Instructables Tutorial](https://www.instructables.com/UWB-Indoor-Positioning-System-With-ESP32-and-Qorvo/)

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Arduino Uno cannot do TWR | HIGH | Critical | Migrate to ESP32 |
| Library incompatibility | MEDIUM | High | Port from multiple sources |
| Accuracy worse than target | MEDIUM | Medium | Document limitations |
| Long development time | HIGH | Medium | Set decision points |

---

## Status: Phase 1 Complete ✓

**Completed:**
- ✓ Code review
- ✓ Hardware identification
- ✓ Web research
- ✓ Documentation structure
- ✓ Project roadmap
- ✓ Port detection

**Next: Phase 2 - Library Integration and Setup**

**Estimated Time to Working System:**
- Optimistic (Uno works): 10-15 days
- Realistic (Uno struggles): 15-26 days
- With ESP32 migration: 18-26 days

---

## Conclusion

Your PCL298336 boards use DWM3000 chips (NOT DWM1000). Current code is incompatible.

Arduino Uno support is **unproven and risky** - no one has successfully demonstrated working TWR on ATmega328P.

**Recommendation**: Proceed with Arduino Uno development as planned, but be prepared to migrate to ESP32 if we hit technical limitations. ESP32 migration path is well-documented and proven to work.

**Current Status**: Ready to begin Phase 2 - library integration and PlatformIO setup.
