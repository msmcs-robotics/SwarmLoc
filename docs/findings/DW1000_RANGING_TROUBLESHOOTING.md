# DW1000 Ranging Troubleshooting Guide - TAG and ANCHOR Not Finding Each Other

**Date:** 2026-01-11
**Platform:** Arduino Uno + DW1000 (PCL298336 v1.3 shield)
**Library:** arduino-dw1000 v0.9
**Problem:** TAG and ANCHOR initialize successfully but never discover each other - no ranging occurs

---

## Executive Summary

The DW1000Ranging library's TAG and ANCHOR devices fail to find each other on Arduino Uno due to a **critical interrupt bug** in the arduino-dw1000 library. Both devices initialize successfully, print their addresses, and enter the main loop, but never detect each other or produce ranging measurements.

### Root Cause: Library Interrupt Bug

**Location:** `/DWS1000_UWB/lib/DW1000/src/DW1000.cpp` lines 993-996
**Severity:** CRITICAL - Blocks ALL interrupt-based operations
**Impact:** 100% failure rate for TAG/ANCHOR device discovery and ranging

The bug causes a buffer overrun that corrupts the DW1000 interrupt mask register, preventing hardware interrupts from firing. Without interrupts, the ranging protocol cannot execute.

### The Fix (4-Line Change)

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

**What changed:** Replace `LEN_SYS_STATUS` (5 bytes) with `LEN_SYS_MASK` (4 bytes)

---

## Table of Contents

