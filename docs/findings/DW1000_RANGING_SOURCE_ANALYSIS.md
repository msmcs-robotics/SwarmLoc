# DW1000Ranging Library Source Code Analysis

**Date**: 2026-01-11
**Objective**: Identify why DW1000 devices don't discover each other during ranging protocol

## Critical Discovery: Root Causes Identified

After analyzing the DW1000Ranging library source code, I've identified **MULTIPLE CRITICAL ISSUES** preventing device discovery:

---

## 1. CRITICAL: Network ID Hardcoded to 0xDECA

### Location
- **File**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000Ranging.cpp`
- **Line**: 183 (Anchor), 216 (Tag)

### The Problem
```cpp
// In startAsAnchor() - Line 183
DW1000Ranging.configureNetwork(_currentShortAddress[0]*256+_currentShortAddress[1], 0xDECA, mode);

// In startAsTag() - Line 216
DW1000Ranging.configureNetwork(_currentShortAddress[0]*256+_currentShortAddress[1], 0xDECA, mode);
```

**BOTH anchor and tag are HARDCODED to Network ID 0xDECA (57034 decimal)**

### Impact
- This is actually GOOD - both devices are on the same network
- However, there's no way to change it without modifying library source
- If you need multiple independent networks, this is a limitation

---

## 2. CRITICAL: Frame Filtering is DISABLED by Default

### Location
- **File**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp`
- **Line**: 1261

### The Problem
```cpp
void DW1000Class::setDefaults() {
    if(_deviceMode == IDLE_MODE) {
        // ...
        //for global frame filtering
        setFrameFilter(false);  // <<<< FRAME FILTER DISABLED!

        /* old defaults with active frame filter - commented out:
        setFrameFilter(true);
        setFrameFilterAllowData(true);
        setFrameFilterAllowReserved(true);
        */
    }
}
```

### Impact
**THIS IS LIKELY THE MAIN ISSUE!** The library used to enable frame filtering but changed the default behavior. Comments say "better set filter in every script where you really need it" but the ranging examples DON'T SET IT.

With frame filtering disabled:
- Devices may reject or ignore properly formatted frames
- BLINK messages (device discovery) may not be recognized
- Data frames (POLL, POLL_ACK, RANGE) may be dropped

---

## 3. CRITICAL: No Antenna Delay Calibration

### Location
- **File**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp`
- **Lines**: 63-64, 1011-1014

### The Problem
```cpp
// Initial state
DW1000Time DW1000Class::_antennaDelay;
boolean    DW1000Class::_antennaCalibrated = false;

void DW1000Class::setAntennaDelay(const uint16_t value) {
    _antennaDelay.setTimestamp(value);
    _antennaCalibrated = true;
}
```

**The antenna delay is initialized to 0 and never set in the ranging examples!**

### Impact
- Zero antenna delay = incorrect timestamp calculations
- Ranging accuracy severely impacted (even if devices discover each other)
- May cause timing issues in the ranging protocol
- **Typical antenna delay: ~16450 for DW1000 modules**

---

## 4. Discovery Protocol Analysis

### How Device Discovery Works

#### TAG Side (Initiator):
1. **Line 727-738**: `timerTick()` function controls discovery
2. **Every 21st tick** (`counterForBlink == 0`): Tag transmits BLINK message
3. **Line 776-779**: BLINK is broadcast message with tag's address

```cpp
void DW1000RangingClass::timerTick() {
    if(_networkDevicesNumber > 0 && counterForBlink != 0) {
        if(_type == TAG) {
            transmitPoll(nullptr);  // Normal ranging
        }
    }
    else if(counterForBlink == 0) {
        if(_type == TAG) {
            transmitBlink();  // DEVICE DISCOVERY
        }
    }
    counterForBlink++;
    if(counterForBlink > 20) {
        counterForBlink = 0;  // Reset: Send BLINK every 21 cycles
    }
}
```

#### ANCHOR Side (Responder):
1. **Line 456-471**: Anchor waits for BLINK messages
2. When BLINK received:
   - Decodes tag address
   - Creates DW1000Device object
   - Adds to network devices array
   - Sends RANGING_INIT response
   - Expects POLL message next

```cpp
if(messageType == BLINK && _type == ANCHOR) {
    byte address[8];
    byte shortAddress[2];
    _globalMac.decodeBlinkFrame(data, address, shortAddress);
    DW1000Device myTag(address, shortAddress);

    if(addNetworkDevices(&myTag)) {
        if(_handleBlinkDevice != 0) {
            (*_handleBlinkDevice)(&myTag);  // Callback
        }
        transmitRangingInit(&myTag);  // Send response
        noteActivity();
    }
    _expectedMsgId = POLL;
}
```

### Timing Parameters
```cpp
#define DEFAULT_RESET_PERIOD 200       // 200ms inactivity timeout
#define DEFAULT_REPLY_DELAY_TIME 7000  // 7000us (7ms) reply delay
#define DEFAULT_TIMER_DELAY 80         // 80ms between timer ticks
```

**BLINK transmission frequency**: Every 21 × 80ms = **1.68 seconds**

---

## 5. Message Flow State Machine

### Expected Message Flow
```
TAG                          ANCHOR
 |                              |
 |-------- BLINK ------------->|  (Every 1.68s until anchor found)
 |                              |
 |<----- RANGING_INIT ---------|  (Anchor responds, adds tag to network)
 |                              |
 |-------- POLL -------------->|  (Tag starts ranging)
 |                              |
 |<------ POLL_ACK ------------|
 |                              |
 |-------- RANGE ------------->|
 |                              |
 |<----- RANGE_REPORT ---------|  (Contains distance)
 |                              |
