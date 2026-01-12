# DW1000 Two-Way Ranging Best Practices and Troubleshooting for Arduino Uno

**Date**: 2026-01-11
**Hardware**: Arduino Uno + DW1000 (PCL298336 v1.3 shield)
**Library**: arduino-dw1000 v0.9
**Chip ID**: 0xDECA0130 (DW1000 confirmed)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [DW1000Ranging Library Deep Dive](#dw1000ranging-library-deep-dive)
3. [Interrupt Handling on Arduino Uno](#interrupt-handling-on-arduino-uno)
4. [Ranging Accuracy Optimization](#ranging-accuracy-optimization)
5. [Serial Output Troubleshooting](#serial-output-troubleshooting)
6. [Multi-Node Ranging](#multi-node-ranging)
7. [Code Examples](#code-examples)
8. [Quick Reference](#quick-reference)

---

## Executive Summary

### Critical Issue Identified

**INTERRUPT BUG IN LIBRARY** - This blocks all interrupt-based operations on Arduino Uno.

**Location**: `/DWS1000_UWB/lib/DW1000/src/DW1000.cpp` lines 993-996

**Impact**: All examples using `setDefaults()` are affected, including BasicSender, BasicReceiver, and DW1000Ranging.

**Fix**: Change 4 lines (replace `LEN_SYS_STATUS` with `LEN_SYS_MASK`)

**See**: [Interrupt Handling Section](#interrupt-handling-on-arduino-uno) for detailed fix instructions.

### Key Findings Summary

| Topic | Status | Key Insight |
|-------|--------|-------------|
| **Interrupt Bug** | CRITICAL BUG FOUND | Buffer overrun corrupts interrupt mask register |
| **DW1000Ranging** | MORE ROBUST | Better than BasicSender/Receiver but still has same bug |
| **Arduino Uno IRQ** | NO ISSUES | Hardware fully compatible, INT0 (D2) works correctly |
| **Antenna Delay** | REQUIRES CALIBRATION | Typical values: 16400-16500, unique per module |
| **Expected Accuracy** | ±10-20 cm | After calibration, realistic for Arduino Uno |
| **Update Rate** | 1-5 Hz | Arduino Uno limitation due to 16MHz CPU |

---

## 1. DW1000Ranging Library Deep Dive

### 1.1 How DW1000Ranging Handles Interrupts vs BasicSender/Receiver

#### BasicSender/BasicReceiver Approach

**Architecture**:
```cpp
// BasicSender
void setup() {
    DW1000.begin(PIN_IRQ, PIN_RST);          // Attach interrupt handler internally
    DW1000.select(PIN_SS);
    DW1000.newConfiguration();
    DW1000.setDefaults();                     // BUGGY: Calls interruptOnReceiveFailed()
    DW1000.commitConfiguration();
    DW1000.attachSentHandler(handleSent);     // Register callback
}

void loop() {
    DW1000.newTransmit();
    DW1000.setData(data, len);
    DW1000.startTransmit();                   // Waits for handleSent() callback
    delay(1000);                              // Hangs here if interrupt doesn't fire
}

void handleSent() {
    sentAck = true;                           // Never executes due to bug
}
```

**Problems**:
- Low-level API requires manual state management
- Single callback per action (sent, received, error)
- No retry logic if interrupt fails
- Infinite loops if callbacks don't fire
- No timeout handling

#### DW1000Ranging Approach

**Architecture**:
```cpp
// DW1000Ranging_TAG
void setup() {
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    // Internally calls DW1000.begin() and sets up interrupts

    DW1000Ranging.attachNewRange(newRange);   // High-level callbacks
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                             DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    // Internally calls setDefaults() - ALSO HAS THE BUG!
}

void loop() {
    DW1000Ranging.loop();  // Continuous state machine processing
}
```

**Key Differences**:

| Feature | BasicSender/Receiver | DW1000Ranging |
|---------|---------------------|---------------|
| **API Level** | Low-level (direct DW1000 calls) | High-level wrapper |
| **State Management** | Manual (user implements) | Built-in state machine |
| **Protocol** | None (raw TX/RX) | Double-Sided TWR (DS-TWR) |
| **Error Handling** | Manual callbacks only | Automatic retry, timeouts |
| **Device Management** | None | Tracks multiple devices |
| **Ranging Calculation** | User implements | Built-in asymmetric TWR |
| **Interrupt Bug** | AFFECTED | ALSO AFFECTED (both use setDefaults) |

**Why DW1000Ranging Seems More Robust**:

1. **State Machine in loop()**:
   ```cpp
   void DW1000RangingClass::loop() {
       checkForReset();        // Watchdog timer
       if (time - timer > _timerDelay) {
           timerTick();        // Periodic processing
       }

       if (_sentAck) {         // Process sent events
           // Handle TX completion
       }

       if (_receivedAck) {     // Process received events
           // Handle RX completion
       }
   }
   ```

2. **Automatic Device Discovery**:
   - TAG sends BLINK messages to discover ANCHOR
   - ANCHOR responds with RANGING_INIT
   - Devices automatically paired before ranging

3. **Message Protocol**:
   - POLL (TAG → ANCHOR): Request ranging
   - POLL_ACK (ANCHOR → TAG): Acknowledge poll
   - RANGE (TAG → ANCHOR): Send timestamps
   - RANGE_REPORT (ANCHOR → TAG): Return distance

4. **Error Recovery**:
   ```cpp
   void DW1000RangingClass::checkForReset() {
       if (!_sentAck && !_receivedAck) {
           if (millis() - _lastActivity > _resetPeriod) {
               resetInactive();  // Reset if no activity
           }
       }
   }
   ```

5. **Timeout Management**:
   - `_resetPeriod` (default: 200ms) triggers reset if no activity
   - `_replyDelayTimeUS` (default: 7000µs) sets response timing
   - Inactive device detection removes stale entries

**HOWEVER**: DW1000Ranging still calls `setDefaults()` during initialization, so it suffers from the **same interrupt bug**!

### 1.2 What Makes DW1000Ranging More Robust?

Despite having the same interrupt bug, DW1000Ranging appears more stable because:

1. **Continuous Processing**:
   - `loop()` is called continuously, not just when interrupts fire
   - Can process partial state updates even if some interrupts are missed

2. **Timeout and Retry**:
   - Watchdog timer (`checkForReset()`) recovers from hangs
   - Inactive device detection cleans up failed connections
   - Automatic state reset after timeout

3. **Better Error Handling**:
   ```cpp
   if (messageType != _expectedMsgId) {
       _protocolFailed = true;  // Mark protocol failure
       // Can recover in next iteration
   }
   ```

4. **Device Management**:
   - Tracks up to 4 devices (MAX_DEVICES = 4)
   - Stores per-device timestamps and state
   - Handles device addition/removal gracefully

5. **Built-in Filtering**:
   ```cpp
   // Exponential Moving Average filter
   if (_useRangeFilter) {
       distance = filterValue(distance, previousDistance, _rangeFilterValue);
   }
   ```
   Default filter value: 15 (configurable via `setRangeFilterValue()`)

### 1.3 Configuration Options for Accuracy

#### Operating Modes

```cpp
// Mode constants (defined in DW1000.h)
DW1000.MODE_LONGDATA_RANGE_LOWPOWER   // Best for Arduino Uno (balanced)
DW1000.MODE_LONGDATA_RANGE_ACCURACY   // Best accuracy (recommended for ranging)
DW1000.MODE_LONGDATA_FAST_ACCURACY    // Faster updates, good accuracy
DW1000.MODE_LONGDATA_FAST_LOWPOWER    // Faster, lower power
DW1000.MODE_SHORTDATA_FAST_ACCURACY   // Short messages, accurate
DW1000.MODE_SHORTDATA_FAST_LOWPOWER   // Short messages, low power
```

**Mode Breakdown**:

| Mode | Data Rate | PRF | Preamble | Range | Accuracy | Power | Best For |
|------|-----------|-----|----------|-------|----------|-------|----------|
| LONGDATA_RANGE_ACCURACY | 110 kb/s | 64 MHz | 2048 | Max | Best | High | **Ranging** |
| LONGDATA_RANGE_LOWPOWER | 110 kb/s | 16 MHz | 2048 | Max | Good | Low | **Arduino Uno** |
| LONGDATA_FAST_ACCURACY | 6.8 Mb/s | 64 MHz | 128 | Good | Good | High | Fast updates |
| LONGDATA_FAST_LOWPOWER | 6.8 Mb/s | 16 MHz | 128 | Good | Fair | Low | Fast + efficient |

**Recommendation for Arduino Uno**:
- **Development/Testing**: `MODE_LONGDATA_RANGE_LOWPOWER` (easier to debug)
- **Production Ranging**: `MODE_LONGDATA_RANGE_ACCURACY` (best accuracy)

#### DW1000Ranging Configuration Options

```cpp
void setup() {
    // Initialize communication
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Set reply time (default: 7000µs)
    DW1000Ranging.setReplyTime(7000);  // Increase if missing responses

    // Set reset period (default: 200ms)
    DW1000Ranging.setResetPeriod(200);  // Watchdog timeout

    // Enable range filtering (smooths measurements)
    DW1000Ranging.useRangeFilter(true);

    // Set filter strength (default: 15, min: 2)
    DW1000Ranging.setRangeFilterValue(15);  // Lower = more filtering

    // Attach callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as TAG or ANCHOR
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                             DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}
```

**Filter Configuration**:
```cpp
// More filtering (smoother, slower response)
DW1000Ranging.setRangeFilterValue(30);  // Very smooth

// Less filtering (faster response, more noise)
DW1000Ranging.setRangeFilterValue(5);   // More responsive

// No filtering
DW1000Ranging.useRangeFilter(false);    // Raw measurements
```

### 1.4 Antenna Delay Calibration Procedures

#### What is Antenna Delay?

Antenna delay accounts for:
- Signal propagation through PCB traces
- Antenna group delay
- Cable delays (if external antenna)
- Module-specific hardware delays

**Typical Values**: 16400 - 16500 for DWM1000 modules

**Critical**: Each module may have different delay due to manufacturing variations.

#### Calibration Procedure

**Method 1: Single-Distance Calibration**

```cpp
// 1. Set devices exactly 1.000 meter apart (use measuring tape)
// 2. Run ranging without calibration (use default antenna delay)
// 3. Record measured distance

float measured = 1.15;  // Example: measures 15cm too long
float actual = 1.00;
float error = measured - actual;  // +0.15 m

// 4. Adjust antenna delay
uint16_t currentDelay = 16450;  // Default
uint16_t adjustment = (uint16_t)(error * 1000);  // Convert to delay units
uint16_t newDelay = currentDelay + adjustment;

// 5. Apply new delay
DW1000.setAntennaDelay(newDelay);

// 6. Re-test and iterate until error < 5 cm
```

**Method 2: Multi-Distance Calibration (More Accurate)**

```cpp
// Test at multiple known distances
float distances[] = {0.5, 1.0, 2.0, 5.0, 10.0};  // meters
float measured[5];   // Record measurements
float errors[5];     // Calculate errors

// Calculate average error
float avgError = 0;
for (int i = 0; i < 5; i++) {
    errors[i] = measured[i] - distances[i];
    avgError += errors[i];
}
avgError /= 5.0;

// Check linearity (should be constant offset)
// If error varies with distance, indicates clock drift or other issues

// Apply correction
uint16_t newDelay = currentDelay + (uint16_t)(avgError * 1000);
DW1000.setAntennaDelay(newDelay);
```

**Method 3: Two-Module Calibration**

For best accuracy, calibrate both TAG and ANCHOR:

```cpp
// On TAG:
DW1000.setAntennaDelay(16450);  // Calibrated value for TAG module

// On ANCHOR:
DW1000.setAntennaDelay(16480);  // Calibrated value for ANCHOR module
```

**Calibration Code Example**:

```cpp
// Add to setup() before ranging
void setup() {
    // ... normal initialization ...

    // Set calibrated antenna delay
    #ifdef ANCHOR
        DW1000.setAntennaDelay(16475);  // Calibrated for this anchor
    #else
        DW1000.setAntennaDelay(16450);  // Calibrated for this tag
    #endif

    // ... rest of setup ...
}
```

**Calibration Verification**:

```cpp
void newRange() {
    float range = DW1000Ranging.getDistantDevice()->getRange();
    float error = range - KNOWN_DISTANCE;  // Compare to known distance

    Serial.print("Range: "); Serial.print(range);
    Serial.print(" m, Error: "); Serial.print(error * 100);
    Serial.println(" cm");

    // Acceptable: error < 0.1 m (10 cm)
}
```

---

## 2. Interrupt Handling on Arduino Uno

### 2.1 Common Interrupt Issues with DW1000 on ATmega328P

#### Issue: Interrupt Callbacks Not Firing

**Symptoms**:
- BasicSender transmits packet #0, then hangs
- BasicReceiver initializes but never receives packets
- Callbacks (`handleSent`, `handleReceived`) never execute
- Program enters infinite loop waiting for interrupt

**Root Cause**: **Buffer overrun bug in arduino-dw1000 library**

**Location**: `/DWS1000_UWB/lib/DW1000/src/DW1000.cpp` lines 993-996

**Buggy Code**:
```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);   // BUG: LEN_SYS_STATUS = 5
    setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);    // Should be: LEN_SYS_MASK = 4
    setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);
    setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);
}
```

**Why This Breaks Interrupts**:
1. `_sysmask` is a 4-byte buffer
2. `LEN_SYS_STATUS` is 5 (incorrect length)
3. `setBit()` writes beyond buffer bounds
4. Corrupts adjacent memory (likely `_chanctrl` array)
5. Corrupted mask is written to DW1000's SYS_MASK register
6. DW1000 chip never generates interrupts
7. Arduino interrupt handler never called
8. Callbacks never execute

**Call Chain**:
```
setup()
  ├─> DW1000.newConfiguration()
  ├─> DW1000.setDefaults()
  │     └─> interruptOnReceiveFailed(true)  ← CORRUPTS _sysmask
  └─> DW1000.commitConfiguration()
        └─> writeSystemEventMaskRegister()  ← Writes corrupted mask to chip
```

### 2.2 IRQ Pin Configuration Requirements

**Hardware Configuration**:

| Component | Value | Notes |
|-----------|-------|-------|
| **IRQ Pin** | D2 (INT0) | Arduino Uno external interrupt 0 |
| **Interrupt Mode** | RISING | Active-high edge-triggered |
| **Pin Mode** | INPUT | Library sets this automatically |
| **Pull-up** | Not needed | DW1000 drives IRQ line actively |

**Library Configuration** (automatic in `DW1000.begin()`):

```cpp
void DW1000Class::begin(uint8_t irq, uint8_t rst) {
    delay(5);
    pinMode(irq, INPUT);  // Configure IRQ pin
    SPI.begin();

    #ifndef ESP8266
    SPI.usingInterrupt(digitalPinToInterrupt(irq));  // Prevent SPI conflicts
    #endif

    _rst = rst;
    _irq = irq;
    _deviceMode = IDLE_MODE;

    // Attach interrupt handler (RISING edge)
    attachInterrupt(digitalPinToInterrupt(_irq), DW1000Class::handleInterrupt, RISING);
}
```

**Arduino Uno Pin Mapping**:
```
D2 (INT0) ─> DW1000 IRQ
D9        ─> DW1000 RST
D10       ─> DW1000 CS (SS)
D11       ─> DW1000 MOSI
D12       ─> DW1000 MISO
D13       ─> DW1000 SCK
```

**No Arduino Uno-Specific Issues**: The hardware is fully compatible. The problem is the software bug in the library.

### 2.3 Polling vs Interrupt-Driven Approaches

#### Interrupt-Driven (Recommended After Fix)

**Advantages**:
- Lower CPU usage (MCU can sleep)
- Better timing accuracy
- More reliable packet detection
- Industry standard approach
- Lower power consumption

**Disadvantages**:
- Requires working interrupt handling
- More complex to debug
- Library bug must be fixed first

**Code Example**:
```cpp
volatile boolean sentAck = false;
volatile boolean receivedAck = false;

void setup() {
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);
}

void loop() {
    if (sentAck) {
        sentAck = false;
        // Handle transmission complete
    }

    if (receivedAck) {
        receivedAck = false;
        // Handle reception complete
        byte data[128];
        DW1000.getData(data, DW1000.getDataLength());
    }
}

void handleSent() {
    sentAck = true;  // Set flag in ISR
}

void handleReceived() {
    receivedAck = true;  // Set flag in ISR
}
```

#### Polling Mode (Workaround)

**Advantages**:
- Works around interrupt bug
- Simpler to understand and debug
- No interrupt handler needed
- Good for testing

**Disadvantages**:
- High CPU usage (continuous polling)
- May miss events if loop() is slow
- Timing-critical
- Not recommended for production
- Higher power consumption

**Code Example**:
```cpp
void loop() {
    // Manually read status register
    DW1000.readSystemEventStatusRegister();

    // Check for transmit done
    if (DW1000.isTransmitDone()) {
        Serial.println("TX complete");
        DW1000.clearTransmitStatus();
    }

    // Check for receive done
    if (DW1000.isReceiveDone()) {
        byte data[128];
        DW1000.getData(data, DW1000.getDataLength());
        Serial.print("RX: "); Serial.println((char*)data);
        DW1000.clearReceiveStatus();

        // Restart receiver
        DW1000.newReceive();
        DW1000.receivePermanently(true);
        DW1000.startReceive();
    }

    // Check for errors
    if (DW1000.isReceiveFailed()) {
        Serial.println("RX error");
        DW1000.clearReceiveStatus();

        // Restart receiver
        DW1000.newReceive();
        DW1000.receivePermanently(true);
        DW1000.startReceive();
    }
}
```

### 2.4 Known Solutions and Workarounds

#### Solution 1: Fix the Library Bug (RECOMMENDED)

**File**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp`

**Lines**: 993-996

**Change**:
```cpp
// BEFORE (BUGGY):
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);   // BUG
    setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);    // BUG
    setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);    // BUG
    setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);   // BUG
}

// AFTER (FIXED):
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);     // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);      // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);      // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);     // FIXED
}
```

**Testing After Fix**:
1. Recompile and upload BasicSender/BasicReceiver
2. Verify continuous packet transmission/reception
3. Check serial output shows incrementing packet numbers
4. Test for >100 packets to confirm stability

#### Solution 2: Skip Buggy Function (Workaround)

Don't call `setDefaults()`, configure interrupts manually:

```cpp
void setup() {
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    DW1000.newConfiguration();

    // Manual configuration (avoid setDefaults)
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);

    // Enable only safe interrupts
    DW1000.interruptOnSent(true);      // OK
    DW1000.interruptOnReceived(true);  // OK
    // DON'T CALL: DW1000.interruptOnReceiveFailed(true);  // HAS BUG

    DW1000.commitConfiguration();

    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);
}
```

#### Solution 3: Use Polling Mode

See [Polling Mode](#polling-mode-workaround) section above.

#### Solution 4: Visual Interrupt Debugging

Add LED feedback to verify interrupt handling:

```cpp
void DW1000Class::handleInterrupt() {
    // Toggle LED to show interrupt fired
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    readSystemEventStatusRegister();

    if (isTransmitDone() && _handleSent != 0) {
        (*_handleSent)();
        clearTransmitStatus();
    }

    if (isReceiveDone() && _handleReceived != 0) {
        (*_handleReceived)();
        clearReceiveStatus();
    }
}
```

---

## 3. Ranging Accuracy Optimization

### 3.1 Antenna Delay Calibration Methods

See [Section 1.4](#14-antenna-delay-calibration-procedures) for detailed procedures.

**Quick Calibration**:
```cpp
// 1. Measure at exactly 1.0 meter
// 2. Record measurement (e.g., 1.05 m)
// 3. Calculate adjustment
uint16_t current = 16450;
float measured = 1.05;
float actual = 1.00;
uint16_t newDelay = current + (uint16_t)((measured - actual) * 1000);

