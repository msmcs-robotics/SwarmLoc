# DW1000 Ranging Troubleshooting Guide

## Problem Statement

**Scenario**: Two Arduino Unos with DW1000 shields using arduino-dw1000 library v0.9
- Both devices initialize correctly: "ANCHOR ready" and "TAG ready"
- No `[DEVICE] Found` messages appear
- No ranging measurements after 2+ minutes of operation
- LEN_SYS_MASK bug fix already applied

**Date**: 2026-01-11

---

## Table of Contents

1. [Common Reasons DW1000Ranging Fails to Detect Devices](#1-common-reasons-dw1000ranging-fails-to-detect-devices)
2. [Required Configuration for TAG to Find ANCHOR](#2-required-configuration-for-tag-to-find-anchor)
3. [Channel, PRF, Preamble Settings That Must Match](#3-channel-prf-preamble-settings-that-must-match)
4. [Known Issues with arduino-dw1000 Library](#4-known-issues-with-arduino-dw1000-library)
5. [Alternative Libraries](#5-alternative-libraries)
6. [Debug Techniques to Verify TX/RX](#6-debug-techniques-to-verify-txrx)
7. [Step-by-Step Troubleshooting Checklist](#7-step-by-step-troubleshooting-checklist)

---

## 1. Common Reasons DW1000Ranging Fails to Detect Devices

### 1.1 Voltage Level Mismatch (CRITICAL for Arduino Uno)

**The Problem**: DW1000 operates at 3.3V, but Arduino Uno GPIO operates at 5V.

When the Uno GPIO sends a LOW signal (0V), the DW1000 reads it correctly. However, when sending HIGH (5V), this exceeds the DW1000's maximum input voltage and can:
- Damage the DW1000 chip over time
- Cause unreliable SPI communication
- Result in intermittent failures where devices sometimes work, sometimes don't

**Solution**: Use a bi-directional logic level converter (LLC) between Arduino Uno and DW1000:
- HV side: 5V (Arduino)
- LV side: 3.3V (DW1000)
- Required for: MOSI, SCLK, CS, RST (Arduino outputs to DW1000)
- MISO can connect directly (3.3V from DW1000 is read as HIGH by Arduino)
- IRQ can connect directly (3.3V output from DW1000)

**Alternative**: Use a 3.3V Arduino (Pro Mini 3.3V, Arduino Due, ESP32) which eliminates level shifting entirely.

**Sources**:
- [Fast and Cheap Level shifter 5V -> 3.3V - Issue #67](https://github.com/thotro/arduino-dw1000/issues/67)
- [Bi-Directional Logic Level Converter Hookup Guide](https://learn.sparkfun.com/tutorials/bi-directional-logic-level-converter-hookup-guide/all)

---

### 1.2 Power Supply Insufficiency

**The Problem**: Arduino Uno's 3.3V regulator can only supply ~50-150mA. The DW1000 module can draw up to **150mA during transmission**.

**Symptoms**:
- Initialization works (low power)
- Transmission fails or is weak (high power demand)
- Voltage drops from 3.3V to ~3.0V during TX
- Intermittent communication

**Solutions**:
1. **External 3.3V regulator**: Use an AMS1117-3.3 or similar with 500mA+ capacity
2. **Capacitor**: Add 100-500uF capacitor on DW1000 power line
3. **Separate power**: Power DW1000 from external 3.3V supply, share ground with Arduino

**Sources**:
- [DW1000Ranging_Tag/Anchor - Issue #44](https://github.com/thotro/arduino-dw1000/issues/44)
- [Arduino Forum - DWM1000 with Arduino](https://forum.qorvo.com/t/dwm1000-with-arduino-problem/7741)

---

### 1.3 Interrupt Pin (IRQ) Wiring Issues

**The Problem**: The DW1000 library uses hardware interrupts. IRQ pin MUST be connected to an interrupt-capable pin.

**Arduino Uno Interrupt Pins**:
| Pin | Interrupt Number |
|-----|------------------|
| 2   | INT0 (default)   |
| 3   | INT1             |

**Common Mistakes**:
- IRQ connected to wrong pin (not pin 2 or 3)
- IRQ not connected at all
- Missing pull-down resistor on IRQ line

**Correct Wiring**:
```
DWM1000 Pin 22 (IRQ/GPIO8) --> Arduino Pin 2 (INT0)
```

**Optional but Recommended**:
- Add 10k-12k pull-down resistor between IRQ and GND
- This prevents floating IRQ states

**Sources**:
- [without connecting IRQ pin to gpio2 - Issue #163](https://github.com/thotro/arduino-dw1000/issues/163)
- [Connect IRQ and RESET - Issue #57](https://github.com/thotro/arduino-dw1000/issues/57)

---

### 1.4 Volatile Variable Bug (Multiple Device Discovery)

**The Problem**: The `_networkDevicesNumber` variable in DW1000Ranging may not be read correctly due to missing `volatile` keyword, causing device list errors.

**Symptoms**:
- Devices briefly detected then immediately lost
- "delete inactive device" messages
- Anchor shows "Not found" repeatedly
- Works with 1 anchor/1 tag but fails with more

**Fix**: In `DW1000Ranging.h`, ensure the variable is declared volatile:
```cpp
// CORRECT (with volatile):
volatile uint8_t DW1000RangingClass::_networkDevicesNumber = 0;
```

**Note**: This project's library already has this fix applied (line 41 in DW1000Ranging.cpp shows `volatile uint8_t`).

**Sources**:
- [Bug fix - Issue #179](https://github.com/thotro/arduino-dw1000/issues/179)

---

### 1.5 Frame Filtering Configuration

**The Problem**: Frame filtering must be properly configured for TAG/ANCHOR communication.

**How DW1000Ranging Uses Frame Filtering**:
- Frame filtering is ENABLED when using DW1000Ranging
- Both devices must have same PAN ID (default: 0xDECA)
- MAC addresses must be properly formatted
- Destination addresses must match receiver's short address

**Common Issues**:
- Frame filtering enabled on one device but not the other
- PAN ID mismatch
- MAC frame format incorrect

**Verification**: In DW1000Ranging.cpp line 183:
```cpp
DW1000Ranging.configureNetwork(_currentShortAddress[0]*256+_currentShortAddress[1], 0xDECA, mode);
```
Both ANCHOR and TAG use PAN ID 0xDECA by default.

**Sources**:
- [Frame filtering - Issue #191](https://github.com/thotro/arduino-dw1000/issues/191)
- [MAC message format - Issue #5](https://github.com/thotro/arduino-dw1000/issues/5)

---

## 2. Required Configuration for TAG to Find ANCHOR

### 2.1 Minimum Configuration Requirements

Both TAG and ANCHOR must have:

| Parameter | Must Match | Default Value |
|-----------|------------|---------------|
| Channel | YES | Channel 5 |
| PRF | YES | 16 MHz |
| Data Rate | YES | 6.8 Mbps |
| Preamble Length | YES | 128 symbols |
| PAN ID | YES | 0xDECA |
| Network Mode | YES | MODE_LONGDATA_RANGE_LOWPOWER |

### 2.2 Correct Initialization Sequence

**ANCHOR Setup**:
```cpp
void setup() {
    Serial.begin(115200);

    // Initialize communication (RST, SS, IRQ pins)
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Set callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachBlinkDevice(newBlink);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as anchor with EUI address
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);

    Serial.println("ANCHOR ready");
}

void loop() {
    DW1000Ranging.loop();
}
```

**TAG Setup**:
```cpp
void setup() {
    Serial.begin(115200);

    // Initialize communication (RST, SS, IRQ pins)
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Set callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as tag with DIFFERENT EUI address
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);

    Serial.println("TAG ready");
}

void loop() {
    DW1000Ranging.loop();
}
```

### 2.3 Critical Requirements

1. **Unique EUI Addresses**: Each device MUST have a unique 8-byte EUI address
2. **Same MODE constant**: Both must use identical mode (e.g., `DW1000.MODE_LONGDATA_RANGE_LOWPOWER`)
3. **Call DW1000Ranging.loop()**: Must be called in `loop()` function continuously
4. **Attach callbacks**: At least `attachNewRange` should be attached

---

## 3. Channel, PRF, Preamble Settings That Must Match

### 3.1 Available DW1000 Modes

| Mode Constant | Data Rate | PRF | Preamble | Channel | Range |
|---------------|-----------|-----|----------|---------|-------|
| MODE_LONGDATA_RANGE_LOWPOWER | 110 Kbps | 16 MHz | 2048 | 5 | Long |
| MODE_LONGDATA_RANGE_ACCURACY | 110 Kbps | 64 MHz | 2048 | 5 | Long |
| MODE_SHORTDATA_FAST_LOWPOWER | 6.8 Mbps | 16 MHz | 128 | 5 | Short |
| MODE_SHORTDATA_FAST_ACCURACY | 6.8 Mbps | 64 MHz | 128 | 5 | Short |
| MODE_LONGDATA_FAST_LOWPOWER | 6.8 Mbps | 16 MHz | 128 | 5 | Medium |
| MODE_LONGDATA_FAST_ACCURACY | 6.8 Mbps | 64 MHz | 128 | 5 | Medium |

### 3.2 PRF and Preamble Code Requirements

**Pulse Repetition Frequency (PRF)** must match the preamble code:
- **16 MHz PRF**: Use preamble codes 1-8
- **64 MHz PRF**: Use preamble codes 9-24

**Preamble Length Guidelines**:
| Distance | Recommended Preamble |
|----------|---------------------|
| Short (<10m) | 128 or 256 |
| Medium (10-50m) | 512 or 1024 |
| Long (>50m) | 2048 or 4096 |

### 3.3 DW1000 Channels

| Channel | Center Frequency | Bandwidth |
|---------|-----------------|-----------|
| 1 | 3494.4 MHz | 499.2 MHz |
| 2 | 3993.6 MHz | 499.2 MHz |
| 3 | 4492.8 MHz | 499.2 MHz |
| 4 | 3993.6 MHz | 1331.2 MHz |
| 5 | 6489.6 MHz | 499.2 MHz |
| 7 | 6489.6 MHz | 1081.6 MHz |

**Default**: Channel 5 (6.5 GHz band)

### 3.4 Changing Channel in DW1000Ranging

To use a different channel (e.g., Channel 2):
```cpp
DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY, DW1000.CHANNEL_2);
DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY, DW1000.CHANNEL_2);
```

**Note**: Both devices MUST use the same channel!

**Sources**:
- [Added CHANNEL selection - PR #303](https://github.com/thotro/arduino-dw1000/pull/303/files)
- [DW1000 User Manual](https://www.qorvo.com/products/d/da007967)

---

## 4. Known Issues with arduino-dw1000 Library

### 4.1 Library Status

**CRITICAL**: The thotro/arduino-dw1000 library has been **unmaintained since 2020**.

Last release: v0.9 (May 2020)
Active development: NONE

### 4.2 Known Bugs

| Bug | Status | Description |
|-----|--------|-------------|
| LEN_SYS_MASK buffer overrun | FIXED in this project | `interruptOnReceiveFailed()` used wrong buffer length |
| Volatile _networkDevicesNumber | FIXED in this project | Missing volatile keyword caused device list corruption |
| Range-dependent error | UNFIXED | Distance error increases proportionally with distance |
| SPI speed on some boards | WORKAROUND | Reduce SPI speed if seeing unstable Device ID |
| Arduino M0 Pro compatibility | UNFIXED | All 0xFF values in Device ID |
| Arduino Due compilation | UNFIXED | digitalWriteFast.h errors |

### 4.3 IRQ Pin Bug (Historical)

**Fixed in newer versions**: Earlier versions had a bug where instead of the real pin, the IRQ register was set incorrectly. The fix changed:
```cpp
// OLD (buggy):
DW1000.begin(0, myRST);

// NEW (fixed):
DW1000.begin(D2, myRST);
```

### 4.4 Antenna Delay Calibration Issues

The library's antenna delay calibration may be inaccurate:
- Default antenna delay values may be wrong for your hardware
- Range-dependent errors suggest scaling factor issues
- Calibration at one distance doesn't hold at other distances

**Workaround**: Calibrate at your expected operating distance, or apply a scaling factor to readings.

**Sources**:
- [Questionable ranging performance - Issue #205](https://github.com/thotro/arduino-dw1000/issues/205)
- [Unstable operation - Issue #107](https://github.com/thotro/arduino-dw1000/issues/107)
- [Device ID issues - Issue #136](https://github.com/thotro/arduino-dw1000/issues/136)

---

## 5. Alternative Libraries

### 5.1 arduino-dw1000-ng (F-Army)

**Repository**: https://github.com/F-Army/arduino-dw1000-ng

**Advantages**:
- Fork of original with improvements
- Antenna delay calibration support
- More control over TWR frame sending
- Cleaner API design
- Uses polling instead of interrupts (option)

**Disadvantages**:
- **ARCHIVED** - No longer maintained
- May have its own bugs
- Less community support

### 5.2 jremington DW1000 Library

**Repository**: Search for jremington DW1000 on GitHub

**Advantages**:
- Recommended for ESP32 antenna delay calibration
- Active development (check current status)

### 5.3 Makerfabs ESP32-UWB Library

**Repository**: https://github.com/Makerfabs/Makerfabs-ESP32-UWB

**Advantages**:
- Designed for ESP32
- SS TWR implementation
- Active examples

**Disadvantages**:
- ESP32 specific

### 5.4 arduino-dw1000-esp32 (jaarenhoevel)

**Repository**: https://github.com/jaarenhoevel/arduino-dw1000-esp32

**Advantages**:
- ESP32 modified version of thotro library

**Disadvantages**:
- No active development

### 5.5 Recommendation

For Arduino Uno: Stick with thotro/arduino-dw1000 with fixes applied, or consider migrating to ESP32 for better support and no level shifting requirement.

**Sources**:
- [arduino-dw1000-ng GitHub](https://github.com/F-Army/arduino-dw1000-ng)
- [arduino-dw1000-esp32 GitHub](https://github.com/jaarenhoevel/arduino-dw1000-esp32)

---

## 6. Debug Techniques to Verify TX/RX

### 6.1 Verify SPI Communication First

**Run BasicConnectivityTest**:
```
Expected output:
Device ID: DECA - model: 1, version: 3, revision: 0
```

**If you see**:
- `Device ID: 00 - model: 0, version: 0, revision: 0` = SPI not working at all
- `Device ID: FFFF - model: 255, version: 15, revision: 15` = SPI lines disconnected
- `Device ID: DECA - model: 255, version: 15, revision: 15` = SPI speed too high or unstable connection

**Fix for unstable SPI**: Reduce SPI clock speed in DW1000.cpp or add capacitor to power line.

### 6.2 Enable DW1000 LED Blinking

Add this to your setup() to enable TX/RX activity LEDs:
```cpp
DW1000.enableDebounceClock();
DW1000.enableLedBlinking();
```

**LED Behavior**:
- **RXLED (GPIO2/Pin 13)**: Blinks when receiver is active
- **TXLED (GPIO3/Pin 12)**: Blinks briefly when frame transmitted

If LEDs don't blink, the DW1000 isn't actually transmitting/receiving.

### 6.3 Interrupt Verification

Add interrupt counter to verify IRQ is working:
```cpp
volatile uint32_t interruptCount = 0;

void DW1000InterruptHandler() {
    interruptCount++;
    // ... rest of handler
}

void loop() {
    static uint32_t lastCount = 0;
    static uint32_t lastPrint = 0;

    if (millis() - lastPrint > 1000) {
        Serial.print("Interrupts/sec: ");
        Serial.println(interruptCount - lastCount);
        lastCount = interruptCount;
        lastPrint = millis();
    }

    DW1000Ranging.loop();
}
```

**Expected**: Non-zero interrupt count (typically 1-10 per second during ranging)
**If zero**: IRQ pin not connected or interrupt not enabled

### 6.4 Read System Status Register

Check what the DW1000 is actually doing:
```cpp
void debugSystemStatus() {
    byte status[5];
    DW1000.readBytes(SYS_STATUS, NO_SUB, status, LEN_SYS_STATUS);

    Serial.print("SYS_STATUS: ");
    for (int i = 4; i >= 0; i--) {
        if (status[i] < 0x10) Serial.print("0");
        Serial.print(status[i], HEX);
    }
    Serial.println();

    // Decode key bits
    if (status[0] & 0x02) Serial.println("  RXDFR - Received frame");
    if (status[0] & 0x04) Serial.println("  RXFCG - Good CRC");
    if (status[0] & 0x08) Serial.println("  RXFCE - CRC Error");
    if (status[0] & 0x80) Serial.println("  TXFRS - Transmit done");
}
```

### 6.5 Use GPIO for Logic Analyzer

Configure DW1000 GPIO pins for external monitoring:
```cpp
// Enable TX/RX GPIO outputs for logic analyzer
DW1000.setGPIOMode(MSGP2, LED_MODE);  // RX activity
DW1000.setGPIOMode(MSGP3, LED_MODE);  // TX activity
```

Connect logic analyzer to DWM1000 pins 12 (TXLED) and 13 (RXLED).

### 6.6 Serial Debug Output

Enable library debug output:
```cpp
// In DW1000Ranging.cpp, set:
#define DEBUG true
```

This will print detailed information during ranging.

**Sources**:
- [DW(M)1000 LED Blink Feature Wiki](https://github.com/thotro/arduino-dw1000/wiki/DW(M)1000-LED-Blink-Feature)
- [LED Blink Feature - Issue #237](https://github.com/thotro/arduino-dw1000/issues/237)
- [APS022 Debugging DW1000 Application Note](https://www.qorvo.com/products/d/da008452)

---

## 7. Step-by-Step Troubleshooting Checklist

### Phase 1: Hardware Verification

- [ ] **Power Supply Check**
  - [ ] DW1000 VCC measures 3.3V (3.2-3.4V acceptable)
  - [ ] Power supply can provide 200mA+
  - [ ] Add 100uF capacitor on VCC if not present

- [ ] **Level Shifting (Arduino Uno ONLY)**
  - [ ] MOSI goes through level shifter (5V->3.3V)
  - [ ] SCLK goes through level shifter (5V->3.3V)
  - [ ] CS goes through level shifter (5V->3.3V)
  - [ ] RST goes through level shifter (5V->3.3V)
  - [ ] MISO can be direct (3.3V->5V safe for Arduino input)
  - [ ] IRQ can be direct

- [ ] **Pin Connections**
  - [ ] VCC -> 3.3V (NOT 5V)
  - [ ] GND -> GND
  - [ ] MOSI -> Arduino Pin 11
  - [ ] MISO -> Arduino Pin 12
  - [ ] SCLK -> Arduino Pin 13
  - [ ] CS -> Arduino Pin 10 (or as configured)
  - [ ] RST -> Arduino Pin 9 (or as configured)
  - [ ] IRQ -> Arduino Pin 2 (INT0)

- [ ] **Physical Check**
  - [ ] All solder joints solid
  - [ ] No cold solder joints
  - [ ] Antenna not blocked by metal
  - [ ] Devices within 10m of each other for testing

### Phase 2: SPI Communication

- [ ] **Run BasicConnectivityTest**
  - [ ] Upload to device
  - [ ] Open Serial Monitor at 115200 baud
  - [ ] Verify output shows `Device ID: DECA - model: 1, version: 3, revision: 0`

- [ ] **If Device ID is wrong**
  - [ ] 0x00000000 = SPI not working, check MOSI/MISO/SCLK wiring
  - [ ] 0xFFFFFFFF = No chip communication, check CS and power
  - [ ] Unstable values = Add capacitor, reduce SPI speed

### Phase 3: Interrupt Verification

- [ ] **Verify IRQ pin**
  - [ ] IRQ connected to Pin 2 (Arduino Uno)
  - [ ] Add 10k pulldown resistor on IRQ line (optional but recommended)

- [ ] **Run interrupt counter test**
  - [ ] Add interrupt counting code (see Section 6.3)
  - [ ] Verify interrupts are firing (non-zero count)

### Phase 4: Library Configuration

- [ ] **Verify LEN_SYS_MASK fix applied**
  - [ ] Open `lib/DW1000/src/DW1000.cpp`
  - [ ] Find `interruptOnReceiveFailed()` function
  - [ ] Verify all 4 lines use `LEN_SYS_MASK` (not `LEN_SYS_STATUS`)

- [ ] **Verify volatile fix**
  - [ ] Open `lib/DW1000/src/DW1000Ranging.cpp`
  - [ ] Line 41 should show `volatile uint8_t DW1000RangingClass::_networkDevicesNumber`

### Phase 5: Device Configuration

- [ ] **Both devices have unique EUI addresses**
  - [ ] ANCHOR: e.g., "82:17:5B:D5:A9:9A:E2:9C"
  - [ ] TAG: e.g., "7D:00:22:EA:82:60:3B:9C"

- [ ] **Both devices use same mode**
  - [ ] Both use `DW1000.MODE_LONGDATA_RANGE_LOWPOWER` (or same alternative)

- [ ] **Both devices use same channel**
  - [ ] Default Channel 5, or explicitly set same channel

- [ ] **DW1000Ranging.loop() called in loop()**

### Phase 6: Communication Test

- [ ] **Run BasicSender/BasicReceiver FIRST**
  - [ ] Upload BasicSender to one device
  - [ ] Upload BasicReceiver to other device
  - [ ] Verify packets are received
  - [ ] This confirms basic radio communication works

- [ ] **Then test DW1000Ranging**
  - [ ] Upload ANCHOR code
  - [ ] Upload TAG code
  - [ ] Watch for "device added" messages

### Phase 7: Advanced Debugging

- [ ] **Enable LED blinking**
  ```cpp
  DW1000.enableDebounceClock();
  DW1000.enableLedBlinking();
  ```

- [ ] **Enable debug output**
  - [ ] Set `#define DEBUG true` in DW1000Ranging.cpp

- [ ] **Check antenna delay**
  - [ ] If ranging works but distances are wrong
  - [ ] Calibrate antenna delay at known distance

---

## Quick Reference: Most Common Issues

| Symptom | Most Likely Cause | Fix |
|---------|-------------------|-----|
| Device ID: FFFF | SPI wiring or power | Check all connections |
| No interrupts | IRQ not on Pin 2, or LEN_SYS_MASK bug | Verify IRQ pin and apply fix |
| Devices initialize but never find each other | Level shifting, power, or config mismatch | Check 3.3V power, add level shifter |
| Ranging starts then stops | Power droop during TX, volatile bug | Increase power supply capacity, check volatile fix |
| Distance readings wildly wrong | Antenna delay, wrong mode | Calibrate antenna delay |

---

## Sources and References

### GitHub Issues
- [DW1000 using Nano - Data send working but no Ranging](https://forum.arduino.cc/t/dw1000-using-nano-data-send-working-but-no-ranging/1228196)
- [Bug fix - Issue #179](https://github.com/thotro/arduino-dw1000/issues/179)
- [DW1000Ranging_Tag/Anchor - Issue #44](https://github.com/thotro/arduino-dw1000/issues/44)
- [anchor and tag ranging stops - Issue #274](https://github.com/thotro/arduino-dw1000/issues/274)
- [DW1000Ranging_TAG nothing receive - Issue #233](https://github.com/thotro/arduino-dw1000/issues/233)
- [Frame filtering - Issue #191](https://github.com/thotro/arduino-dw1000/issues/191)
- [Device ID issues - Issue #136](https://github.com/thotro/arduino-dw1000/issues/136)
- [BasicConnectivityTest - Issue #150](https://github.com/thotro/arduino-dw1000/issues/150)

### Official Documentation
- [DW1000 User Manual](https://www.qorvo.com/products/d/da007967)
- [APS022 Debugging DW1000](https://www.qorvo.com/products/d/da008452)
- [APS014 Antenna Delay Calibration](https://www.qorvo.com/products/d/da008449)

### Alternative Libraries
- [arduino-dw1000-ng (F-Army)](https://github.com/F-Army/arduino-dw1000-ng)
- [arduino-dw1000-esp32](https://github.com/jaarenhoevel/arduino-dw1000-esp32)
- [Makerfabs ESP32-UWB](https://github.com/Makerfabs/Makerfabs-ESP32-UWB)

### Tutorials
- [Interfacing DWM1000 with Arduino](https://electropeak.com/learn/interfacing-dwm1000-positioning-module-with-arduino/)
- [Wayne's Tinkering Page - UWB Ranging](https://sites.google.com/site/wayneholder/uwb-ranging-with-the-decawave-dwm1000-part-ii)
- [Getting Started with ESP32 DW1000 UWB](https://how2electronics.com/getting-started-with-esp32-dw1000-uwb-ultra-wideband-module/)
- [Bi-Directional Logic Level Converter Guide](https://learn.sparkfun.com/tutorials/bi-directional-logic-level-converter-hookup-guide/all)

---

**Document Version**: 1.0
**Date**: 2026-01-11
**Project**: SwarmLoc DWS1000_UWB