```

### State Variables
```cpp
volatile byte _expectedMsgId;  // Expected next message type
boolean _protocolFailed;       // Protocol error flag
```

---

## 6. Configuration Analysis

### What configureNetwork() Does
```cpp
void DW1000RangingClass::configureNetwork(uint16_t deviceAddress,
                                           uint16_t networkId,
                                           const byte mode[]) {
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(deviceAddress);  // Short address
    DW1000.setNetworkId(networkId);          // PAN ID
    DW1000.enableMode(mode);                 // Data rate, PRF, preamble
    DW1000.commitConfiguration();
}
```

### Default Configuration from setDefaults()
- **Frame filtering**: DISABLED (this is the problem!)
- **Frame check**: ENABLED (CRC)
- **Receiver auto-reenable**: ENABLED
- **Channel**: 5 (center frequency 6489.6 MHz)
- **Mode**: MODE_LONGDATA_RANGE_LOWPOWER
  - Data rate: 110 kbps
  - PRF: 16 MHz
  - Preamble length: 2048 symbols
- **Preamble code**: 4 (for 16MHz PRF)
- **Smart power**: DISABLED

### What's NOT Set
- Antenna delay (remains 0)
- Frame filter configuration
- PAN ID filtering behavior

---

## 7. DEBUG Flag Available

### Location
```cpp
// In DW1000Ranging.h - Line 69
#ifndef DEBUG
#define DEBUG false
#endif
```

### Debug Output (when DEBUG=true)
Lines 126-152 in DW1000Ranging.cpp show debug info:
- Device ID
- Unique ID
- Network ID & Device Address
- Device mode configuration

Line 501-509: Debug output when device not found in network

---

## 8. Missing Configuration Steps

Based on library examples and source analysis, here's what's MISSING:

### In Our Code
```cpp
// Current initialization (INCOMPLETE)
DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",
                              DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
DW1000Ranging.loop();
```

### What We NEED to Add

```cpp
// 1. Initialize communication
DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

// 2. CRITICAL: Enable frame filtering BEFORE startAsAnchor/Tag
//    This must be done by modifying library or calling DW1000 directly
//    AFTER initCommunication but BEFORE startAsAnchor
// Note: This requires accessing DW1000 object directly since
// DW1000Ranging doesn't expose frame filter configuration

// 3. Set antenna delay (for accuracy)
//    Again, must access DW1000 directly
// DW1000.setAntennaDelay(16450);  // Typical value for DWM1000

// 4. Attach handlers (we do this)
DW1000Ranging.attachNewRange(newRange);
DW1000Ranging.attachBlinkDevice(newBlink);
DW1000Ranging.attachInactiveDevice(inactiveDevice);

// 5. Start as anchor or tag
DW1000Ranging.startAsAnchor(address, mode);

// 6. Call loop() continuously
DW1000Ranging.loop();
```

---

## 9. The Real Problem: Configuration Timing

### Issue
**The library calls `setDefaults()` INSIDE `configureNetwork()`**, which is called from `startAsAnchor()` and `startAsTag()`. By that point:
- We can't set frame filtering before defaults are applied
- Defaults disable frame filtering
- No way to enable it through DW1000Ranging API

### Evidence
```cpp
// startAsAnchor() line 183
DW1000Ranging.configureNetwork(...);
  // Which calls DW1000.setDefaults()
    // Which disables frame filtering!