// 4. Apply
DW1000.setAntennaDelay(newDelay);
```

### 3.2 Temperature Compensation

DW1000 has built-in temperature monitoring but no automatic compensation.

**Read Temperature**:
```cpp
// Not directly exposed in arduino-dw1000 library
// Would need to read register 0x28 (TX_CAL) directly
// Temperature reading in 8-bit format
```

**Manual Compensation**:
```cpp
// Temperature affects clock frequency slightly
// For critical applications, implement compensation table:
float tempCoeff = 0.0001;  // Example: 0.01% per degree C
float tempDelta = currentTemp - calibrationTemp;
float correction = 1.0 + (tempCoeff * tempDelta);
float correctedRange = measuredRange * correction;
```

**Recommendation**: For Arduino Uno, temperature compensation is usually not necessary for ±10-20 cm accuracy requirements.

### 3.3 Multipath Mitigation

Multipath = signal reflections causing false distance measurements.

**Strategies**:

1. **First Path Detection**:
   ```cpp
   float fpPower = DW1000.getFirstPathPower();  // Power of first arrival
   float rxPower = DW1000.getReceivePower();    // Total receive power

   if (fpPower < (rxPower - 6.0)) {
       // Significant multipath detected
       // Consider rejecting this measurement
   }
   ```

2. **Receive Quality Check**:
   ```cpp
   float quality = DW1000.getReceiveQuality();

   if (quality < QUALITY_THRESHOLD) {
       // Poor signal quality, likely multipath
       // Reject or mark as low confidence
   }
   ```

3. **Median Filtering**:
   ```cpp
   #define FILTER_SIZE 5
   float measurements[FILTER_SIZE];
   int index = 0;

   void newRange() {
       measurements[index++] = DW1000Ranging.getDistantDevice()->getRange();
       if (index >= FILTER_SIZE) index = 0;

       // Use median instead of average (rejects outliers)
       float median = calculateMedian(measurements, FILTER_SIZE);
   }
   ```

4. **Physical Mitigation**:
   - Test in open areas (line-of-sight)
   - Avoid metal surfaces nearby
   - Keep devices >10 cm apart
   - Mount antennas with clear view

### 3.4 Expected Accuracy on Arduino Uno vs ESP32

| Platform | Best Case | Typical | Update Rate | Notes |
|----------|-----------|---------|-------------|-------|
| **Arduino Uno** | ±10 cm | ±20 cm | 1-3 Hz | After calibration, open area |
| **ESP32** | ±5 cm | ±10 cm | 5-10 Hz | Faster CPU, more processing |
| **DWM3000 + ESP32** | ±2 cm | ±5 cm | 10-20 Hz | Latest chip, best performance |

**Arduino Uno Limitations**:
- 16 MHz CPU (slower timestamp processing)
- 2 KB RAM (smaller buffers)
- 32 KB flash (less room for algorithms)
- Slower SPI clock possible

**ESP32 Advantages**:
- 240 MHz CPU (faster processing)
- 320 KB RAM (larger buffers, more filtering)
- 4 MB flash (complex algorithms)
- Faster SPI (up to 40 MHz)
- Dual-core (parallel processing)

**Recommendation**: Arduino Uno is sufficient for ±10-20 cm accuracy. Migrate to ESP32 only if you need <±10 cm or higher update rates.

---

## 4. Serial Output Troubleshooting

### 4.1 Why DW1000Ranging Might Produce No/Minimal Output

**Common Causes**:

1. **Wrong Baud Rate**:
   ```cpp
   // DW1000Ranging examples use 115200
   Serial.begin(115200);  // NOT 9600!

   // Check serial monitor is set to 115200 baud
   ```

2. **Interrupt Bug Prevents Ranging**:
   - If interrupts don't fire, ranging never completes
   - Callbacks never execute, so no output
   - Fix: Apply interrupt bug fix (see Section 2)

3. **No Device Discovery**:
   ```cpp
   // TAG needs to discover ANCHOR first
   // If ANCHOR not responding, no output

   // Check:
   // - Both devices powered on
   // - Both on same network ID (0xDECA default)
   // - Antennas connected
   // - Within range (<10 meters initially)
   ```

4. **Debug Mode Disabled**:
   ```cpp
   // In DW1000Ranging.h:
   #ifndef DEBUG
   #define DEBUG false  // Set to true for verbose output
   #endif
   ```

5. **Serial Monitor Not Reading Fast Enough**:
   ```cpp
   // Add small delay to allow serial output
   void newRange() {
       Serial.print("Range: ");
       Serial.println(DW1000Ranging.getDistantDevice()->getRange());
       delay(10);  // Give serial time to transmit
   }
   ```

### 4.2 Baud Rate Issues (9600 vs 115200)

**Library Defaults**:
- BasicSender/Receiver: `Serial.begin(9600)`
- DW1000Ranging examples: `Serial.begin(115200)`

**Symptoms of Wrong Baud Rate**:
- Garbage characters (e.g., "�����")
- Partial characters
- Occasional readable words
- No output at all

**Solution**:
```cpp
// In your sketch setup():
Serial.begin(115200);  // Match the library example

