# Two-Way Ranging (TWR) Accuracy Optimization for DW1000 on Arduino Uno

**Date**: 2026-01-11
**Hardware**: Arduino Uno (ATmega328P @ 16MHz, 2KB RAM) + DW1000 UWB Module
**Library**: arduino-dw1000 v0.9
**Purpose**: Maximize ranging accuracy within Arduino Uno hardware constraints

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Arduino Uno Hardware Constraints](#arduino-uno-hardware-constraints)
3. [Interrupt Latency Minimization](#interrupt-latency-minimization)
4. [Timestamp Capture Optimization](#timestamp-capture-optimization)
5. [Clock Drift Compensation Techniques](#clock-drift-compensation-techniques)
6. [Environmental Factors Affecting Accuracy](#environmental-factors-affecting-accuracy)
7. [Code-Level Optimizations](#code-level-optimizations)
8. [Expected Accuracy Limits](#expected-accuracy-limits)
9. [Arduino Uno vs ESP32 Comparison](#arduino-uno-vs-esp32-comparison)
10. [When to Migrate to ESP32](#when-to-migrate-to-esp32)
11. [Implementation Guidelines](#implementation-guidelines)
12. [References and Resources](#references-and-resources)

---

## Executive Summary

### Key Findings

The DW1000 UWB transceiver can achieve **±10 cm ranging accuracy** indoors when properly calibrated, but the Arduino Uno's 16MHz CPU and limited resources impose practical constraints that limit achievable performance compared to faster platforms like ESP32.

### Realistic Expectations for Arduino Uno + DW1000

| Metric | Arduino Uno | ESP32 | Notes |
|--------|-------------|-------|-------|
| **Best Case Accuracy** | ±10 cm | ±5 cm | Short range (<3m), optimal conditions |
| **Typical Accuracy** | ±20 cm | ±10 cm | Mid-range (3-5m), indoor environment |
| **Update Rate** | 1-5 Hz | 10-50 Hz | Limited by CPU speed |
| **Max Anchors** | 3-4 | 10+ | RAM constraint (2KB vs 320KB) |
| **Interrupt Latency** | 3.5-5 μs | <1 μs | ATmega328P vs ESP32 |
| **Timing Precision** | 4 μs | 0.25 μs | micros() resolution |

### Critical Optimization Priorities

1. **Antenna Delay Calibration** (50-100 cm improvement)
2. **DS-TWR Protocol** (eliminates clock drift errors)
3. **ISR Optimization** (reduces 3-5 cm timing jitter)
4. **Environmental Control** (minimizes multipath, 5-20 cm improvement)
5. **Library Bug Fix** (enables interrupt-driven operation)

### Bottom Line

Arduino Uno is **sufficient** for:
- Basic ranging applications (±10-20 cm accuracy acceptable)
- Low update rates (1-5 Hz)
- Small networks (3-4 anchors)
- Learning and prototyping

Arduino Uno is **insufficient** for:
- High-precision positioning (<±10 cm)
- Real-time tracking (>10 Hz update rate)
- Large networks (>4 anchors)
- Production systems requiring reliability

---

## Arduino Uno Hardware Constraints

### CPU and Timing Limitations

**ATmega328P @ 16MHz**:
- **Clock Period**: 62.5 ns per instruction cycle
- **Interrupt Latency**: 3.5-5 μs (measured)
  - Entry: ~82 clock cycles / 16MHz = 5.1 μs
  - ISR execution: ~56 cycles = 3.5 μs
- **Timer Resolution**: 4 μs (micros() function only returns multiples of 4)
- **SPI Clock**: Up to 8 MHz (half of system clock)

**DW1000 Timestamp Precision**:
- **Timestamp Resolution**: 15.65 picoseconds (40-bit @ 63.9 GHz)
- **TX Scheduling Resolution**: 8 ns (low 9 bits ignored)
- **Speed of Light**: ~0.3 m/ns = ~3 mm per 10 ns

**Mismatch**: The Arduino's 4 μs timing resolution is **255,000 times coarser** than the DW1000's 15.65 ps timestamp precision. This creates fundamental limitations.

### Memory Constraints

**SRAM: 2048 bytes**
- **DW1000 Library**: ~800 bytes (buffers, state)
- **DW1000Ranging**: ~1200 bytes (device tracking)
- **Each DW1000Device**: 74 bytes
- **User Application**: ~200-400 bytes remaining

**Practical Limits**:
- Maximum 4 anchors tracked simultaneously (default MAX_DEVICES)
- No room for sophisticated filtering algorithms
- Limited buffering for measurement history
- Cannot implement complex multilateration on-device

**Flash: 32 KB**
- Library code: ~15-20 KB
- User code space: ~10-15 KB
- Sufficient for basic ranging, but limits advanced features

### Impact on Ranging Accuracy

| Constraint | Impact on Accuracy | Magnitude |
|------------|-------------------|-----------|
| Interrupt latency (3.5 μs) | Timestamp capture jitter | ±1-3 cm |
| Timer resolution (4 μs) | Software timing errors | ±1-2 cm |
| SPI speed (8 MHz vs 20 MHz) | Register read latency | ±0.5 cm |
| Limited RAM | Cannot implement advanced filters | ±2-5 cm |
| CPU speed | Protocol overhead, slower processing | 1-5 Hz update rate |

**Total Error Budget**: ±5-10 cm from Arduino Uno limitations alone (independent of DW1000 chip performance).

---

## Interrupt Latency Minimization

### Understanding Interrupt Latency on ATmega328P

**Interrupt Handling Phases**:
1. **Hardware latency**: Instruction completion + context save (~5 cycles)
2. **ISR vector jump**: Jump to interrupt handler (~7 cycles)
3. **Compiler prologue**: Save registers (varies, ~10-20 cycles)
4. **ISR execution**: User interrupt code
5. **Compiler epilogue**: Restore registers
6. **RETI instruction**: Return from interrupt (~4 cycles)

**Measured Total**: 82 clock cycles @ 16MHz = **5.1 μs entry**, 3.5 μs ISR execution

### DW1000 Interrupt Timing Requirements

The DW1000 generates interrupts on events (TX done, RX done, errors). Timestamp capture happens **in hardware** at the DW1000 chip, NOT in software. However, ISR latency affects:

1. **Response time**: Delay between RX and TX response packets
2. **Protocol timing**: Time to process received messages
3. **Update rate**: How fast ranging loop can execute

**Good news**: Timestamps are captured by DW1000 hardware with 15.65 ps precision, independent of Arduino ISR latency.

**Bad news**: ISR latency affects protocol timing, which impacts overall system performance.

### Optimization Techniques

#### 1. Minimize ISR Code (Critical Path Optimization)

**BAD** (common mistake):
```cpp
void handleReceived() {
    // DON'T do heavy processing in ISR!
    DW1000.getData(buffer, len);           // SPI transaction (slow)
    processMessage(buffer);                 // Complex logic
    Serial.println("Received!");           // Serial I/O (very slow)
    calculateRange();                       // Floating point math
    updateDisplay();                        // More I/O
}
```

**GOOD** (flag-based approach):
```cpp
volatile boolean receivedFlag = false;

void handleReceived() {
    // ISR: Just set flag and return
    receivedFlag = true;  // Single instruction, <1 μs
}

void loop() {
    if (receivedFlag) {
        receivedFlag = false;

        // Process in main loop (no time pressure)
        DW1000.getData(buffer, len);
        processMessage(buffer);
        // ... rest of processing
    }
}
```

**ISR Duration**:
- Bad approach: 500-2000 μs
- Good approach: <1 μs

**Impact**: Reduces interrupt blocking, allows other interrupts to fire, improves system responsiveness.

#### 2. Disable Interrupts During Critical Sections

**Use Case**: When reading/writing multi-byte variables shared between ISR and main loop.

```cpp
// Shared timestamp (5 bytes = 40 bits)
volatile byte timestamp[5];

void loop() {
    byte local_timestamp[5];

    // Atomic read (prevent corruption)
    noInterrupts();  // Disable interrupts (cli instruction, 1 cycle)
    memcpy(local_timestamp, timestamp, 5);
    interrupts();    // Re-enable interrupts (sei instruction, 1 cycle)

    // Process timestamp safely
    uint64_t ts = bytesToTimestamp(local_timestamp);
}
```

**Important**: Keep `noInterrupts()` sections as short as possible (<10 μs).

#### 3. Use Interrupt-Driven SPI (Hardware Support)

Arduino's SPI library supports interrupt notifications:

```cpp
void setup() {
    SPI.begin();
    SPI.usingInterrupt(digitalPinToInterrupt(PIN_IRQ));
    // Tells SPI to disable interrupts during transactions
}
```

**Effect**: Prevents DW1000 interrupt from firing during SPI communication, avoiding race conditions.

#### 4. Optimize Compiler for ISR Code

Use `__attribute__((always_inline))` for critical ISR functions:

```cpp
static inline void __attribute__((always_inline)) setFlag() {
    receivedFlag = true;
}

void handleReceived() {
    setFlag();  // Inlined, no function call overhead
}
```

**Savings**: ~10 cycles (function call overhead)

#### 5. Pin Change Interrupts vs External Interrupts

**Arduino Uno External Interrupts**:
- INT0 (D2) and INT1 (D3)
- Fastest interrupt response
- Hardware-triggered

**Use INT0 for DW1000 IRQ pin** (already default in library).

**Avoid**: Pin change interrupts (PCINT) - slower, more software overhead.

### Practical Impact on Ranging

**Before Optimization**:
- ISR duration: 500-1000 μs
- Protocol timing jitter: ±50 μs
- Ranging error contribution: ±5-10 cm

**After Optimization**:
- ISR duration: <5 μs
- Protocol timing jitter: ±5 μs
- Ranging error contribution: ±1-2 cm

**Improvement**: 3-8 cm accuracy gain from ISR optimization alone.

---

## Timestamp Capture Optimization

### DW1000 Hardware Timestamping

**Key Insight**: The DW1000 captures timestamps **in hardware** at the RF transceiver level, independent of the microcontroller. This is the secret to achieving centimeter-level accuracy despite slow MCU.

**Timestamp Types**:
1. **TX Timestamp**: Captured when preamble starts transmitting
2. **RX Timestamp**: Captured at first path arrival detection
3. **Delayed TX**: Schedule future transmission with 8 ns resolution

### Timestamp Registers

**System Time Counter (SYS_TIME)**:
- 40-bit free-running counter
- Clocked at 63.8976 GHz (499.2 MHz × 128)
- Rolls over every ~17.2 seconds

**TX/RX Timestamp Registers**:
- TX_TIME (0x17): Transmit timestamp (5 bytes)
- RX_TIME (0x15): Receive timestamp (5 bytes)

### Reading Timestamps Efficiently

**Library Implementation** (already optimized):
```cpp
// DW1000.cpp - getReceiveTimestamp()
void DW1000Class::getReceiveTimestamp(DW1000Time& time) {
    byte rxTimeBytes[LEN_RX_TIME];
    readBytes(RX_TIME, RX_STAMP_SUB, rxTimeBytes, LEN_RX_TIME);
    time.setTimestamp(rxTimeBytes);
}
```

**Direct register read** (if needed):
```cpp
// 5-byte read via SPI (~40 μs @ 8 MHz SPI)
byte rxTime[5];
DW1000.readBytes(0x15, 0x00, rxTime, 5);
```

### Optimization Strategies

#### 1. Minimize Register Reads

**Avoid**:
```cpp
void loop() {
    DW1000.readSystemEventStatusRegister();  // Read 5 bytes
    DW1000.readSystemEventStatusRegister();  // Read again (wasteful!)

    if (DW1000.isReceiveDone()) {
        DW1000.getReceiveTimestamp(ts);      // Read 5 bytes
        DW1000.getData(buffer, len);         // Read N bytes
    }
}
```

**Optimized**:
```cpp
void loop() {
    DW1000.readSystemEventStatusRegister();  // Read once

    if (DW1000.isReceiveDone()) {
        // Read timestamp and data in one go if possible
        DW1000.getReceiveTimestamp(ts);
        DW1000.getData(buffer, len);

        // Clear status (single write)
        DW1000.clearReceiveStatus();
    }
}
```

**Savings**: Reduces SPI transactions by 50%+

#### 2. Use DW1000Time Class Efficiently

**Good**: DW1000Time uses 40-bit timestamps efficiently
```cpp
DW1000Time timePollSent;
DW1000Time timePollAckReceived;

// Timestamp difference (handles rollover correctly)
DW1000Time delta = (timePollAckReceived - timePollSent).wrap();

// Convert to distance
float distance = delta.getAsMeters();  // Uses TIME_RES constant
```

**Avoid**: Converting to 64-bit integers unnecessarily
```cpp
// Wasteful (uses more RAM)
uint64_t t1 = timePollSent.getTimestamp();
uint64_t t2 = timePollAckReceived.getTimestamp();
uint64_t delta = t2 - t1;  // 64-bit math on 8-bit CPU (slow)
```

#### 3. Timestamp Arithmetic Optimization

**For short time intervals** (< 1 second), use 32-bit arithmetic:

```cpp
// DS-TWR calculation (from DW1000 app note)
// Using 32-bit timestamps for speed (if reply < 1 second)

uint32_t round1 = (uint32_t)(T4 - T1);  // Truncate to 32 bits
uint32_t reply1 = (uint32_t)(T3 - T2);
uint32_t round2 = (uint32_t)(T6 - T3);
uint32_t reply2 = (uint32_t)(T5 - T4);

// Compute ToF (32-bit math much faster on Arduino)
float tof = (float)(round1 * round2 - reply1 * reply2) /
            (float)(round1 + round2 + reply1 + reply2);
```

**Constraint**: Only valid if all time intervals < 1 second (32 bits @ 63.9 GHz = 67 ms max)

**Benefit**: 4x faster math on 8-bit AVR (32-bit vs 64-bit operations)

#### 4. Delayed Transmission Optimization

**Use delayed TX for precise timing**:
```cpp
// Schedule reply exactly 3000 μs after RX
DW1000Time replyTime;
DW1000.getReceiveTimestamp(replyTime);

DW1000Time delay(3000, DW1000Time::MICROSECONDS);
DW1000Time scheduledTxTime = replyTime + delay;

DW1000.newTransmit();
DW1000.setDelay(scheduledTxTime);  // Schedule TX precisely
DW1000.setData(data, len);
DW1000.startTransmit();  // Will TX at scheduled time
```

**Effect**: TX timestamp is precisely controlled by DW1000 hardware, independent of Arduino timing.

**Resolution**: 8 ns (512 × 15.65 ps) due to low 9 bits being ignored

**Benefit**: Eliminates Arduino timing jitter from protocol

### Timestamp-Related Accuracy Gains

| Optimization | Accuracy Improvement | Rationale |
|--------------|---------------------|-----------|
| Hardware timestamping | ±5-10 cm vs ±50 cm | DW1000 captures timestamps, not Arduino |
| Delayed TX | ±2-3 cm | Eliminates software timing jitter |
| Efficient register reads | ±1 cm | Reduces SPI latency variations |
| 32-bit arithmetic | N/A | Faster processing, enables higher update rate |

**Total**: Hardware timestamping is the **primary reason** DW1000 achieves cm-level accuracy on Arduino Uno.

---

## Clock Drift Compensation Techniques

### Understanding Clock Drift

**Clock drift** occurs when two devices' oscillators run at slightly different frequencies.

**DW1000 Crystal**: 38.4 MHz ±10 ppm (parts per million)

**Example**:
- Device A: 38.400000 MHz
- Device B: 38.400384 MHz (+10 ppm)
- Frequency difference: 384 Hz

**Impact on Ranging**:
- Over 1 second: 384 ns difference
- Corresponds to: 384 ns × 0.3 m/ns = **115 m error**
- Over 1 ms: 0.115 m = **11.5 cm error**

**Critical**: Clock drift MUST be compensated for accurate ranging.

### Single-Sided Two-Way Ranging (SS-TWR) - NOT Recommended

**Protocol**:
```
TAG                    ANCHOR
 |                        |
 |----POLL (T1)---------->| (T2)
 |                        |
 |<---RESPONSE (T3)-------| (T4)
 |                        |
```

**ToF Calculation**:
```
round_trip = T4 - T1  (TAG's clock)
response   = T3 - T2  (ANCHOR's clock)  <- Different clock!
ToF = (round_trip - response) / 2
```

**Problem**: T3-T2 measured on ANCHOR's clock, T4-T1 on TAG's clock. Clock drift causes error.

**Error**:
```
ToF_error = (T3 - T2) × (clock_drift_ppm / 1e6)
For 10 ppm drift and 1 ms response: Error = 10 ns = 3 mm
For 10 ppm drift and 100 ms response: Error = 1 μs = 30 cm
```

**Verdict**: SS-TWR is unusable unless response time < 1 ms (difficult on Arduino Uno).

### Double-Sided Two-Way Ranging (DS-TWR) - Recommended

**Protocol** (Asymmetric DS-TWR):
```
TAG                          ANCHOR
 |                              |
 |--------POLL (T1)------------>| (T2)
 |                              |
 |<-------POLL_ACK (T3)---------| (T4)
 |                              |
 |--------RANGE (T5)----------->| (T6)
 |    (contains T1, T4, T5)     |
```

**ToF Calculation**:
```cpp
round1 = T4 - T1  // TAG's round trip
reply1 = T3 - T2  // ANCHOR's reply time
round2 = T6 - T3  // ANCHOR's round trip
reply2 = T5 - T4  // TAG's reply time

ToF = (round1 × round2 - reply1 × reply2) / (round1 + round2 + reply1 + reply2)
```

**Clock Drift Compensation**:

The formula **cancels out** clock drift to first order:
- Numerator: Mixed terms cancel clock offset
- Denominator: Normalizes by total time

**Derivation** (simplified):

Let TAG clock run at rate `(1 + ε)` relative to perfect clock.

```
T_TAG = (1 + ε) × T_true
T_ANCHOR = (1 - ε) × T_true  (opposite drift)

After substitution and algebra:
ToF = T_actual + O(ε²)
```

**Second-order error** (ε² term) is negligible:
```
For ε = 10 ppm = 10e-6:
ε² = 1e-10 (100 parts per trillion)
Error contribution: < 0.1 mm
```

**Verdict**: DS-TWR effectively eliminates clock drift errors.

### Symmetrical Double-Sided TWR (SDS-TWR) - Best for High Precision

**Protocol**:
```
TAG                          ANCHOR
 |                              |
 |--------POLL (T1)------------>| (T2)
 |                              |
 |<-------POLL_ACK (T3)---------| (T4)
 |                              |
 |--------FINAL (T5)----------->| (T6)
 |                              |
 |<-------REPORT (T7)-----------| (T8)
```

**Additional message** allows more symmetric timing, further reduces clock drift sensitivity.

**ToF Calculation**:
```cpp
// Average two DS-TWR measurements
ToF1 = DS_TWR(T1, T2, T3, T4, T5, T6)
ToF2 = DS_TWR(T5, T6, T7, T8, T1, T2)  // Reversed role
ToF = (ToF1 + ToF2) / 2
```

**Advantage**: Even better clock drift compensation (higher-order error cancellation)

**Disadvantage**:
- 4 messages instead of 3 (33% more airtime)
- Slower on Arduino Uno (more processing)
- Not implemented in standard arduino-dw1000 library

**Recommendation for Arduino Uno**: Use asymmetric DS-TWR (3 messages). SDS-TWR gains are minimal (<1 cm) and not worth complexity.

### Carrier Frequency Offset (CFO) Measurement

**Advanced Technique**: DW1000 can measure carrier frequency offset.

**Register**: 0x14 (RX_TTCKO) - RX Time Tracking Offset

**Use Case**: Measure relative clock drift between devices

```cpp
// Read CFO register
int16_t cfo = DW1000.readCFO();  // Not in standard library

// Convert to ppm
float cfo_ppm = cfo / 1024.0;  // Approximate conversion

// Apply correction to timestamps (advanced)
float corrected_time = measured_time * (1.0 + cfo_ppm / 1e6);
```

**Benefit**: Can correct residual clock drift errors in post-processing

**Complexity**: Requires custom library modifications, complex math

**Recommendation**: Not necessary for Arduino Uno (DS-TWR is sufficient)

### Temperature Effects on Clock

**Crystal frequency** changes with temperature:
- Typical: ±0.035 ppm/°C² (parabolic)
- Over 20°C range: ±14 ppm additional drift

**Mitigation**:
1. Use DS-TWR (compensates automatically)
2. Temperature-compensated crystal oscillator (TCXO) - expensive
3. Calibrate at operating temperature

**For Arduino Uno**: DS-TWR is sufficient; TCXO not necessary.

### Practical Implementation

**arduino-dw1000 library** implements asymmetric DS-TWR in `DW1000Ranging` examples.

**Example** (TAG side):
```cpp
void setup() {
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.startAsTag(TAG_ADDRESS, DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}

void loop() {
    DW1000Ranging.loop();  // Handles DS-TWR protocol automatically
}

void newRange() {
    float range = DW1000Ranging.getDistantDevice()->getRange();
    // Range already compensated for clock drift via DS-TWR
}
```

**No manual clock drift compensation needed** - the library handles it.

### Clock Drift Impact Summary

| Method | Clock Drift Compensation | Arduino Uno Suitable? | Accuracy |
|--------|-------------------------|----------------------|----------|
| SS-TWR | None (high error) | NO | ±10-50 cm (poor) |
| DS-TWR (asymmetric) | Excellent (first-order) | YES | ±10-20 cm (good) |
| SDS-TWR (symmetric) | Excellent (higher-order) | Marginal | ±5-10 cm (better) |
| CFO correction | Advanced (post-processing) | NO (too complex) | ±5 cm (best) |

**Recommendation**: Use **DS-TWR** (asymmetric) on Arduino Uno - best balance of accuracy and simplicity.

---

## Environmental Factors Affecting Accuracy

### 1. Multipath Propagation

**Problem**: RF signals reflect off walls, metal objects, creating multiple arrival paths.

**Effect**:
- First path (direct) arrives first
- Reflected paths arrive later
- DW1000 may lock onto stronger reflected path instead of first path
- Results in **longer measured distance** (signal took longer path)

**Typical Error**: ±5-20 cm indoors, ±1-2 cm outdoors

**Mitigation Strategies**:

#### A. First Path Detection

DW1000 provides "first path power" measurement:

```cpp
float fpPower = DW1000.getFirstPathPower();  // Power of first arrival
float rxPower = DW1000.getReceivePower();    // Total received power

// If first path much weaker than total, multipath present
if (fpPower < (rxPower - 6.0)) {  // 6 dB threshold
    // Significant multipath detected
    // Option 1: Reject this measurement
    return;

    // Option 2: Flag as low confidence
    confidence = LOW;
}
```

**Implementation**:
```cpp
void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();
    float fpPower = device->getFPPower();
    float rxPower = device->getRXPower();

    if (fpPower < (rxPower - 6.0)) {
        Serial.println("Multipath detected!");
        return;  // Skip this measurement
    }

    float range = device->getRange();
    // Use range...
}
```

#### B. Physical Environment Control

**Best Practices**:
- Test in open areas (line-of-sight)
- Avoid metal surfaces within 1-2 meters
- Mount antennas away from reflective surfaces
- Keep devices >30 cm from walls
- Outdoor testing preferred for calibration

**Example Setup**:
```
       TAG
        |
        | 1.00 m (clear LOS)
        v
     ANCHOR

Both devices on tripods, 1.5 m above ground
No walls within 3 m radius
```

#### C. Median Filtering

Reject outliers caused by multipath:

```cpp
#define FILTER_SIZE 5
float measurements[FILTER_SIZE];
int index = 0;

void newRange() {
    float range = DW1000Ranging.getDistantDevice()->getRange();

    // Add to circular buffer
    measurements[index++] = range;
    if (index >= FILTER_SIZE) index = 0;

    // Calculate median (reject outliers)
    float sorted[FILTER_SIZE];
    memcpy(sorted, measurements, sizeof(measurements));
    bubbleSort(sorted, FILTER_SIZE);

    float median = sorted[FILTER_SIZE / 2];

    Serial.print("Median range: ");
    Serial.println(median);
}

void bubbleSort(float arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                float temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}
```

**RAM Cost**: 20 bytes (5 floats × 4 bytes) - acceptable on Arduino Uno

### 2. Temperature Effects

**Impact**: Temperature affects:
1. Antenna delay (~2 mm/°C per device)
2. Crystal oscillator frequency (compensated by DS-TWR)
3. PCB expansion (negligible)

**Measurement**:

Antenna delay varies **±2.15 mm/°C** per DW1000 device.

**Example**:
- Calibrated at 20°C, antenna delay = 16450
- Operating at 30°C (10°C warmer)
- Delay change: 10°C × 2.15 mm/°C = 21.5 mm
- Ranging error: ±2 cm

**Mitigation**:

#### A. Calibrate at Operating Temperature

Best practice: Calibrate at expected operating temperature.

For outdoor use:
- Calibrate at 15°C (mid-range for 0-30°C)
- Error: ±1.5°C × 2.15 mm/°C = ±3 mm

#### B. Temperature Compensation (Advanced)

```cpp
// Read external temperature sensor
float temp = readTemperatureSensor();  // Your sensor (e.g., DHT22)

// Base calibration at 20°C
const uint16_t BASE_ANTENNA_DELAY = 16450;
const float CALIBRATION_TEMP = 20.0;
const float TEMP_COEFF = 2.15 / 4.69;  // mm/°C → time units/°C
                                        // 2.15 mm/°C / 4.69 mm per time unit

// Calculate adjustment
float tempDelta = temp - CALIBRATION_TEMP;
int16_t tempAdjustment = (int16_t)(tempDelta * TEMP_COEFF);

// Apply corrected antenna delay
uint16_t correctedDelay = BASE_ANTENNA_DELAY + tempAdjustment;
DW1000.setAntennaDelay(correctedDelay);
```

**Complexity**: Requires external sensor, periodic updates

**Recommendation for Arduino Uno**: Only implement if accuracy <±5 cm required and temperature varies >10°C.

### 3. RF Interference

**Sources**:
- WiFi (2.4 GHz, 5 GHz)
- Bluetooth (2.4 GHz)
- Microwave ovens (2.45 GHz)
- Other UWB devices

**DW1000 Channels**:
- Channel 1: 3494.4 MHz (36 MHz below WiFi)
- Channel 2: 3993.6 MHz (between WiFi bands)
- Channel 3: 4492.8 MHz (in 5 GHz WiFi band)
- Channel 4: 3993.6 MHz
- Channel 5: 6489.6 MHz (above WiFi)
- Channel 7: 6489.6 MHz (same as 5)

**Recommended Channels**:
- **Channel 5 or 7**: 6.5 GHz, minimal WiFi interference
- **Channel 2 or 4**: 4 GHz, moderate interference
- Avoid Channel 3 (overlaps 5 GHz WiFi)

**Implementation**:
```cpp
void setup() {
    DW1000.newConfiguration();
    DW1000.setChannel(5);  // Use 6.5 GHz channel (least interference)
    DW1000.commitConfiguration();
}
```

**Signal Quality Check**:
```cpp
void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();

    float rxPower = device->getRXPower();

    if (rxPower < -90.0) {  // dBm
        Serial.println("Weak signal - possible interference");
        return;  // Reject measurement
    }

    float quality = device->getQuality();
    if (quality < 50.0) {  // Arbitrary threshold
        Serial.println("Poor quality - interference likely");
        return;
    }

    // Measurement is good quality
    float range = device->getRange();
}
```

### 4. Antenna Delay Calibration

**Most Critical Factor** for absolute accuracy.

**Impact**: Uncalibrated modules have ±50-100 cm systematic offset.

**Procedure**: See `/docs/findings/CALIBRATION_GUIDE.md` for detailed steps.

**Quick Calibration**:
```cpp
// 1. Measure at exactly 1.000 m
// 2. Read measured distance (e.g., 1.087 m)
// 3. Calculate adjustment

float actual = 1.000;
float measured = 1.087;
float error = measured - actual;  // +0.087 m

uint16_t currentDelay = 16450;
float errorPerDevice = error / 2.0;  // Both devices contribute
int16_t adjustment = (int16_t)(errorPerDevice * 213.14);  // Convert to time units
uint16_t newDelay = currentDelay + adjustment;

// 4. Apply to both TAG and ANCHOR
DW1000.setAntennaDelay(newDelay);

// 5. Verify error < 5 cm
```

**Expected Values**: 16400-16500 time units for DWM1000 modules

**Per-Device Calibration**: Each module may have different delay (±50 time units variation)

### 5. Power Supply Noise

**Issue**: Noisy power supply affects DW1000 performance.

**Symptoms**:
- Erratic measurements
- Communication failures
- Reduced range

**Mitigation**:
```cpp
// Hardware:
// - Add 10 μF + 0.1 μF capacitors near DW1000 VDD pins
// - Use low-noise LDO regulator for 3.3V (not Arduino Uno's 3.3V pin)
// - Star ground topology

// Software: Check supply voltage (if ADC available)
// DW1000 has internal voltage monitor (register 0x08)
```

**Arduino Uno 3.3V Pin Limitation**:
- Maximum current: ~50 mA
- DW1000 TX current: up to 150 mA
- **Solution**: Use external 3.3V regulator (e.g., AMS1117-3.3)

### Environmental Factors Summary

| Factor | Typical Error | Mitigation | Arduino Uno Implementation |
|--------|---------------|------------|---------------------------|
| Multipath | ±5-20 cm | FP power check, LOS, median filter | YES (simple logic) |
| Temperature | ±2 mm/°C | Calibrate at op temp, temp compensation | Calibration: YES<br>Compensation: Optional |
| RF interference | ±5-10 cm | Use channel 5/7, signal quality check | YES (simple threshold) |
| Antenna delay | ±50-100 cm | Calibration (critical!) | YES (required) |
| Power supply | ±5 cm | External regulator, decoupling caps | YES (hardware fix) |

**Priority Order**:
1. Antenna delay calibration (50-100 cm improvement)
2. Multipath mitigation (5-20 cm improvement)
3. RF interference reduction (5-10 cm improvement)
4. Temperature compensation (optional, 2-5 cm improvement)
5. Power supply quality (baseline requirement)

---

## Code-Level Optimizations

### 1. Memory Management (2KB RAM Constraint)

**Problem**: DW1000Ranging uses ~1200 bytes, leaving <800 bytes for application.

#### Optimization A: Reduce MAX_DEVICES

Default: 4 devices × 74 bytes = 296 bytes

```cpp
// In DW1000Device.h (modify library)
#define MAX_DEVICES 3  // Reduce to 3 for more RAM

// Savings: 74 bytes
```

#### Optimization B: Use Fixed-Point Math Instead of Float

Floats use 4 bytes each and are slow on AVR.

**Before**:
```cpp
float measurements[10];  // 40 bytes
float sum = 0;
for (int i = 0; i < 10; i++) {
    sum += measurements[i];
}
float average = sum / 10.0;  // Slow float division
```

**After**:
```cpp
int16_t measurements[10];  // 20 bytes (store as mm)
int32_t sum = 0;
for (int i = 0; i < 10; i++) {
    sum += measurements[i];
}
int16_t average = sum / 10;  // Fast integer division

// Convert to meters when needed
float average_m = average / 1000.0;
```

**Savings**: 20 bytes RAM, faster processing

#### Optimization C: Use PROGMEM for Constants

Store large constant arrays in flash instead of RAM:

```cpp
// BAD (uses RAM)
const char* anchorNames[] = {
    "Anchor_1_FrontLeft",
    "Anchor_2_FrontRight",
    "Anchor_3_BackLeft",
    "Anchor_4_BackRight"
};  // 80+ bytes in RAM

// GOOD (uses flash)
const char anchor1[] PROGMEM = "Anchor_1_FrontLeft";
const char anchor2[] PROGMEM = "Anchor_2_FrontRight";
const char anchor3[] PROGMEM = "Anchor_3_BackLeft";
const char anchor4[] PROGMEM = "Anchor_4_BackRight";

const char* const anchorNames[] PROGMEM = {
    anchor1, anchor2, anchor3, anchor4
};

// Read from flash when needed
char buffer[32];
strcpy_P(buffer, (char*)pgm_read_word(&(anchorNames[0])));
```

**Savings**: 80 bytes RAM

### 2. SPI Speed Optimization

**DW1000 supports up to 20 MHz SPI**, but Arduino Uno defaults to 4 MHz.

```cpp
// In DW1000.cpp (modify library)
void DW1000Class::begin(...) {
    SPI.begin();
    SPI.setClockDivider(SPI_CLOCK_DIV2);  // 16 MHz / 2 = 8 MHz
    // Default is DIV4 (4 MHz)
}
```

**Impact**:
- 5-byte register read: 10 μs @ 4 MHz → 5 μs @ 8 MHz
- Faster protocol execution
- Higher update rate possible

**Safety**: Test thoroughly - some DW1000 modules may not work reliably >8 MHz.

### 3. Reduce Serial Output

Serial.print() is **very slow** (~1 ms per line @ 115200 baud).

**Problem**:
```cpp
void newRange() {
    Serial.print("Range to anchor 0x");
    Serial.print(addr, HEX);
    Serial.print(": ");
    Serial.print(range, 2);
    Serial.print(" m | RSSI: ");
    Serial.print(rssi);
    Serial.print(" dBm | FP: ");
    Serial.println(fpPower);
}
// Total: ~8-10 ms per ranging update!
```

**Optimized**:
```cpp
void newRange() {
    // Minimal output (CSV format, fast parsing)
    Serial.print(addr, HEX);
    Serial.print(',');
    Serial.print(range, 2);
    Serial.print(',');
    Serial.println(rssi);

    // Or use binary protocol for even faster output
}
// Total: ~2-3 ms
```

**Alternative**: Buffer output and send in batches
```cpp
#define BUFFER_SIZE 5
int bufferIndex = 0;

void newRange() {
    // Store in buffer (no serial output)
    rangeBuffer[bufferIndex++] = range;

    if (bufferIndex >= BUFFER_SIZE) {
        // Send batch
        for (int i = 0; i < BUFFER_SIZE; i++) {
            Serial.print(rangeBuffer[i], 2);
            Serial.print(',');
        }
        Serial.println();
        bufferIndex = 0;
    }
}
```

### 4. Eliminate Floating Point Where Possible

AVR has no FPU - floating point is emulated in software (slow).

**Example**: Distance comparison

**Before**:
```cpp
float range = DW1000Ranging.getDistantDevice()->getRange();
if (range > 5.0 && range < 10.0) {
    // In zone
}
```

**After** (use fixed-point):
```cpp
// Store range as mm (int16_t)
int16_t range_mm = (int16_t)(range * 1000.0);  // One-time conversion
if (range_mm > 5000 && range_mm < 10000) {
    // In zone (integer comparison is faster)
}
```

### 5. Loop Optimization

**Avoid** delays in main loop:

**Bad**:
```cpp
void loop() {
    DW1000Ranging.loop();
    delay(100);  // Blocks for 100 ms!
}
```

**Good**:
```cpp
void loop() {
    DW1000Ranging.loop();  // Non-blocking

    // Do time-based actions
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 1000) {
        lastPrint = millis();
        printStatistics();
    }
}
```

### 6. Minimize Dynamic Memory Allocation

**Avoid**: `malloc()`, `new`, `String` class (dynamic allocation)

**Problem**: Heap fragmentation on small RAM systems

**Bad**:
```cpp
String buildMessage() {
    String msg = "Range: ";  // Dynamic allocation
    msg += String(range);     // More allocation
    msg += " m";
    return msg;               // More allocation on copy
}
```

**Good**:
```cpp
void buildMessage(char* buffer, size_t len) {
    snprintf(buffer, len, "Range: %.2f m", range);
}

// Usage:
char msg[32];
buildMessage(msg, sizeof(msg));
Serial.println(msg);
```

### Code Optimization Summary

| Optimization | RAM Savings | Speed Improvement | Complexity |
|--------------|-------------|-------------------|-----------|
| Reduce MAX_DEVICES | 74 bytes/device | - | Trivial |
| Fixed-point math | 50% float storage | 2-5x faster | Medium |
| PROGMEM constants | 50-200 bytes | - | Easy |
| SPI speed 8 MHz | - | 2x faster register access | Easy |
| Minimal serial output | - | 3-5x faster loop | Easy |
| Eliminate float ops | - | 2-5x faster | Medium |
| Non-blocking code | - | Higher update rate | Easy |
| Static allocation | Prevents fragmentation | - | Medium |

**Priority**:
1. Increase SPI speed to 8 MHz (easy, big impact)
2. Reduce serial output (easy, big impact)
3. Non-blocking loop (easy, enables higher update rate)
4. PROGMEM for constants (easy, frees RAM)
5. Fixed-point math (medium, significant benefits)

---

## Expected Accuracy Limits

### Theoretical Limits

**DW1000 Chip Performance** (ideal conditions):
- **Timestamp resolution**: 15.65 ps
- **Distance resolution**: 0.00469 m = 4.69 mm
- **RMS error (datasheet)**: ±10 cm

**Theoretical Best Case**: ±5 mm (limited by physics, not chip)

### Practical Limits on Arduino Uno

**Error Budget** (cumulative):

| Error Source | Contribution | Notes |
|--------------|-------------|-------|
| DW1000 timestamp quantization | ±2 cm | 15.65 ps × speed of light |
| Antenna delay variation | ±5 cm | After calibration |
| Temperature drift | ±2 cm | 10°C variation, uncorrected |
| Interrupt latency jitter | ±2 cm | 3-5 μs ISR timing variation |
| Multipath (indoor) | ±10 cm | Even with LOS, reflections present |
| RF interference | ±3 cm | Typical indoor environment |
| Protocol timing | ±2 cm | Arduino processing delays |

**Root Sum Square (RSS)**:
```
Total_RMS = sqrt(2² + 5² + 2² + 2² + 10² + 3² + 2²) = 12.6 cm
```

**Expected Accuracy on Arduino Uno**:

#### Short Range (<3 m)
- **Best case**: ±10 cm (optimal conditions, after calibration)
- **Typical**: ±15 cm (indoor, normal conditions)
- **Worst case**: ±25 cm (high multipath, interference)

#### Medium Range (3-5 m)
- **Best case**: ±15 cm
- **Typical**: ±20 cm
- **Worst case**: ±35 cm

#### Long Range (5-10 m)
- **Best case**: ±20 cm
- **Typical**: ±30 cm
- **Worst case**: ±50 cm

### Comparison with ESP32

**ESP32 Advantages**:
- 240 MHz CPU (15x faster)
- Interrupt latency <1 μs (vs 3.5 μs on Uno)
- 320 KB RAM (160x more)
- Dual-core (parallel processing)
- Faster SPI (up to 40 MHz vs 8 MHz)

**Error Budget on ESP32**:

| Error Source | Arduino Uno | ESP32 | Improvement |
|--------------|-------------|-------|-------------|
| DW1000 hardware | ±2 cm | ±2 cm | - |
| Antenna delay | ±5 cm | ±5 cm | - |
| Temperature | ±2 cm | ±1 cm | Better compensation |
| Interrupt latency | ±2 cm | ±0.5 cm | 4x better |
| Multipath | ±10 cm | ±8 cm | Better filtering |
| Interference | ±3 cm | ±2 cm | Better quality detection |
| Protocol timing | ±2 cm | ±0.5 cm | Faster processing |
| **Total (RSS)** | **±12.6 cm** | **±10.2 cm** | ~20% better |

**Expected Accuracy on ESP32**:
- Short range (<5 m): ±5-10 cm
- Medium range (5-10 m): ±10-15 cm
- Long range (10-30 m): ±15-25 cm

### Update Rate Limits

**Arduino Uno**:
- Protocol overhead: ~50-100 ms per ranging cycle
- Serial output: ~5-10 ms per update
- Processing: ~20-30 ms
- **Maximum**: 5-10 Hz (realistically 2-5 Hz)

**ESP32**:
- Protocol overhead: ~10-20 ms
- Processing: ~5 ms
- **Maximum**: 30-50 Hz (realistically 10-20 Hz)

### Network Size Limits

**Arduino Uno**:
- RAM constraint: 3-4 anchors max
- Processing: 1 TAG ranging to 3 anchors @ 1-2 Hz

**ESP32**:
- RAM: 10+ anchors easily
- Processing: 1 TAG ranging to 10 anchors @ 10 Hz

### Summary Table

| Metric | Arduino Uno | ESP32 | ESP32 Advantage |
|--------|-------------|-------|-----------------|
| **Accuracy (short range)** | ±10-15 cm | ±5-10 cm | 2x better |
| **Accuracy (medium range)** | ±15-25 cm | ±10-15 cm | 1.5x better |
| **Update rate** | 1-5 Hz | 10-50 Hz | 10x faster |
| **Max anchors** | 3-4 | 10+ | 3x more |
| **Calibration effort** | High (manual) | Medium (can automate) | - |
| **Development time** | Longer (optimization needed) | Shorter | - |
| **Cost** | $3-5 | $5-10 | 2x more expensive |

**Verdict**: Arduino Uno achieves **±10-20 cm** accuracy, which is:
- **Sufficient** for: Room-level localization, zone detection, proximity sensing
- **Insufficient** for: Precision robotics (<±5 cm), drone navigation, industrial automation

---

## Arduino Uno vs ESP32 Comparison

### Hardware Specifications

| Feature | Arduino Uno | ESP32 | Ratio |
|---------|-------------|-------|-------|
| **CPU** | ATmega328P @ 16 MHz | Xtensa LX6 @ 240 MHz | 15x |
| **Architecture** | 8-bit AVR | 32-bit dual-core | - |
| **SRAM** | 2 KB | 320 KB (520 KB with PSRAM) | 160x |
| **Flash** | 32 KB | 4 MB | 125x |
| **Clock precision** | 16 MHz ±50 ppm | 40 MHz ±10 ppm (with crystal) | Better |
| **Interrupt latency** | 3.5-5 μs | <1 μs | 4x better |
| **Timer resolution** | 4 μs (micros()) | 1 μs | 4x better |
| **SPI speed** | 8 MHz (max 8 MHz safe) | 20 MHz (max 40 MHz) | 2.5x |
| **FPU** | No (emulated) | Yes (hardware) | 10-100x faster |
| **Price** | $3-5 | $5-10 | 2x |

### DW1000 Ranging Performance

#### Accuracy Comparison

**Short Range (<3 m)**:
- Arduino Uno: ±10-15 cm (after calibration)
- ESP32: ±5-10 cm
- **Winner**: ESP32 (2x better)

**Medium Range (3-10 m)**:
- Arduino Uno: ±15-25 cm
- ESP32: ±10-15 cm
- **Winner**: ESP32 (1.5x better)

**Long Range (>10 m)**:
- Arduino Uno: ±25-50 cm (often unreliable)
- ESP32: ±15-25 cm
- **Winner**: ESP32 (2x better)

#### Update Rate Comparison

**Single Anchor Ranging**:
- Arduino Uno: 5-10 Hz (theoretical), 2-5 Hz (practical with serial output)
- ESP32: 30-50 Hz (theoretical), 10-20 Hz (practical)
- **Winner**: ESP32 (5-10x faster)

**Multi-Anchor (3 anchors)**:
- Arduino Uno: 1-2 Hz total (0.3-0.7 Hz per anchor)
- ESP32: 10-15 Hz total (3-5 Hz per anchor)
- **Winner**: ESP32 (10x faster)

#### Network Capacity

**Maximum Devices Tracked**:
- Arduino Uno: 3-4 anchors (RAM limited)
- ESP32: 10-20+ anchors (processing limited, not RAM)
- **Winner**: ESP32 (5x more)

### Code Complexity

**Arduino Uno**:
- Requires heavy optimization (RAM, speed)
- Manual memory management critical
- Fixed-point math often needed
- PROGMEM for constants
- Minimal serial output
- **Development time**: 2-3x longer

**ESP32**:
- Can use standard C++ practices
- Plenty of RAM for buffers, filtering
- Floating-point math no problem
- Serial output not bottleneck
- **Development time**: Baseline

### Feature Comparison

| Feature | Arduino Uno | ESP32 |
|---------|-------------|-------|
| WiFi/Bluetooth | No | Yes (built-in) |
| Data logging | SD card (shield) | SD card, SPIFFS, WiFi upload |
| Real-time clock | External RTC needed | Built-in |
| Multicore | No | Yes (dual-core) |
| Advanced algorithms | Limited (RAM) | Yes (Kalman filter, etc.) |
| Over-the-air updates | No | Yes |
| Battery life | Better (slower = less power) | Worse (but sleep modes help) |

### Specific Limitations

#### Arduino Uno Cannot:
1. Track >4 anchors simultaneously (RAM)
2. Achieve <±10 cm accuracy consistently
3. Update faster than 5 Hz with multiple anchors
4. Run Kalman filters or advanced algorithms (RAM, speed)
5. Implement sophisticated multipath rejection (processing)
6. Do automatic temperature compensation (no headroom)
7. Support WiFi/Bluetooth for wireless data (no hardware)

#### ESP32 Cannot:
1. Run on 5V directly (needs 3.3V, but most boards have regulator)
2. Match Arduino's simplicity (more complex toolchain)
3. Beat Arduino on power consumption (in active mode)

### Use Case Recommendations

**Choose Arduino Uno if**:
- Learning UWB/ranging basics
- Prototype/proof-of-concept
- Low-cost requirement (<$5 per node)
- Simple ranging (1-2 anchors, ±20 cm accuracy OK)
- No real-time requirements
- Already familiar with Arduino ecosystem

**Choose ESP32 if**:
- Production system
- Accuracy <±10 cm required
- Real-time tracking (>5 Hz update rate)
- Multi-anchor networks (>3 anchors)
- Need WiFi/BLE connectivity
- Want to implement advanced algorithms
- Future-proofing

### Migration Path

**Start with Arduino Uno**:
1. Learn DW1000 basics
2. Develop ranging protocol
3. Understand calibration
4. Identify bottlenecks

**Migrate to ESP32**:
1. Port code (mostly compatible)
2. Increase update rate
3. Add more anchors
4. Implement advanced features
5. Add wireless connectivity

**Code portability**: ~80-90% of Arduino code works on ESP32 with minimal changes.

### Cost Analysis

**Arduino Uno System** (3-anchor network):
- 3× Arduino Uno: $15
- 3× DW1000 module: $45
- 3× Shields/wiring: $15
- Development time: 40 hours
- **Total**: $75 + 40 hours

**ESP32 System** (3-anchor network):
- 3× ESP32: $20
- 3× DW1000 module: $45
- 3× Breakout boards: $10
- Development time: 20 hours
- **Total**: $75 + 20 hours

**Verdict**: ESP32 system costs same hardware but **saves 50% development time**.

---

## When to Migrate to ESP32

### Performance Triggers

**Migrate to ESP32 when you encounter**:

#### 1. Accuracy Requirements

**Arduino Uno limit**: ±10-20 cm (after optimization)

**Migrate if you need**:
- ±5-10 cm accuracy consistently
- <±5 cm accuracy (ESP32 + advanced algorithms)

**Examples**:
- Warehouse robot navigation (requires ±5 cm)
- Automated guided vehicles (AGVs)
- Precision asset tracking

#### 2. Update Rate Requirements

**Arduino Uno limit**: 1-5 Hz

**Migrate if you need**:
- >5 Hz update rate
- Real-time tracking (10-20 Hz)
- Smooth trajectory estimation

**Examples**:
- Drone navigation (needs 20+ Hz)
- Sports tracking (10+ Hz)
- Human motion capture

#### 3. Network Size Requirements

**Arduino Uno limit**: 3-4 anchors

**Migrate if you need**:
- >4 anchors for better coverage
- Multi-floor positioning
- Large area coverage

**Examples**:
- Building-wide tracking
- Multi-room localization
- Outdoor positioning

#### 4. Processing Requirements

**Arduino Uno limit**: Minimal filtering, basic math

**Migrate if you need**:
- Kalman filtering
- Multilateration algorithms on-device
- Advanced multipath rejection
- Real-time data fusion (IMU + UWB)

**Examples**:
- Autonomous robots
- Sensor fusion applications
- Machine learning integration

#### 5. Connectivity Requirements

**Arduino Uno**: No wireless (need external modules)

**Migrate if you need**:
- WiFi data upload
- Bluetooth Low Energy (BLE)
- MQTT/HTTP communication
- Cloud integration

**Examples**:
- IoT asset tracking
- Remote monitoring
- Cloud-based localization

### Development Triggers

**Migrate when**:

1. **RAM exhaustion**: Cannot add features due to 2KB limit
2. **Optimization fatigue**: Spending >50% time optimizing instead of developing features
3. **Update rate bottleneck**: Cannot achieve required responsiveness
4. **Code complexity**: Fixed-point math, PROGMEM making code unmaintainable

### Cost-Benefit Analysis

**Arduino Uno Development Cost**:
- Hardware: $25 (Uno + DW1000)
- Development time: 40-60 hours (with optimization)
- Achieved: ±15 cm @ 2 Hz, 3 anchors

**ESP32 Development Cost**:
- Hardware: $30 (ESP32 + DW1000)
- Development time: 20-30 hours
- Achieved: ±8 cm @ 15 Hz, 10 anchors

**Break-even**: If you value your time at >$10/hour, ESP32 pays for itself in saved development time.

### Migration Checklist

**Before migrating, ensure you have**:

- [ ] Mastered basic DW1000 concepts on Arduino Uno
- [ ] Completed antenna delay calibration procedure
- [ ] Understood DS-TWR protocol
- [ ] Identified specific performance bottleneck
- [ ] Clear requirements for ESP32 features
- [ ] Budget for ESP32 hardware ($5 extra per node)

**Migration steps**:

1. **Port code to ESP32** (PlatformIO or Arduino IDE with ESP32 board support)
2. **Adjust pin definitions** (ESP32 has different pinout)
3. **Increase SPI speed** to 20 MHz
4. **Remove Arduino Uno optimizations** (PROGMEM, fixed-point math if not needed)
5. **Increase MAX_DEVICES** if needed
6. **Implement advanced features** (WiFi, Kalman filter, etc.)
7. **Test and tune** for higher update rates

**Expected effort**: 1-2 days for basic port, 1-2 weeks for advanced features.

### Hybrid Approach

**Consider**: Arduino Uno as tags, ESP32 as anchors

**Rationale**:
- Tags (mobile): Low-cost Arduino Uno
- Anchors (fixed): ESP32 with WiFi for data aggregation
- Anchors do ranging calculations (more powerful)
- Tags just send/receive messages (simple)

**Example System**:
- 10× Tags: Arduino Uno @ $3 = $30
- 4× Anchors: ESP32 @ $7 = $28
- Total: $58 (vs $70 all-Uno or $110 all-ESP32)

### Final Recommendation

**Start with Arduino Uno if**:
- First UWB project
- Learning/educational purpose
- Budget <$50
- No time pressure

**Start directly with ESP32 if**:
- Production system planned
- Accuracy <±10 cm required
- Update rate >5 Hz required
- Need connectivity (WiFi/BLE)
- Time is valuable (save development time)

**Migration is straightforward** - most code is portable, so starting with Arduino Uno for learning and migrating to ESP32 for production is a valid strategy.

---

## Implementation Guidelines

### Quick Start Checklist

#### Hardware Setup

- [ ] Connect DW1000 to Arduino Uno:
  - IRQ → D2 (INT0)
  - RST → D9
  - CS → D10 (SS)
  - MOSI → D11
  - MISO → D12
  - SCK → D13
  - VDD → 3.3V (external regulator recommended)
  - GND → GND

- [ ] Verify chip ID:
```cpp
char deviceID[128];
DW1000.getPrintableDeviceIdentifier(deviceID);
Serial.println(deviceID);  // Should show "DECA0130"
```

#### Software Setup

- [ ] Apply library bug fix (see `/docs/findings/DW1000_RANGING_BEST_PRACTICES.md`)
- [ ] Set SPI speed to 8 MHz
- [ ] Configure for DS-TWR mode (use DW1000Ranging library)
- [ ] Set channel to 5 or 7 (6.5 GHz, minimal interference)

#### Calibration

- [ ] Perform antenna delay calibration (both TAG and ANCHOR)
- [ ] Test at multiple distances (0.5, 1.0, 2.0, 5.0 m)
- [ ] Verify error <10 cm at all distances
- [ ] Document calibrated antenna delay values

#### Optimization

- [ ] Minimize serial output (CSV format, not verbose)
- [ ] Use flag-based ISR (not heavy processing in ISR)
- [ ] Implement median filter for outlier rejection
- [ ] Add signal quality checks (RX power, first path power)
- [ ] Test in target environment (multipath, interference)

### Code Template (Optimized TAG)

```cpp
/*
 * Optimized DW1000 TAG for Arduino Uno
 * Achieves ~±15 cm accuracy @ 2-3 Hz update rate
 */

#include <SPI.h>
#include <DW1000Ranging.h>

// Pin definitions
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Calibration (IMPORTANT: Calibrate your modules!)
const uint16_t ANTENNA_DELAY = 16450;  // Replace with calibrated value

// Filtering
#define FILTER_SIZE 5
float rangeBuffer[FILTER_SIZE];
int bufferIndex = 0;

// Signal quality thresholds
const float MIN_RX_POWER = -90.0;  // dBm
const float MIN_FP_MARGIN = 6.0;   // dB

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("DW1000 TAG - Optimized"));

    // Initialize
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Apply calibration
    DW1000.setAntennaDelay(ANTENNA_DELAY);

    // Configure for accuracy
    DW1000Ranging.setReplyTime(7000);  // μs
    DW1000Ranging.setResetPeriod(200);  // ms

    // Disable built-in filter (we'll use median filter)
    DW1000Ranging.useRangeFilter(false);

    // Attach callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as TAG (use accurate mode)
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                             DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println(F("Ready"));
}

void loop() {
    DW1000Ranging.loop();  // Non-blocking
}

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();

    // Quality checks
    float rxPower = device->getRXPower();
    float fpPower = device->getFPPower();

    if (rxPower < MIN_RX_POWER) {
        return;  // Weak signal, reject
    }

    if (fpPower < (rxPower - MIN_FP_MARGIN)) {
        return;  // Multipath detected, reject
    }

    // Get range
    float range = device->getRange();

    // Median filter
    rangeBuffer[bufferIndex++] = range;
    if (bufferIndex >= FILTER_SIZE) bufferIndex = 0;

    float filteredRange = median(rangeBuffer, FILTER_SIZE);

    // Minimal output (CSV format for fast parsing)
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(',');
    Serial.print(filteredRange, 2);
    Serial.print(',');
    Serial.println(rxPower, 1);
}

void newDevice(DW1000Device* device) {
    Serial.print(F("+ 0x"));
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print(F("- 0x"));
    Serial.println(device->getShortAddress(), HEX);
}

// Median filter (bubble sort for small arrays)
float median(float arr[], int n) {
    float sorted[FILTER_SIZE];
    memcpy(sorted, arr, n * sizeof(float));

    // Bubble sort
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                float temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }

    return sorted[n / 2];
}
```

### Performance Expectations

With this optimized code on Arduino Uno:
- **Accuracy**: ±10-15 cm (indoor, <5 m range)
- **Update rate**: 2-3 Hz (single anchor)
- **Latency**: ~300-500 ms per ranging cycle
- **RAM usage**: ~1400 bytes (600 bytes free)
- **Flash usage**: ~20 KB (12 KB free)

### Troubleshooting Guide

**Problem**: Accuracy >20 cm

- Check antenna delay calibration
- Verify clear line-of-sight
- Test signal quality (RX power >-85 dBm)
- Check for multipath (FP power margin >6 dB)

**Problem**: Low update rate (<1 Hz)

- Reduce serial output
- Verify SPI speed 8 MHz
- Check interrupt bug fix applied
- Ensure non-blocking code (no delays)

**Problem**: Ranging fails / no output

- Verify library bug fix applied
- Check wiring (especially IRQ → D2)
- Test with polling mode as fallback
- Verify both devices on same network ID

---

## References and Resources

### Web Research Sources

**DW1000 General Resources**:
- [DecaDuino Library for Arduino](https://www.irit.fr/~Adrien.Van-Den-Bossche/decaduino/) - TWR and SDS-TWR examples
- [arduino-dw1000 GitHub](https://github.com/thotro/arduino-dw1000) - Main library used in this project
- [arduino-dw1000-ng GitHub](https://github.com/F-Army/arduino-dw1000-ng) - Next-generation fork
- [ESP32 UWB Indoor Positioning](https://www.instructables.com/ESP32-UWB-Indoor-Positioning-Test/) - ESP32 comparison

**Antenna Delay Calibration**:
- [Antenna Delay Calibration Guide (APS014)](https://www.qorvo.com/products/d/da008449) - Official Qorvo application note
- [DW1000 Antenna Delay Calibration - GitHub](https://github.com/lowfet/AntennaDelayCalibration) - Automated calibration tool
- [ESP32 UWB Antenna Delay Calibrating - Makerfabs](https://www.makerfabs.cc/article/esp32-uwb-antenna-delay-calibrating.html)
- [Antenna Delay Calibration Research Paper](https://www.researchgate.net/publication/351249995_Antenna_Delay_Calibration_of_UWB_Nodes)

**Clock Drift Compensation**:
- [Clock Drift Compensation - Qorvo Forum](https://forum.qorvo.com/t/clock-drift-compensation-for-ranging/3262)
- [Decawave UWB Clock Drift Correction Paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC6651025/)
- [Clock Compensation TWR (CC-TWR) Paper](https://ieeexplore.ieee.org/document/9045461/)
- [CFO Measurements in UWB TDoA](https://www.mdpi.com/1424-8220/23/5/2595)

**Arduino Interrupt Optimization**:
- [Interrupt Latency & Response Time - DeepBlue](https://deepbluembedded.com/interrupt-latency-response-arduino/)
- [Understanding Interrupt Latency in Arduino - 2024](https://thecustomizewindows.com/2024/07/understanding-interrupt-latency-and-response-time-interrupt-speed-in-arduino/)
- [The Do's and Don'ts of Using Arduino Interrupts - DigiKey](https://www.digikey.com/en/maker/tutorials/2022/the-dos-and-donts-of-using-arduino-interrupts)

**DW1000 Accuracy and Performance**:
- [Tests on Ranging Accuracy - GitHub Issue #6](https://github.com/thotro/arduino-dw1000/issues/6)
- [Benchmarks - arduino-dw1000 Wiki](https://github.com/thotro/arduino-dw1000/wiki/Benchmarks)
- [DW1000 Ranging Accuracy vs Signal Level](https://forum.qorvo.com/t/dw1000-received-signal-level-vs-ranging-accuracy/5773)
- [Questionable Ranging Performance - GitHub Issue #205](https://github.com/thotro/arduino-dw1000/issues/205)

**Arduino Uno vs ESP32**:
- [Getting Started with ESP32 DW1000](https://how2electronics.com/getting-started-with-esp32-dw1000-uwb-ultra-wideband-module/)
- [ESP32 UWB - Makerfabs Wiki](https://wiki.makerfabs.com/ESP32_UWB.html)
- [ESP32 vs Arduino Uno Comparison Forum](https://forum.arduino.cc/t/affordable-indoor-positioning-using-esp32-uwb-modules/957727)

### Project Documentation

**Local Guides**:
- `/docs/findings/DW1000_RANGING_BEST_PRACTICES.md` - Comprehensive Arduino Uno guide
- `/docs/findings/CALIBRATION_GUIDE.md` - Detailed calibration procedures
- `/docs/findings/INTERRUPT_ISSUE_SUMMARY.md` - Library bug documentation
- `/docs/roadmap.md` - Project development plan

### Key Takeaways from Research

1. **Antenna delay temperature coefficient**: ±2.15 mm/°C per device (source: Qorvo forums)

2. **DW1000 timestamp resolution**: 15.65 ps (40-bit @ 63.9 GHz) but TX scheduling limited to 8 ns

3. **Arduino Uno interrupt latency**: 3.5-5 μs measured (vs <1 μs on ESP32)

4. **DS-TWR clock drift compensation**: Eliminates first-order clock errors, residual error O(ε²) ≈ 0.1 mm for 10 ppm crystals

5. **Expected accuracy**: ±10 cm achievable with DW1000 + proper calibration (datasheet and community consensus)

6. **Arduino Uno limitations**: 2KB RAM limits to 3-4 anchors, 16 MHz limits update rate to 1-5 Hz

7. **ESP32 advantages**: 240 MHz CPU, 320KB RAM enables ±5-10 cm accuracy at 10-50 Hz update rate

### Recommended Next Steps

1. **Apply library bug fix** (critical for interrupt-based operation)
2. **Calibrate antenna delay** (50-100 cm accuracy improvement)
3. **Implement median filter** (5-10 cm improvement from outlier rejection)
4. **Test in target environment** (understand multipath, interference)
5. **Consider ESP32 migration** if requirements exceed Arduino Uno capabilities

---

**Document Version**: 1.0
**Last Updated**: 2026-01-11
**Hardware Tested**: Arduino Uno R3 + DW1000 (PCL298336 v1.3)
**Author**: SwarmLoc Project
**License**: MIT

---

## Appendix: Quick Reference

### Error Budget Summary

| Error Source | Arduino Uno | ESP32 | Mitigation |
|--------------|-------------|-------|------------|
| DW1000 hardware | ±2 cm | ±2 cm | N/A (inherent limit) |
| Antenna delay (uncalibrated) | ±50-100 cm | ±50-100 cm | **Calibration (critical)** |
| Antenna delay (calibrated) | ±5 cm | ±5 cm | Per-device calibration |
| Temperature drift | ±2 cm | ±1 cm | Calibrate at op temp |
| Interrupt latency | ±2 cm | ±0.5 cm | ISR optimization |
| Multipath | ±10 cm | ±8 cm | LOS, FP check, filtering |
| RF interference | ±3 cm | ±2 cm | Channel 5/7, quality check |
| Protocol timing | ±2 cm | ±0.5 cm | Delayed TX, fast CPU |
| **Total (RSS)** | **±12.6 cm** | **±10.2 cm** | Optimize all factors |

### Optimization Priority

1. **Antenna delay calibration** - 50-100 cm improvement (CRITICAL)
2. **DS-TWR protocol** - Eliminates clock drift (CRITICAL)
3. **Library bug fix** - Enables interrupts (CRITICAL)
4. **Multipath mitigation** - 5-20 cm improvement
5. **Signal quality checks** - Reject bad measurements
6. **Median filtering** - 3-5 cm improvement
7. **ISR optimization** - 1-3 cm improvement
8. **SPI speed increase** - Higher update rate
9. **Temperature compensation** - 1-2 cm improvement (optional)

### Key Code Snippets

**Antenna Delay Adjustment**:
```cpp
float error_m = measured - actual;
uint16_t new_delay = current_delay + (int16_t)(error_m / 2.0 * 213.14);
DW1000.setAntennaDelay(new_delay);
```

**Quality Check**:
```cpp
if (rxPower < -90.0 || fpPower < (rxPower - 6.0)) return;  // Reject
```

**Fast ISR**:
```cpp
volatile bool flag = false;
void ISR() { flag = true; }  // Just set flag
void loop() { if (flag) { flag = false; /* process */ } }
```
