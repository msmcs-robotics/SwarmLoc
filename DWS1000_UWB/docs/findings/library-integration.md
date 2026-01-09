# DWM3000 Library Integration Findings

## Date: 2026-01-08

## Library Source

**Repository**: [emineminof/DWM3000-ATMega328p](https://github.com/emineminof/DWM3000-ATMega328p)

**Status**: Early development - Basic TX/RX works, TWR broken

## Library Structure

```
lib/
├── driver/
│   ├── deca_device_api.c     # Main DWM3000 driver
│   ├── deca_device_api.h
│   ├── deca_regs.h           # Register definitions
│   └── deca_types.h
├── platform/
│   ├── port.c                # Platform-specific (bare ATmega328p)
│   ├── port.h
│   ├── arduino_port.cpp      # NEW: Arduino adaptation
│   └── arduino_port.h        # NEW: Arduino headers
├── examples/
│   ├── simple_tx.c           # Working: Basic transmit
│   ├── simple_rx.c           # Working: Basic receive
│   ├── SS_TWR_INITIATOR.c    # BROKEN: Single-sided TWR initiator
│   └── SS_TWR_RESPONDER.c    # BROKEN: Single-sided TWR responder
└── include/
    ├── main.h
    ├── shared_defines.h      # Constants (SPEED_OF_LIGHT, etc.)
    └── shared_functions.h
```

## Integration Challenges

### Challenge 1: Bare Metal vs Arduino Framework

**Problem**: Library written for Microchip Studio (bare ATmega328p), not Arduino.

**Differences**:
- Bare metal uses direct register access (`DDRB`, `PORTB`, etc.)
- Arduino uses `pinMode()`, `digitalWrite()`, `SPI.h`
- Library expects specific avr/io.h includes
- Different sleep/delay functions

**Solution**: Created Arduino adaptation layer
- `arduino_port.h` - Arduino-compatible function declarations
- `arduino_port.cpp` - Implementations using Arduino.h and SPI.h

### Challenge 2: SPI Implementation

**Original** (in `port.c`):
```c
#define DDR_SPI DDRB
#define DD_SCK DDB5
#define DD_MISO DDB4
#define DD_MOSI DDB3
#define DD_SS DDB2
```

**Arduino Equivalent**:
```cpp
SPI.begin();
SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
```

**Mapping**:
- `spi_init()` → `arduino_spi_init()`
- `readfromspi()` → `arduino_spi_read()`
- `writetospi()` → `arduino_spi_write()`
- `port_set_dw_ic_spi_slowrate()` → `arduino_set_spi_slow()`

### Challenge 3: Pin Definitions

**Shield Pinout** (from hardware research):
```
CS/SS  → D10 (Arduino Uno)
IRQ    → D2  (Hardware interrupt)
RST    → D9  (Reset)
MOSI   → D11 (SPI)
MISO   → D12 (SPI)
SCK    → D13 (SPI)
VCC    → 3.3V
GND    → GND
```

**Arduino Constants**:
```cpp
#define DW_CS_PIN    10
#define DW_IRQ_PIN   2
#define DW_RST_PIN   9
```

### Challenge 4: TWR Examples Are Broken

**From README**:
> "Unfortunately, the TWR-responder and TWR-initiator examples are not currently working..."

**Why TWR Fails** (Analysis of code):

1. **Timing Issues**: Hard-coded delays may not account for Arduino overhead
2. **Interrupt Handling**: Library expects specific ISR configuration
3. **Timestamp Retrieval**: May be reading wrong registers or at wrong times
4. **Clock Synchronization**: DS-TWR requires precise timing

**Our Approach**:
- Start with working simple_tx/simple_rx examples
- Test basic communication first
- Then adapt and debug TWR implementation
- Add extensive serial debug output
- Monitor timestamps at every step

## Current Integration Status

### Completed ✓
- [x] Library cloned and copied to project
- [x] Arduino platform adaptation layer created
- [x] SPI wrapper functions implemented
- [x] Pin definitions mapped

### In Progress
- [ ] Create initiator main.cpp using TWR example
- [ ] Create responder main.cpp using TWR example
- [ ] Bridge library functions to Arduino
- [ ] Add comprehensive debug output

### TODO
- [ ] Test basic SPI communication
- [ ] Verify chip ID read
- [ ] Test simple TX/RX
- [ ] Debug TWR timestamp capture
- [ ] Implement distance calculation
- [ ] Add serial output formatting

## TWR Protocol Implementation Plan

### Message Flow (DS-TWR)

```
Initiator                    Responder
    |                            |
    |-------- POLL ------------->|  T1 (TX), T2 (RX)
    |                            |
    |<------ RESPONSE -----------|  T4 (RX), T3 (TX)
    |                            |
    |------- FINAL ------------->|  T5 (TX), T6 (RX)
    |    (contains T1, T4, T5)   |
    |                            |
   Calculate distance using T1-T6
```

### Required Timestamps

| Timestamp | Description | Captured By | When |
|-----------|-------------|-------------|------|
| T1 | Initiator TX POLL | Initiator | After POLL sent |
| T2 | Responder RX POLL | Responder | When POLL received |
| T3 | Responder TX RESPONSE | Responder | After RESPONSE sent |
| T4 | Initiator RX RESPONSE | Initiator | When RESPONSE received |
| T5 | Initiator TX FINAL | Initiator | After FINAL sent |
| T6 | Responder RX FINAL | Responder | When FINAL received |

### Distance Calculation

```cpp
// Round trip times
int64_t round1 = T4 - T1;  // At initiator
int64_t reply1 = T3 - T2;  // At responder
int64_t round2 = T6 - T3;  // At responder
int64_t reply2 = T5 - T4;  // At initiator

// Time of Flight (cancels clock drift)
int64_t tof_dtu = ((round1 * round2) - (reply1 * reply2)) /
                  (round1 + round2 + reply1 + reply2);

// Convert to distance
float distance_m = tof_dtu * DWT_TIME_UNITS * SPEED_OF_LIGHT;
float distance_cm = distance_m * 100.0;
```

### Constants from `shared_defines.h`

```c
#define SPEED_OF_LIGHT (299702547)  // m/s
#define UUS_TO_DWT_TIME 63898       // Conversion factor

// DWT Time Unit = 1 / (499.2 MHz * 128) ≈ 15.65 ps
#define DWT_TIME_UNITS (1.0 / (499.2e6 * 128.0))
```

## Debug Strategy

### Serial Output Format

```
[INIT] DWM3000 Initiator v1.0
[INIT] SPI initialized
[INIT] Hardware reset complete
[INIT] Reading chip ID...
[INIT] Chip ID: 0xDECA0302 (DW3110)
[INIT] Configuration: CH5, 6.8Mbps, 128-symbol preamble
[INIT] Antenna delay: TX=16385, RX=16385
[INIT] Ready to range

[TWR] Cycle 1 starting...
[TWR] Sending POLL at T1=0x0123456789AB
[TWR] Waiting for RESPONSE...
[TWR] RESPONSE received at T4=0x0123458000
[TWR] Sending FINAL with timestamps
[TWR] FINAL sent at T5=0x0123459000
[CALC] Timestamps: T1=305419896, T2=305420100, T3=305420300, T4=305420500
[CALC] Round1=604, Reply1=200, Round2=600, Reply2=200
[CALC] ToF=302 DTU
[RESULT] Distance: 1.42 m (142 cm) ±20 cm
[TWR] Next cycle in 1000ms
```

### Error Handling

```
[ERROR] SPI communication failed
[ERROR] Chip ID mismatch: expected 0xDECA0302, got 0x00000000
[ERROR] RESPONSE timeout after 300ms
[ERROR] Invalid timestamp: T1=0
[WARN] Large distance jump: 1.2m → 5.8m (possible error)
[WARN] Clock overflow detected in timestamps
```

## Lessons Learned

### From Original .ino Code Review

1. **Never assume timestamps are captured automatically**
   - Must explicitly read from TX_TIME and RX_TIME registers
   - Must read immediately after TX/RX event
   - Original code declared variables but never populated them

2. **Interrupt handlers must be minimal**
   - Just set flags, don't process data
   - Read timestamps in main loop, not ISR
   - Arduino Uno has slow ISR latency

3. **Message protocol must be complete**
   - Original code only had 2 of 3 required messages
   - Must implement full handshake
   - Each message needs proper sequence number

4. **Error detection is critical**
   - Must verify every SPI transaction
   - Must check for timeouts
   - Must validate timestamp values

### Why Community Attempts Failed

Based on research:

1. **CPU Speed**: 16MHz not enough for tight timing loops
2. **Interrupt Latency**: 4-10µs delay affects timestamp accuracy
3. **Memory Constraints**: 2KB RAM limits buffering
4. **Library Maturity**: DWM3000 Arduino support is new/incomplete
5. **Complex Protocol**: DS-TWR requires precise state machine

**Our Strategy**:
- Accept 20-50cm accuracy (not 5-10cm)
- Focus on making it work, not perfecting it
- Extensive debug output
- Document what doesn't work and why

## Next Steps

1. **Create main.cpp files**
   - Adapt TWR examples for Arduino
   - Add Arduino.h includes
   - Use arduino_port functions
   - Add comprehensive serial output

2. **Test incrementally**
   - Test 1: SPI communication, read chip ID
   - Test 2: Simple TX on initiator
   - Test 3: Simple RX on responder
   - Test 4: POLL/RESPONSE handshake
   - Test 5: Full TWR with timestamps
   - Test 6: Distance calculation

3. **Debug TWR**
   - Add timestamp debug at every step
   - Verify register reads
   - Check for overflow
   - Validate calculations

4. **Document results**
   - Actual accuracy achieved
   - Failure modes encountered
   - Limitations found
   - Recommendations for ESP32 migration

## References

- [DWM3000 Datasheet](https://download.mikroe.com/documents/datasheets/DWM3000_datasheet.pdf)
- [emineminof Library](https://github.com/emineminof/DWM3000-ATMega328p)
- [Qorvo Forum TWR Discussion](https://forum.qorvo.com/t/dwm3000-two-way-ranging/12339)
- Project code review: [code-review.md](code-review.md)
- Hardware specs: [hardware-research.md](hardware-research.md)

---

**Status**: Integration layer complete, ready for main.cpp creation
**Date**: 2026-01-08
**Next**: Create initiator and responder main.cpp files