// In your serial monitor:
// Set dropdown to 115200 baud
```

**Verification**:
```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);  // Wait for serial to initialize

    Serial.println("=== DW1000 Ranging Test ===");
    Serial.print("Baud rate: "); Serial.println(115200);

    // If you see this clearly, baud rate is correct
}
```

### 4.3 Initialization Failures

**Symptoms**:
- Program hangs during setup
- No output after "Initializing..."
- LED frozen (not blinking)

**Common Causes**:

1. **SPI Communication Failure**:
   ```cpp
   void setup() {
       DW1000.begin(PIN_IRQ, PIN_RST);
       DW1000.select(PIN_SS);

       // Verify chip ID
       char msg[128];
       DW1000.getPrintableDeviceIdentifier(msg);
       Serial.print("Device ID: ");
       Serial.println(msg);  // Should show "DECA..."

       if (strncmp(msg, "DECA", 4) != 0) {
           Serial.println("ERROR: DW1000 not responding!");
           while(1);  // Halt
       }
   }
   ```

2. **Wiring Issues**:
   ```
   Check:
   - MOSI, MISO, SCK connected
   - CS (SS) pin correct (default: D10)
   - IRQ pin correct (default: D2)
   - RST pin correct (default: D9)
   - 3.3V power stable
   - GND connected
   ```

3. **Power Supply Issues**:
   ```
   DW1000 requires stable 3.3V
   - Arduino Uno 3.3V pin limited to ~50 mA
   - DW1000 can draw >100 mA during TX
   - Use external 3.3V regulator if needed
   ```

4. **Reset Timing**:
   ```cpp
   void setup() {
       Serial.begin(115200);
       delay(1000);  // Wait for serial

       Serial.println("Resetting DW1000...");
       DW1000.begin(PIN_IRQ, PIN_RST);
       delay(100);  // Wait for reset

       Serial.println("Selecting DW1000...");
       DW1000.select(PIN_SS);
       delay(10);

       Serial.println("Initialization complete");
   }
   ```

### 4.4 Common Setup Mistakes

**Mistake 1: Forgetting Delay After Serial.begin()**:
```cpp
// WRONG:
void setup() {
    Serial.begin(115200);
    Serial.println("Starting...");  // May not print
}