generalStart();  // Sets up interrupts and starts receiving
```

### Solutions

#### Option A: Modify Library (Recommended)
Edit `DW1000.cpp` line 1261:
```cpp
// Change from:
setFrameFilter(false);

// To:
setFrameFilter(true);
setFrameFilterAllowData(true);
setFrameFilterAllowReserved(true);
```

#### Option B: Post-Init Configuration
Add after `startAsAnchor()` or `startAsTag()`:
```cpp
// Access DW1000 directly to fix frame filtering
DW1000.idle();  // Stop receiving
DW1000.newConfiguration();
DW1000.setFrameFilter(true);
DW1000.setFrameFilterAllowData(true);
DW1000.setFrameFilterAllowReserved(true);
DW1000.setAntennaDelay(16450);
DW1000.commitConfiguration();
// DW1000Ranging will restart receiver in loop()
```

#### Option C: Fork Library
Create custom version with proper defaults

---

## 10. Additional Issues Found

### A. Anchor Limits to One Tag
```cpp
// Line 268-276 in addNetworkDevices()
if(addDevice) {
    if(_type == ANCHOR) {  // For anchors
        _networkDevicesNumber = 0;  // RESET to 0!
    }
    memcpy(&_networkDevices[_networkDevicesNumber], device, ...);
    _networkDevicesNumber++;
}
```
Comment says "for now let's start with 1 TAG" - Anchor can only track one tag at a time!

### B. MAX_DEVICES Limit
```cpp
#define MAX_DEVICES 4  // Line 49 in DW1000Ranging.h
```
Maximum 4 devices in network array

### C. No Timeout on Failed Discovery
Tag keeps blinking forever with no indication if no anchor responds

### D. Interrupt Pin Must Support RISING Edge
```cpp
// Line 182 in DW1000.cpp
attachInterrupt(digitalPinToInterrupt(_irq),
                DW1000Class::handleInterrupt,
                RISING);
```
Arduino pin 2 is correct - supports interrupts on most boards

---

## 11. Network Configuration Details

### PAN ID (Network ID)
- Hardcoded to **0xDECA** (57034 decimal)
- Same for all devices (correct)
- Used for MAC filtering when enabled

### Device Addresses
From examples:
- **Anchor**: `"82:17:5B:D5:A9:9A:E2:9C"` (8-byte EUI-64)
- **Tag**: `"7D:00:22:EA:82:60:3B:9C"` (8-byte EUI-64)

Short addresses derived from:
- First 2 bytes of EUI-64 (if randomShortAddress = false)
- Random bytes (if randomShortAddress = true)

Our test uses non-random, so:
- Anchor short: 0x8217
- Tag short: 0x7D00

---

## 12. Recommended Fix (Step-by-Step)

### Immediate Fix - Modify Library

1. **Edit**: `DWS1000_UWB/lib/DW1000/src/DW1000.cpp`
2. **Line 1261**, change:
```cpp
// FROM:
setFrameFilter(false);

// TO:
setFrameFilter(true);
setFrameFilterAllowData(true);
setFrameFilterAllowReserved(true);
```

3. **Add antenna delay** in same file, line ~1290 (in setDefaults, after setPreambleCode):
```cpp
// Add after line 1289
if(!_antennaCalibrated) {
    setAntennaDelay(16450);  // Default antenna delay
}
```

### Test Configuration

```cpp
// Anchor
#define PIN_RST 9
#define PIN_IRQ 2
#define PIN_SS SS

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== DW1000 Anchor ===");

    // Initialize
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Attach callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachBlinkDevice(newBlink);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as anchor (library now has correct defaults)
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",
                                 DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println("Anchor started, waiting for tags...");
}

void loop() {
    DW1000Ranging.loop();
}

void newBlink(DW1000Device* device) {
    Serial.print("TAG DISCOVERED! Short addr: 0x");
    Serial.println(device->getShortAddress(), HEX);
}