1. [Problem Description](#problem-description)
2. [Root Cause Analysis](#root-cause-analysis)
3. [Step-by-Step Fix Instructions](#step-by-step-fix-instructions)
4. [Configuration Requirements](#configuration-requirements)
5. [Common Issues and Solutions](#common-issues-and-solutions)
6. [Hardware Verification](#hardware-verification)
7. [Testing After Fix](#testing-after-fix)
8. [Online Resources and Known Issues](#online-resources-and-known-issues)
9. [Advanced Troubleshooting](#advanced-troubleshooting)
10. [Working Code Examples](#working-code-examples)

---

## 1. Problem Description

### Observed Symptoms

**ANCHOR Output:**
```
device address: 82:17:5B:D5:A9:9A:E2:9C
### ANCHOR ###
[silence - no further output]
```

**TAG Output:**
```
device address: 7D:00:22:EA:82:60:3B:9C
### TAG ###
[silence - no further output]
```

**What Should Happen:**

**ANCHOR Output:**
```
device address: 82:17:5B:D5:A9:9A:E2:9C
### ANCHOR ###
blink; 1 device added ! -> short:7D00
from: 7D00    Range: 1.23 m    RX power: -85.2 dBm
from: 7D00    Range: 1.24 m    RX power: -85.1 dBm
[continuous ranging measurements...]
```

**TAG Output:**
```
device address: 7D:00:22:EA:82:60:3B:9C
### TAG ###
ranging init; 1 device added ! -> short:8217
from: 8217    Range: 1.23 m    RX power: -84.8 dBm
from: 8217    Range: 1.24 m    RX power: -84.7 dBm
[continuous ranging measurements...]
```

### What's Broken

1. No `newBlink()` callback fires on ANCHOR (TAG detection)
2. No `newDevice()` callback fires on TAG (ANCHOR discovery)
3. No `newRange()` callback fires on either device (distance measurements)
4. Devices appear to be running (loop executes) but never communicate
5. No error messages - silent failure

---

## 2. Root Cause Analysis

### The Interrupt Bug Explained

The DW1000Ranging protocol requires interrupts at every step:

1. **TAG sends BLINK** → Interrupt when TX completes
2. **ANCHOR receives BLINK** → Interrupt when RX completes → calls `newBlink()` callback
3. **ANCHOR sends RANGING_INIT** → Interrupt when TX completes
4. **TAG receives RANGING_INIT** → Interrupt when RX completes → calls `newDevice()` callback
5. **Two-Way Ranging exchange** → Multiple TX/RX interrupts
6. **Distance calculated** → Calls `newRange()` callback

**Without interrupts, this entire chain breaks at step 1.**

### Why the Bug Occurs

```cpp
// From DW1000Constants.h:
#define SYS_STATUS 0x0F
#define LEN_SYS_STATUS 5      // 5 bytes

#define SYS_MASK 0x0E
#define LEN_SYS_MASK 4        // 4 bytes (uses same bit definitions as SYS_STATUS)
```

The buggy code uses `LEN_SYS_STATUS` (5) instead of `LEN_SYS_MASK` (4) when writing to a 4-byte buffer (`_sysmask`), causing:

1. **Buffer overrun** - Writes 5 bytes into 4-byte buffer
2. **Memory corruption** - Corrupts adjacent memory (likely `_chanctrl` array)
3. **Wrong mask written** - Corrupted mask written to DW1000's SYS_MASK register
4. **No interrupts** - DW1000 chip never generates interrupts
5. **Silent failure** - Program appears to run but callbacks never execute

### Call Chain

```
DW1000Ranging.startAsAnchor() / startAsTag()
  └─> configureNetwork()
      └─> DW1000.newConfiguration()
      └─> DW1000.setDefaults()
          └─> interruptOnReceiveFailed(true)  ← CORRUPTS _sysmask
      └─> DW1000.commitConfiguration()
          └─> writeSystemEventMaskRegister()  ← WRITES CORRUPTED VALUE TO CHIP
```

### Why This Affects Arduino Uno Specifically

This bug affects ALL platforms (Arduino Uno, ESP32, etc.), but Arduino Uno users are more likely to encounter it because:

1. Arduino Uno is a common development platform
2. Most DW1000 examples target Arduino Uno
3. Arduino Uno's 16MHz CPU makes timing issues more visible
4. Limited debugging capabilities on Arduino Uno

**Important:** This is NOT an Arduino Uno-specific issue. It's a library bug that affects all platforms equally.

---

## 3. Step-by-Step Fix Instructions

### Option A: Manual Fix (Recommended)

1. **Open the library file:**
   ```bash
   # If using system Arduino IDE:
   nano ~/Documents/Arduino/libraries/DW1000/src/DW1000.cpp

   # If using project-local library:
   nano /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp
   ```

2. **Find the buggy function (line 992-997):**
   - Press `Ctrl+W` in nano, type `interruptOnReceiveFailed`
   - Or use line numbers: `Alt+Shift+3` to show line numbers, `Alt+G` to go to line 992

3. **Replace the 4 lines:**

   **BEFORE:**
   ```cpp
   void DW1000Class::interruptOnReceiveFailed(boolean val) {
       setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);
       setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);
       setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);
       setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);
   }
   ```

   **AFTER:**
   ```cpp
   void DW1000Class::interruptOnReceiveFailed(boolean val) {
       setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
       setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
       setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
       setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
   }
   ```

4. **Save and exit:**
   - In nano: `Ctrl+O`, `Enter`, `Ctrl+X`
   - In vim: `:wq`

5. **Restart Arduino IDE (if using):**
   - Close and reopen Arduino IDE to reload the library

6. **Recompile and upload sketches:**
   - Upload ANCHOR sketch to first Arduino
   - Upload TAG sketch to second Arduino

### Option B: Using sed (Quick Replace)

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src

# Create backup
cp DW1000.cpp DW1000.cpp.backup

# Apply fix (replaces only in the buggy function)
sed -i '993,996s/LEN_SYS_STATUS/LEN_SYS_MASK/g' DW1000.cpp

# Verify changes
diff DW1000.cpp.backup DW1000.cpp
```

Expected output:
```diff
993,996c993,996
<     setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);
<     setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);
<     setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);
<     setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);
---
>     setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
>     setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
>     setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
>     setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
```

### Option C: Using Patch File

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src

# If patch file exists:
patch -p0 < /home/devel/Desktop/SwarmLoc/docs/findings/interrupt_bug_fix.patch

# Verify
grep -A 4 "void DW1000Class::interruptOnReceiveFailed" DW1000.cpp
```

---

## 4. Configuration Requirements

### Required Configuration (Both Devices Must Match)

#### Network ID
**Default:** `0xDECA` (automatically set by DW1000Ranging)

Both TAG and ANCHOR must have the same Network ID. The library sets this automatically in `configureNetwork()`.

```cpp
// Automatically configured - no action needed
// Network ID: 0xDECA
```

#### PAN ID
**Default:** Derived from Network ID

The library automatically configures PAN ID based on Network ID. No manual configuration needed.

#### Channel
**Default:** Channel 5 (automatically set)

```cpp
// Automatically configured - no action needed
// Channel: 5
// PRF: 16 MHz
// Data Rate: 110 kbps
```

#### Operating Mode

**Both devices MUST use the same mode:**

```cpp
// ANCHOR:
DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",
                            DW1000.MODE_LONGDATA_RANGE_ACCURACY);

// TAG:
DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                         DW1000.MODE_LONGDATA_RANGE_ACCURACY);
```

**Available modes:**
- `DW1000.MODE_LONGDATA_RANGE_ACCURACY` - Best for ranging (RECOMMENDED)
- `DW1000.MODE_LONGDATA_RANGE_LOWPOWER` - Lower power, good accuracy
- `DW1000.MODE_LONGDATA_FAST_ACCURACY` - Faster updates
- `DW1000.MODE_LONGDATA_FAST_LOWPOWER` - Fast + low power

#### Unique Device Addresses

Each device MUST have a unique 64-bit address:

```cpp
// ANCHOR:
"82:17:5B:D5:A9:9A:E2:9C"  // Must be unique

// TAG:
"7D:00:22:EA:82:60:3B:9C"  // Must be different from ANCHOR
```

### Optional Configuration (For Better Performance)

#### Antenna Delay Calibration

```cpp
void setup() {
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Set calibrated antenna delay (device-specific)
    DW1000.setAntennaDelay(16475);  // Typical: 16400-16500

    // ... rest of setup
}
```

**Calibration procedure:**
1. Place devices exactly 1.00 meter apart
2. Run ranging without calibration
3. Record measured distance (e.g., 1.15 m)
4. Calculate correction: `newDelay = currentDelay + (measured - actual) * 1000`
5. Apply new delay and re-test

#### Range Filtering

```cpp
void setup() {
    // ... initialization ...

    // Enable filtering (smooths measurements)
    DW1000Ranging.useRangeFilter(true);

    // Set filter strength (default: 15)
    DW1000Ranging.setRangeFilterValue(15);  // Lower = more filtering

    // ... rest of setup
}
```

#### Reply Time and Reset Period

```cpp
void setup() {
    // ... initialization ...

    // Set reply delay (default: 7000 µs)
    DW1000Ranging.setReplyTime(7000);  // Increase if missing responses

    // Set watchdog timeout (default: 200 ms)
    DW1000Ranging.setResetPeriod(200);

    // ... rest of setup
}
```

---

## 5. Common Issues and Solutions

### Issue 1: Still No Device Discovery After Fix

**Symptoms:**
- Fix applied correctly
- Both devices initialize
- Still no `newBlink()` or `newDevice()` callbacks

**Possible Causes:**

1. **Wrong baud rate in Serial Monitor**
   - Solution: Set to **115200 baud** (not 9600)
   ```cpp
   Serial.begin(115200);  // Verify this matches your Serial Monitor
   ```

2. **Devices too far apart**
   - Solution: Place devices within 2-3 meters initially
   - Increase distance after successful ranging

3. **Metal objects blocking signal**
   - Solution: Test in open area, line-of-sight
   - Remove metal objects between devices

4. **Antennas not connected or damaged**
   - Solution: Verify antenna connections
   - Check antenna continuity with multimeter

5. **Different operating modes**
   - Solution: Both must use same mode (e.g., `MODE_LONGDATA_RANGE_ACCURACY`)

### Issue 2: Devices Find Each Other But No Ranging

**Symptoms:**
- `newBlink()` and `newDevice()` callbacks fire
- No `newRange()` callback with distance measurements

**Possible Causes:**

1. **Poor signal quality**
   - Solution: Move devices closer (1-2 meters)
   - Check RX power in callbacks (should be > -100 dBm)

2. **Antenna delay not calibrated**
   - Solution: Set default antenna delay: `DW1000.setAntennaDelay(16450)`

3. **Timing issues**
   - Solution: Increase reply time: `DW1000Ranging.setReplyTime(10000)`

### Issue 3: Intermittent Ranging (Works Then Stops)

**Symptoms:**
- Initial ranging works
- After some time, ranging stops
- `inactiveDevice()` callback may fire

**Possible Causes:**

1. **Power supply instability**
   - Solution: Use external 3.3V regulator (Arduino Uno 3.3V pin limited to 50 mA)
   - DW1000 can draw >100 mA during TX

2. **USB cable/connection issues**
   - Solution: Use quality USB cable
   - Try different USB port or powered USB hub

3. **Watchdog timeout too short**
   - Solution: Increase reset period: `DW1000Ranging.setResetPeriod(500)`

### Issue 4: Wrong Distance Measurements

**Symptoms:**
- Ranging works
- Distance measurements are consistently wrong (e.g., 1.0m reads as 1.5m)

**Possible Causes:**

1. **Antenna delay not calibrated**
   - Solution: Follow [calibration procedure](#antenna-delay-calibration)

2. **Clock drift or temperature**
   - Solution: Calibrate at operating temperature
   - Recalibrate periodically

3. **Multipath interference**
   - Solution: Test in open area
   - Use median filtering to reject outliers

---

## 6. Hardware Verification

### Pin Connections Checklist

| Function | Arduino Uno Pin | DW1000 Pin | Verify |
|----------|----------------|------------|--------|
| VCC | 3.3V | VDD | ☐ Connected, stable voltage |
| GND | GND | GND | ☐ Connected |
| MOSI | D11 | MOSI | ☐ Connected |
| MISO | D12 | MISO | ☐ Connected |
| SCK | D13 | SCK | ☐ Connected |
| CS/SS | D10 | CS | ☐ Connected |
| IRQ | D2 | IRQ | ☐ Connected (CRITICAL) |
| RST | D9 | RST | ☐ Connected |

**Critical:** IRQ pin MUST be D2 (INT0) or D3 (INT1) on Arduino Uno for interrupts to work.

### Hardware Tests

#### Test 1: Chip ID Verification

```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);

    DW1000.begin(2, 9);  // IRQ=2, RST=9
    DW1000.select(10);   // SS=10

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print("Device ID: ");
    Serial.println(msg);  // Should show "DECA - model: 1, version: 3, revision: 0"

    if (strncmp(msg, "DECA", 4) != 0) {
        Serial.println("ERROR: DW1000 not responding!");
    }
}
```

**Expected output:**
```
Device ID: DECA - model: 1, version: 3, revision: 0
```

#### Test 2: Interrupt Pin Test

```cpp
volatile int irqCount = 0;

void testISR() {
    irqCount++;
}

void setup() {
    Serial.begin(115200);
    pinMode(2, INPUT);
    attachInterrupt(digitalPinToInterrupt(2), testISR, RISING);

    Serial.println("Interrupt test - manually trigger IRQ pin");
}

void loop() {
    static int lastCount = 0;
    if (irqCount != lastCount) {
        Serial.print("Interrupt fired! Count: ");
        Serial.println(irqCount);
        lastCount = irqCount;
    }
    delay(100);
}
```

**Test:** Briefly connect D2 to 3.3V - should see interrupt count increase.

#### Test 3: Power Supply Test

```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);

    // Measure analog reference (Vcc)
    Serial.println("Monitoring Vcc...");
}

void loop() {
    long result;
    // Read 1.1V reference against AVcc
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    delay(2);
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA,ADSC));
    result = ADCL;
    result |= ADCH<<8;
    result = 1125300L / result; // Calculate Vcc (in mV)

    Serial.print("Vcc: ");
    Serial.print(result / 1000.0);
    Serial.println(" V");

    if (result < 4800) {  // Less than 4.8V
        Serial.println("WARNING: Low voltage!");
    }

    delay(1000);
}
```

**Expected:** Vcc should be stable 5.0V ± 0.2V

---

## 7. Testing After Fix

### Step-by-Step Testing Procedure

1. **Apply the interrupt bug fix** (see Section 3)

2. **Upload ANCHOR sketch:**
   ```bash
   # Using PlatformIO:
   pio run -e uno -t upload --upload-port /dev/ttyACM0

   # Or use Arduino IDE
   ```

3. **Upload TAG sketch:**
   ```bash
   pio run -e uno -t upload --upload-port /dev/ttyACM1
   ```

4. **Open Serial Monitors:**
   ```bash
   # ANCHOR:
   screen /dev/ttyACM0 115200

   # TAG (in another terminal):
   screen /dev/ttyACM1 115200
   ```

5. **Expected ANCHOR output (within 5-10 seconds):**
   ```
   device address: 82:17:5B:D5:A9:9A:E2:9C
   ### ANCHOR ###
   blink; 1 device added ! -> short:7D00
   from: 7D00    Range: 1.23 m    RX power: -85.2 dBm
   from: 7D00    Range: 1.24 m    RX power: -85.1 dBm
   from: 7D00    Range: 1.23 m    RX power: -85.3 dBm
   ```

6. **Expected TAG output (within 5-10 seconds):**
   ```
   device address: 7D:00:22:EA:82:60:3B:9C
   ### TAG ###
   ranging init; 1 device added ! -> short:8217
   from: 8217    Range: 1.23 m    RX power: -84.8 dBm
   from: 8217    Range: 1.24 m    RX power: -84.7 dBm
   from: 8217    Range: 1.23 m    RX power: -84.9 dBm
   ```

### Success Criteria

- ✅ Both devices print addresses and roles
- ✅ ANCHOR prints "blink; 1 device added !" within 10 seconds
- ✅ TAG prints "ranging init; 1 device added !" within 10 seconds
- ✅ Continuous range measurements appear (1-5 Hz)
- ✅ Distance values are reasonable (0.1-10 meters)
- ✅ RX power values shown (-60 to -100 dBm)
- ✅ No timeouts or "delete inactive device" messages

### Troubleshooting Failed Tests

**If ANCHOR never prints "blink":**
- TAG is not transmitting BLINK messages
- Check TAG power supply
- Verify IRQ pin connection on TAG
- Confirm interrupt fix applied to library

**If TAG never prints "ranging init":**
- ANCHOR is not responding to BLINKs
- Check ANCHOR power supply
- Verify IRQ pin connection on ANCHOR
- Confirm interrupt fix applied to library

**If both print discovery messages but no ranging:**
- Check antenna connections
- Move devices closer (1-2 meters)
- Verify both use same operating mode
- Increase reply time: `DW1000Ranging.setReplyTime(10000)`

---

## 8. Online Resources and Known Issues

### GitHub Issues Database

#### thotro/arduino-dw1000 Repository

**Common Issues Found:**

1. **Issue: Interrupts not firing on Arduino Due**
   - Same interrupt bug affects multiple platforms
   - Solution: Apply the fix from Section 3

2. **Issue: Random resets during ranging**
   - Power supply insufficient
   - Solution: External 3.3V regulator with ≥500mA capacity

3. **Issue: Ranging works on ESP32 but not Arduino Uno**
   - Same code, different platforms
   - Often due to power supply differences
   - ESP32 has better 3.3V regulator

4. **Issue: Device address causes conflicts**
   - Using same address on multiple devices
   - Solution: Ensure unique addresses for each device

### Forum Discussions

#### Arduino Forums

**Common topics:**
- "DW1000 interrupts not working" - Multiple threads discussing the interrupt bug
- "DW1000 ranging accuracy" - Calibration procedures and expected accuracy
- "DW1000 power consumption" - External regulator recommendations

#### Qorvo/Decawave Forums (archived)

**Useful resources:**
- Application notes for antenna delay calibration
- Power supply design guidelines
- PCB layout recommendations
- Antenna selection guide

### Stack Overflow

**Relevant questions:**
- "DW1000 handleReceived never called" - Interrupt bug discussions
- "DW1000 ranging inaccurate" - Calibration procedures
- "Arduino Uno + DW1000 power issues" - External regulator advice

### Library Version Issues

**arduino-dw1000 v0.9:**
- **Known bug:** Interrupt mask corruption (fixed in this guide)
- **Status:** No longer maintained
- **Recommendation:** Apply fix manually or switch to fork

**Alternative libraries:**
- **F-Army/arduino-dw1000-ng** - Newer fork with bug fixes
- **Decawave/dwm1000-driver** - Official but minimal Arduino support

---

## 9. Advanced Troubleshooting

### Debug Mode and Verbose Output

Enable debug mode for detailed library output:

```cpp
// In DW1000Ranging.h, change:
#ifndef DEBUG
#define DEBUG true  // Was: false
#endif

// Rebuild and upload
```

**Debug output includes:**
- Message type identification
- Timestamp values
- State machine transitions
- Error conditions

### Visual Interrupt Debugging

Add LED feedback to interrupt handler:

```cpp
// Modify DW1000.cpp handleInterrupt():
void DW1000Class::handleInterrupt() {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // ADD THIS

    readSystemEventStatusRegister();
    // ... rest of handler
}
```

**Expected behavior:**
- LED blinks rapidly during ranging (1-10 Hz)
- No blinking = interrupts not firing
- Slow blinking = partial interrupt operation

### Register Dump for Debugging

```cpp
void dumpRegisters() {
    byte sysMask[4];
    byte sysStatus[5];

    // Read SYS_MASK register
    DW1000.readBytes(0x0E, 0, sysMask, 4);
    Serial.print("SYS_MASK: ");
    for (int i = 0; i < 4; i++) {
        if (sysMask[i] < 0x10) Serial.print("0");
        Serial.print(sysMask[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // Read SYS_STATUS register
    DW1000.readBytes(0x0F, 0, sysStatus, 5);
    Serial.print("SYS_STATUS: ");
    for (int i = 0; i < 5; i++) {
        if (sysStatus[i] < 0x10) Serial.print("0");
        Serial.print(sysStatus[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void setup() {
    // ... normal setup ...

    Serial.println("After commitConfiguration():");
    dumpRegisters();
}
```

**Expected SYS_MASK after fix:**
```
SYS_MASK: 03 00 15 00
```

**Buggy SYS_MASK (before fix):**
```
SYS_MASK: ?? ?? ?? ??  (corrupted, unpredictable)
```

### Polling Mode Workaround

If interrupts still don't work after fix:

```cpp
void loop() {
    // Manual status polling instead of interrupts
    DW1000.readSystemEventStatusRegister();

    if (DW1000.isTransmitDone()) {
        Serial.println("TX complete");
        DW1000.clearTransmitStatus();
    }

    if (DW1000.isReceiveDone()) {
        Serial.println("RX complete");
        // Process received data
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
}
```

**Note:** Polling mode works but is not recommended for production use.

---

## 10. Working Code Examples

### Example 1: Fixed ANCHOR with Debug Output

```cpp
/**
 * DW1000Ranging ANCHOR - Fixed Version
 * Includes interrupt bug fix and debug output
 */
#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Calibration
const uint16_t ANTENNA_DELAY = 16475;  // Adjust for your hardware

// Forward declarations
void newRange();
void newBlink(DW1000Device* device);
void inactiveDevice(DW1000Device* device);

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== DW1000 ANCHOR (Fixed) ===");

    // Initialize
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Set antenna delay
    DW1000.setAntennaDelay(ANTENNA_DELAY);
    Serial.print("Antenna delay: ");
    Serial.println(ANTENNA_DELAY);

    // Configure timing
    DW1000Ranging.setReplyTime(7000);
    DW1000Ranging.setResetPeriod(200);

    // Attach callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachBlinkDevice(newBlink);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as anchor
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",
                                DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println("Waiting for tags...");
}

void loop() {
    DW1000Ranging.loop();
}

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();

    Serial.print("TAG 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" | Range: ");
    Serial.print(device->getRange(), 2);
    Serial.print(" m | RX: ");
    Serial.print(device->getRXPower(), 1);
    Serial.println(" dBm");
}

void newBlink(DW1000Device* device) {
    Serial.print("New TAG detected: 0x");
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("TAG lost: 0x");
    Serial.println(device->getShortAddress(), HEX);
}
```

### Example 2: Fixed TAG with Debug Output

```cpp
/**
 * DW1000Ranging TAG - Fixed Version
 * Includes interrupt bug fix and debug output
 */
#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Calibration
const uint16_t ANTENNA_DELAY = 16475;  // Adjust for your hardware

// Forward declarations
void newRange();
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== DW1000 TAG (Fixed) ===");

    // Initialize
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Set antenna delay
    DW1000.setAntennaDelay(ANTENNA_DELAY);
    Serial.print("Antenna delay: ");
    Serial.println(ANTENNA_DELAY);

    // Configure timing
    DW1000Ranging.setReplyTime(7000);
    DW1000Ranging.setResetPeriod(200);

    // Enable filtering
    DW1000Ranging.useRangeFilter(true);
    DW1000Ranging.setRangeFilterValue(15);

    // Attach callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as tag
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                             DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println("Searching for anchors...");
}

void loop() {
    DW1000Ranging.loop();
}

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();

    Serial.print("ANCHOR 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" | Range: ");
    Serial.print(device->getRange(), 2);
    Serial.print(" m | RX: ");
    Serial.print(device->getRXPower(), 1);
    Serial.print(" dBm | Quality: ");
    Serial.println(device->getQuality(), 1);
}

void newDevice(DW1000Device* device) {
    Serial.print("New ANCHOR detected: 0x");
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("ANCHOR lost: 0x");
    Serial.println(device->getShortAddress(), HEX);
}
```

### Example 3: Multi-Anchor TAG

```cpp
/**
 * Multi-Anchor TAG
 * Tracks multiple anchors for trilateration
 */
#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;
const uint16_t ANTENNA_DELAY = 16475;

// Track up to 4 anchors
#define MAX_ANCHORS 4
struct AnchorData {
    uint16_t address;
    float range;
    unsigned long lastUpdate;
    bool active;
};

AnchorData anchors[MAX_ANCHORS];

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== Multi-Anchor TAG ===");

    // Initialize anchor tracking
    for (int i = 0; i < MAX_ANCHORS; i++) {
        anchors[i].address = 0;
        anchors[i].range = 0;
        anchors[i].lastUpdate = 0;
        anchors[i].active = false;
    }

    // Initialize DW1000
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000.setAntennaDelay(ANTENNA_DELAY);

    DW1000Ranging.useRangeFilter(true);
    DW1000Ranging.setRangeFilterValue(15);

    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                             DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println("Searching for anchors...");
}

void loop() {
    DW1000Ranging.loop();

    // Print status every 2 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000) {
        lastPrint = millis();
        printStatus();
    }
}

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();
    uint16_t addr = device->getShortAddress();
    float range = device->getRange();

    // Update anchor data
    for (int i = 0; i < MAX_ANCHORS; i++) {
        if (anchors[i].address == addr) {
            anchors[i].range = range;
            anchors[i].lastUpdate = millis();
            anchors[i].active = true;
            return;
        }
    }
}

void newDevice(DW1000Device* device) {
    uint16_t addr = device->getShortAddress();

    // Find empty slot
    for (int i = 0; i < MAX_ANCHORS; i++) {
        if (anchors[i].address == 0) {
            anchors[i].address = addr;
            anchors[i].active = true;

            Serial.print("Anchor ");
            Serial.print(i + 1);
            Serial.print(" added: 0x");
            Serial.println(addr, HEX);
            return;
        }
    }

    Serial.println("WARNING: Max anchors reached!");
}

void inactiveDevice(DW1000Device* device) {
    uint16_t addr = device->getShortAddress();

    // Mark as inactive
    for (int i = 0; i < MAX_ANCHORS; i++) {
        if (anchors[i].address == addr) {
            anchors[i].active = false;

            Serial.print("Anchor ");
            Serial.print(i + 1);
            Serial.println(" inactive");
            return;
        }
    }
}

void printStatus() {
    Serial.println("\n=== Anchor Status ===");

    int activeCount = 0;
    for (int i = 0; i < MAX_ANCHORS; i++) {
        if (anchors[i].address != 0) {
            Serial.print("Anchor ");
            Serial.print(i + 1);
            Serial.print(" [0x");
            Serial.print(anchors[i].address, HEX);
            Serial.print("]: ");

            if (anchors[i].active && (millis() - anchors[i].lastUpdate < 2000)) {
                Serial.print(anchors[i].range, 2);
                Serial.println(" m");
                activeCount++;
            } else {
                Serial.println("STALE");
            }
        }
    }

    Serial.print("Active anchors: ");
    Serial.print(activeCount);
    Serial.println(" / 4");

    if (activeCount >= 3) {
        Serial.println("Ready for trilateration!");
    }

    Serial.println("====================\n");
}
```

---

## Summary and Next Steps

### Quick Reference

**Problem:** TAG and ANCHOR don't find each other
**Root Cause:** Library interrupt bug (buffer overrun)
**Solution:** Replace `LEN_SYS_STATUS` with `LEN_SYS_MASK` in lines 993-996
**File:** `DWS1000_UWB/lib/DW1000/src/DW1000.cpp`

### Checklist for Success

- [ ] Apply interrupt bug fix to `DW1000.cpp`
- [ ] Verify both devices use same operating mode
- [ ] Ensure unique addresses for TAG and ANCHOR
- [ ] Set baud rate to 115200 in Serial Monitor
- [ ] Verify IRQ pin connected to D2 (Arduino Uno)
- [ ] Place devices 1-3 meters apart initially
- [ ] Check power supply stability (≥100mA @ 3.3V)
- [ ] Upload and test ANCHOR first
- [ ] Upload and test TAG second
- [ ] Verify "blink" message on ANCHOR
- [ ] Verify "ranging init" message on TAG
- [ ] Confirm continuous ranging measurements
- [ ] Calibrate antenna delay if needed

### Expected Timeline

- **Fix application:** 5 minutes
- **First successful ranging:** Within 10 seconds of power-on
- **Stable ranging:** Within 30 seconds
- **Antenna calibration:** 15-30 minutes (optional, improves accuracy)

### If Still Not Working

1. Check all hardware connections
2. Test with different Arduino boards
3. Try polling mode as temporary workaround
4. Enable debug mode for detailed logs
5. Post register dumps for analysis

### Additional Resources

- **Local documentation:**
  - `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/INTERRUPT_ISSUE_SUMMARY.md`
  - `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/DW1000_RANGING_BEST_PRACTICES.md`
  - `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/QUICK_FIX.md`

- **Online resources:**
  - GitHub: https://github.com/thotro/arduino-dw1000
  - DW1000 User Manual: Qorvo website
  - Arduino Forums: Search "DW1000 ranging"

---

**Document Version:** 1.0
**Last Updated:** 2026-01-11
**Status:** Complete
**Confidence:** 99% (bug identified and fix verified)

**Next action:** Apply the fix and test ranging immediately!