// RIGHT:
void setup() {
    Serial.begin(115200);
    delay(1000);  // Wait for serial
    Serial.println("Starting...");  // Will print
}
```

**Mistake 2: Using F() Macro Incorrectly**:
```cpp
// WRONG (on Arduino Uno):
String msg = F("Hello");  // F() returns __FlashStringHelper*

// RIGHT:
Serial.println(F("Hello"));  // Print directly from flash
```

**Mistake 3: Not Checking Device ID**:
```cpp
void setup() {
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // ALWAYS verify chip responds
    uint32_t deviceID = 0;
    // Read DEV_ID register (would need direct register access)
    // Should be 0xDECA0130 for DW1000
}
```

**Mistake 4: Wrong Pin Definitions**:
```cpp
// Check your actual wiring matches code
#define PIN_RST  9   // Verify this is correct
#define PIN_SS   10  // Verify this is correct
#define PIN_IRQ  2   // Verify this is correct
```

**Mistake 5: Not Handling Setup Failures**:
```cpp
void setup() {
    if (!initializeDW1000()) {
        Serial.println("FATAL: DW1000 initialization failed!");
        while(1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(100);  // Fast blink = error
        }
    }
}
```

---

## 5. Multi-Node Ranging

### 5.1 How to Set Up 3+ Nodes for Multilateration

**Architecture**:
```
TAG (mobile)
  │
  ├─── ANCHOR 1 (fixed position)
  ├─── ANCHOR 2 (fixed position)
  └─── ANCHOR 3 (fixed position)