void newRange() {
    Serial.print("Range from 0x");
    Serial.print(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
    Serial.print(": ");
    Serial.print(DW1000Ranging.getDistantDevice()->getRange());
    Serial.println(" m");
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("Device timeout: 0x");
    Serial.println(device->getShortAddress(), HEX);
}
```

### Tag Code (Similar)
```cpp
// Tag
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== DW1000 Tag ===");

    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                              DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println("Tag started, searching for anchors...");
}

void newDevice(DW1000Device* device) {
    Serial.print("ANCHOR DISCOVERED! Short addr: 0x");
    Serial.println(device->getShortAddress(), HEX);
}
```

---

## 13. Alternative: Non-Invasive Fix

If you can't modify the library:

```cpp
void setup() {
    Serial.begin(115200);

    // Initialize
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Start (with broken defaults)
    DW1000Ranging.startAsAnchor(address, mode);

    // FIX: Reconfigure immediately after
    DW1000.idle();
    byte syscfg[4];
    // Read current config
    DW1000.readBytes(0x04, 0xFF, syscfg, 4);
    // Enable frame filtering (bit 0)
    syscfg[0] |= 0x01;  // FFEN_BIT
    // Enable data frames (bit 3)
    syscfg[0] |= 0x08;  // FFAD_BIT
    // Enable reserved frames (bit 6)
    syscfg[0] |= 0x40;  // FFAR_BIT
    // Write back
    DW1000.writeBytes(0x04, 0xFF, syscfg, 4);

    // Set antenna delay
    DW1000.setAntennaDelay(16450);

    // Receiver will restart in first loop() call
}
```

---

## 14. Expected Behavior After Fix

### Serial Output - Tag
```
=== DW1000 Tag ===
DW1000-arduino
configuration..
Device ID: DECA - model: 1, version: 3, revision: 0
Unique ID: 7D:00:22:EA:82:60:3B:9C short: 7D:00
Network ID & Device Address: PAN: DECA, Short Address: 7D00
Device mode: ...
### TAG ###
Tag started, searching for anchors...
ANCHOR DISCOVERED! Short addr: 0x8217
Range from 0x8217: 2.35 m
Range from 0x8217: 2.34 m
Range from 0x8217: 2.36 m
```

### Serial Output - Anchor
```
=== DW1000 Anchor ===
DW1000-arduino
configuration..
Device ID: DECA - model: 1, version: 3, revision: 0
Unique ID: 82:17:5B:D5:A9:9A:E2:9C short: 82:17
Network ID & Device Address: PAN: DECA, Short Address: 8217
Device mode: ...
### ANCHOR ###
Anchor started, waiting for tags...
TAG DISCOVERED! Short addr: 0x7D00
Range from 0x7D00: 2.35 m
Range from 0x7D00: 2.34 m
Range from 0x7D00: 2.36 m
```

---

## 15. Summary of Root Causes

### Primary Issue
**Frame filtering is disabled by default** - Library changed defaults, examples not updated

### Secondary Issues
1. No antenna delay calibration (impacts accuracy, not discovery)
2. No public API to configure frame filtering
3. Anchor limited to one tag
4. Long BLINK interval (1.68s) makes discovery slow

### Required Fixes
1. **Enable frame filtering** (modify library or post-configuration)
2. **Set antenna delay** for accurate ranging
3. Ensure both devices use same network ID (already correct)
4. Ensure unique device addresses (already correct)

---

## 16. Additional Debugging

### Enable Debug Output
In your sketch before includes:
```cpp
#define DEBUG true
#include <DW1000Ranging.h>
```

### Check Hardware
- Verify 3.3V power supply
- Check SPI connections (MOSI, MISO, SCK)
- Verify interrupt pin wiring (must support interrupts)
- Ensure proper antenna connection
- Separate devices by 0.5-3 meters for testing

### Monitor During Discovery
Add prints in loop():
```cpp
void loop() {
    static unsigned long lastPrint = 0;
    if(millis() - lastPrint > 1000) {
        Serial.print("Network devices: ");
        Serial.println(DW1000Ranging.getNetworkDevicesNumber());
        lastPrint = millis();
    }
    DW1000Ranging.loop();
}
```

---

## Conclusion

The primary reason devices don't discover each other is **disabled frame filtering** in the library's default configuration. This was changed from the original behavior but examples weren't updated.

**Fix**: Modify library to enable frame filtering or reconfigure after initialization.

**Secondary improvement**: Add antenna delay calibration for accurate ranging.

After these fixes, device discovery should work within 1-2 seconds, and ranging should provide accurate distance measurements.