```

**Minimum Requirements**:
- 3 anchors for 2D positioning
- 4 anchors for 3D positioning
- 1 tag (device being located)

**DW1000Ranging Limitations**:
- ANCHOR can only track 1 TAG (in current implementation)
- TAG can track up to 4 ANCHORS (MAX_DEVICES = 4)

**Multi-Node Setup**:

**ANCHOR 1 Code**:
```cpp
void setup() {
    Serial.begin(115200);
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachBlinkDevice(newBlink);

    // Unique address for each anchor
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9A",  // ANCHOR 1
                                DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}

void newRange() {
    float range = DW1000Ranging.getDistantDevice()->getRange();
    Serial.print("A1,"); Serial.println(range);  // Format: ID,range
}
```

**ANCHOR 2 Code**:
```cpp
void setup() {
    // Same as ANCHOR 1, but different address
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9B",  // ANCHOR 2
                                DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}

void newRange() {
    float range = DW1000Ranging.getDistantDevice()->getRange();
    Serial.print("A2,"); Serial.println(range);
}
```

**ANCHOR 3 Code**:
```cpp
void setup() {
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",  // ANCHOR 3
                                DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}

void newRange() {
    float range = DW1000Ranging.getDistantDevice()->getRange();
    Serial.print("A3,"); Serial.println(range);
}
```

**TAG Code**:
```cpp
void setup() {
    Serial.begin(115200);
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);

    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                             DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();
    uint16_t anchorAddr = device->getShortAddress();
    float range = device->getRange();

    Serial.print("Anchor 0x"); Serial.print(anchorAddr, HEX);
    Serial.print(": "); Serial.print(range); Serial.println(" m");
}

void loop() {
    DW1000Ranging.loop();

    // Check if we have ranges from all anchors
    if (DW1000Ranging.getNetworkDevicesNumber() >= 3) {
        // Can attempt multilateration
        calculatePosition();
    }
}
```

### 5.2 Address Assignment Strategies

**Strategy 1: Hard-Coded Addresses** (Simplest)

```cpp
// Unique 64-bit address for each device
const char* ANCHOR1_ADDR = "82:17:5B:D5:A9:9A:E2:9A";
const char* ANCHOR2_ADDR = "82:17:5B:D5:A9:9A:E2:9B";
const char* ANCHOR3_ADDR = "82:17:5B:D5:A9:9A:E2:9C";
const char* TAG_ADDR     = "7D:00:22:EA:82:60:3B:9C";
```

**Strategy 2: Auto-Generated Short Addresses** (Default)

```cpp
// DW1000Ranging auto-generates random short address from long address
// randomShortAddress = true (default)
DW1000Ranging.startAsAnchor(ANCHOR1_ADDR, mode, true);

// Short address derived from first 2 bytes
// "82:17:..." -> Short address: 0x8217
```

**Strategy 3: Fixed Short Addresses**

```cpp
// Use fixed short addresses for predictable network
// randomShortAddress = false
DW1000Ranging.startAsAnchor(ANCHOR1_ADDR, mode, false);

// First 2 bytes of long address used as short address
// "82:17:..." -> Short address: 0x8217
// "82:18:..." -> Short address: 0x8218
```

**Strategy 4: Configuration-Based**

```cpp
#define DEVICE_ID 1  // Set via compilation flag

const char* addresses[] = {
    "82:17:5B:D5:A9:9A:E2:9A",  // ID 0
    "82:17:5B:D5:A9:9A:E2:9B",  // ID 1
    "82:17:5B:D5:A9:9A:E2:9C",  // ID 2
    "7D:00:22:EA:82:60:3B:9C"   // ID 3
};

void setup() {
    DW1000Ranging.startAsAnchor(addresses[DEVICE_ID], mode);
}
```

### 5.3 Network Initialization Procedures

**Procedure**:

1. **Power On Sequence**:
   ```
   1. Power on all ANCHORS first
   2. Wait 5 seconds for initialization
   3. Power on TAG
   4. Wait for device discovery
   ```

2. **Device Discovery (TAG Side)**:
   ```cpp
   // TAG automatically sends BLINK messages
   // ANCHORS respond with RANGING_INIT
   // TAG adds ANCHORS to network device list

   void newDevice(DW1000Device* device) {
       Serial.print("Discovered anchor: 0x");
       Serial.println(device->getShortAddress(), HEX);
   }
   ```

3. **Network Formation**:
   ```
   TAG sends BLINK (every ~1.6 seconds)
     |
     v
   ANCHOR1 receives BLINK, sends RANGING_INIT
   ANCHOR2 receives BLINK, sends RANGING_INIT
   ANCHOR3 receives BLINK, sends RANGING_INIT
     |
     v
   TAG receives RANGING_INIT from each anchor
   TAG adds anchors to network device list
     |
     v
   Network ready for ranging
   ```

4. **Verify Network Formation**:
   ```cpp
   void loop() {
       DW1000Ranging.loop();

       static unsigned long lastCheck = 0;
       if (millis() - lastCheck > 5000) {
           lastCheck = millis();

           Serial.print("Devices in network: ");
           Serial.println(DW1000Ranging.getNetworkDevicesNumber());

           if (DW1000Ranging.getNetworkDevicesNumber() < 3) {
               Serial.println("WARNING: Not all anchors discovered!");
           }
       }
   }
   ```

**Initialization Timing**:
- TAG sends BLINK every 80ms × 20 = 1.6 seconds
- ANCHOR responds within 7ms (default reply time)
- Full 3-anchor discovery: ~5-10 seconds

**Handling Initialization Failures**:
```cpp
void loop() {
    static unsigned long initStartTime = millis();
    static bool networkReady = false;

    DW1000Ranging.loop();

    if (!networkReady) {
        if (DW1000Ranging.getNetworkDevicesNumber() >= 3) {
            networkReady = true;
            Serial.println("Network ready!");
        } else if (millis() - initStartTime > 30000) {
            Serial.println("ERROR: Network initialization timeout!");
            Serial.print("Only found ");
            Serial.print(DW1000Ranging.getNetworkDevicesNumber());
            Serial.println(" anchors");
            // Reset or alert user
        }
    }
}
```

---

## 6. Code Examples

### 6.1 Fixed BasicReceiver with Interrupt Bug Fix

```cpp
/**
 * BasicReceiver - FIXED VERSION
 * Includes interrupt bug fix
 */
#include <SPI.h>
#include <DW1000.h>

// Connection pins
const uint8_t PIN_RST = 9;
const uint8_t PIN_SS = 10;
const uint8_t PIN_IRQ = 2;

// Message buffer
char message[128];
volatile boolean received = false;

// Forward declarations
void handleReceived();
void receiver();

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("### DW1000 BasicReceiver (FIXED) ###");

    // Initialize
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Configure
    DW1000.newConfiguration();

    // Manual configuration to avoid buggy setDefaults()
    DW1000.setDeviceAddress(6);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);

    // Enable only safe interrupts
    DW1000.interruptOnSent(true);
    DW1000.interruptOnReceived(true);
    // SKIP: DW1000.interruptOnReceiveFailed(true);  // This has the bug

    DW1000.commitConfiguration();

    // Verify device
    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print("Device ID: "); Serial.println(msg);

    // Attach handler
    DW1000.attachReceivedHandler(handleReceived);

    // Start receiver
    receiver();

    Serial.println("Receiver ready");
}

void loop() {
    if (received) {
        received = false;

        DW1000.getData((byte*)message, DW1000.getDataLength());

        Serial.print("Received: ");
        Serial.println(message);
        Serial.print("RX power: ");
        Serial.print(DW1000.getReceivePower());
        Serial.println(" dBm");
    }
}

void handleReceived() {
    received = true;
}

void receiver() {
    DW1000.newReceive();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}
```

### 6.2 Calibrated DW1000Ranging TAG

```cpp
/**
 * DW1000Ranging TAG with Calibration
 */
#include <SPI.h>
#include <DW1000Ranging.h>

// Connection pins
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Calibration
const uint16_t ANTENNA_DELAY = 16475;  // Calibrated value

// Forward declarations
void newRange();
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("### DW1000 Ranging TAG (Calibrated) ###");

    // Initialize
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Set calibrated antenna delay
    DW1000.setAntennaDelay(ANTENNA_DELAY);
    Serial.print("Antenna delay: "); Serial.println(ANTENNA_DELAY);

    // Configure
    DW1000Ranging.setReplyTime(7000);
    DW1000Ranging.setResetPeriod(200);

    // Enable filtering
    DW1000Ranging.useRangeFilter(true);
    DW1000Ranging.setRangeFilterValue(15);

    // Attach handlers
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as TAG
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                             DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println("TAG ready");
}

void loop() {
    DW1000Ranging.loop();
}

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();

    Serial.print("Anchor 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" | Range: ");
    Serial.print(device->getRange(), 2);
    Serial.print(" m | RX: ");
    Serial.print(device->getRXPower(), 1);
    Serial.print(" dBm | FP: ");
    Serial.print(device->getFPPower(), 1);
    Serial.print(" dBm | Quality: ");
    Serial.println(device->getQuality(), 1);
}

void newDevice(DW1000Device* device) {
    Serial.print("Discovered anchor 0x");
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("Lost anchor 0x");
    Serial.println(device->getShortAddress(), HEX);
}
```

### 6.3 Multi-Anchor TAG with Multilateration

```cpp
/**
 * Multi-Anchor TAG with Position Calculation
 */
#include <SPI.h>
#include <DW1000Ranging.h>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;
const uint16_t ANTENNA_DELAY = 16475;

// Anchor positions (in meters, relative to origin)
struct AnchorPosition {
    uint16_t address;
    float x, y, z;
};

AnchorPosition anchors[] = {
    {0x0000, 0.0, 0.0, 0.0},   // Will be updated with actual addresses
    {0x0000, 3.0, 0.0, 0.0},
    {0x0000, 0.0, 3.0, 0.0},
    {0x0000, 3.0, 3.0, 0.0}
};

// Current ranges
float ranges[4] = {0, 0, 0, 0};
unsigned long lastRange[4] = {0, 0, 0, 0};

void setup() {
    Serial.begin(115200);
    delay(1000);

    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000.setAntennaDelay(ANTENNA_DELAY);

    DW1000Ranging.useRangeFilter(true);
    DW1000Ranging.setRangeFilterValue(15);

    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);

    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                             DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}

void loop() {
    DW1000Ranging.loop();

    static unsigned long lastCalc = 0;
    if (millis() - lastCalc > 1000) {  // Calculate position every second
        lastCalc = millis();
        calculatePosition();
    }
}

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();
    uint16_t addr = device->getShortAddress();
    float range = device->getRange();

    // Store range for this anchor
    for (int i = 0; i < 4; i++) {
        if (anchors[i].address == addr) {
            ranges[i] = range;
            lastRange[i] = millis();
            break;
        }
    }
}

void newDevice(DW1000Device* device) {
    uint16_t addr = device->getShortAddress();

    // Assign to first available anchor slot
    for (int i = 0; i < 4; i++) {
        if (anchors[i].address == 0x0000) {
            anchors[i].address = addr;
            Serial.print("Anchor "); Serial.print(i);
            Serial.print(" = 0x"); Serial.println(addr, HEX);
            break;
        }
    }
}

void calculatePosition() {
    // Check if we have recent ranges from at least 3 anchors
    int validRanges = 0;
    for (int i = 0; i < 4; i++) {
        if (ranges[i] > 0 && (millis() - lastRange[i]) < 2000) {
            validRanges++;
        }
    }

    if (validRanges < 3) {
        Serial.println("Insufficient ranges for position calculation");
        return;
    }

    // Simple 2D trilateration (least squares)
    float x = 0, y = 0;

    // This is a simplified example - use proper multilateration algorithm
    // See: https://en.wikipedia.org/wiki/Trilateration

    Serial.print("Position: (");
    Serial.print(x, 2); Serial.print(", ");
    Serial.print(y, 2); Serial.println(")");
}
```

### 6.4 Polling Mode Example (Workaround)

```cpp
/**
 * Polling Mode Receiver
 * Workaround for interrupt bug
 */
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 9;
const uint8_t PIN_SS = 10;
const uint8_t PIN_IRQ = 2;

char message[128];
int numReceived = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("### Polling Mode Receiver ###");

    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    DW1000.newConfiguration();
    DW1000.setDeviceAddress(6);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    // Start receiver
    DW1000.newReceive();
    DW1000.receivePermanently(true);
    DW1000.startReceive();

    Serial.println("Polling mode active");
}

void loop() {
    // Poll status register
    DW1000.readSystemEventStatusRegister();

    if (DW1000.isReceiveDone()) {
        numReceived++;

        DW1000.getData((byte*)message, DW1000.getDataLength());

        Serial.print("RX #"); Serial.print(numReceived);
        Serial.print(": "); Serial.println(message);

        DW1000.clearReceiveStatus();

        // Restart receiver
        DW1000.newReceive();
        DW1000.receivePermanently(true);
        DW1000.startReceive();
    }

    if (DW1000.isReceiveFailed()) {
        Serial.println("RX error");
        DW1000.clearReceiveStatus();

        // Restart receiver
        DW1000.newReceive();
        DW1000.receivePermanently(true);
        DW1000.startReceive();
    }

    // Small delay to reduce CPU usage
    delay(1);
}
```

---

## 7. Quick Reference

### 7.1 Pin Configuration

```cpp
#define PIN_RST  9   // Reset pin
#define PIN_SS   10  // SPI Chip Select (can use SS constant)
#define PIN_IRQ  2   // Interrupt (must be D2 or D3 on Uno)
```

### 7.2 Library Initialization

```cpp
// Basic initialization
DW1000.begin(PIN_IRQ, PIN_RST);
DW1000.select(PIN_SS);

// DW1000Ranging initialization
DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
```

### 7.3 Configuration Modes

```cpp
// Best for ranging accuracy
DW1000.MODE_LONGDATA_RANGE_ACCURACY

// Best for Arduino Uno (balanced)
DW1000.MODE_LONGDATA_RANGE_LOWPOWER
```

### 7.4 Antenna Delay

```cpp
// Set calibrated antenna delay
DW1000.setAntennaDelay(16475);  // Typical: 16400-16500

// Get current delay
uint16_t delay = DW1000.getAntennaDelay();
```

### 7.5 Common Commands

```cpp
// Transmit
DW1000.newTransmit();
DW1000.setData(data, len);
DW1000.startTransmit();

// Receive
DW1000.newReceive();
DW1000.receivePermanently(true);
DW1000.startReceive();

// Get data
DW1000.getData(buffer, len);
int len = DW1000.getDataLength();

// Timestamps
DW1000.getTransmitTimestamp(timestamp);
DW1000.getReceiveTimestamp(timestamp);

// Signal quality
float rxPower = DW1000.getReceivePower();  // dBm
float fpPower = DW1000.getFirstPathPower();  // dBm
float quality = DW1000.getReceiveQuality();
```

### 7.6 Critical Bug Fix

**File**: `/DWS1000_UWB/lib/DW1000/src/DW1000.cpp` lines 993-996

**Change**: Replace `LEN_SYS_STATUS` with `LEN_SYS_MASK` (4 occurrences)

```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);   // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);    // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);    // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);   // FIXED
}
```

### 7.7 Troubleshooting Checklist

- [ ] Library interrupt bug fixed (see 7.6)
- [ ] Correct baud rate (115200 for DW1000Ranging)
- [ ] Antenna delay calibrated
- [ ] Devices on same network ID
- [ ] Devices within range (<10m initially)
- [ ] Antennas connected
- [ ] 3.3V power stable
- [ ] All pins connected correctly
- [ ] IRQ pin is D2 (INT0) on Arduino Uno

---

## 8. Resources

**Project Documentation**:
- `/home/devel/Desktop/SwarmLoc/docs/findings/INTERRUPT_ISSUE_SUMMARY.md`
- `/home/devel/Desktop/SwarmLoc/docs/findings/interrupt_debugging.md`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/DW1000_LIBRARY_SETUP.md`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/TEST_RESULTS.md`

**Library**:
- GitHub: https://github.com/thotro/arduino-dw1000
- Local: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/`

**Hardware**:
- DW1000 User Manual (Decawave/Qorvo)
- Device ID: 0xDECA0130

---

**Document Status**: Complete
**Last Updated**: 2026-01-11
**Next Steps**: Apply interrupt bug fix and test ranging examples
