# DW1000 Arduino Library Alternatives Research

**Date:** 2026-01-12
**Purpose:** Evaluate alternatives to thotro/arduino-dw1000 for better interrupt handling and maintainability

---

## Summary Table

| Library | GitHub URL | Stars | Forks | Last Update | Status | PlatformIO |
|---------|-----------|-------|-------|-------------|--------|------------|
| thotro/arduino-dw1000 | [Link](https://github.com/thotro/arduino-dw1000) | 565 | 288 | Jun 2019 (v0.9) | Unmaintained | Yes (`thotro/DW1000`) |
| F-Army/arduino-dw1000-ng | [Link](https://github.com/F-Army/arduino-dw1000-ng) | 132 | 67 | Nov 2023 | **Archived** | Yes (`f-army/DW1000-ng`) |
| arduino-dw1000-lite | [Link](https://github.com/Richardn2002/arduino-dw1000-lite) | 13 | 1 | Feb 2021 | Inactive | No |
| esp32-dw1000-lite | [Link](https://github.com/mat6780/esp32-dw1000-lite) | 6 | 0 | 2022 | Inactive | No |
| jaarenhoevel/arduino-dw1000-esp32 | [Link](https://github.com/jaarenhoevel/arduino-dw1000-esp32) | 0 | 0 | Dec 2022 | Unmaintained | No |
| Nightsd01/arduino-dw1000-esp-idf | [Link](https://github.com/Nightsd01/arduino-dw1000-esp-idf) | 4 | 2 | Dec 2022 | Inactive | No |
| RAKWireless/RAK13801_UWB | [Link](https://github.com/RAKWireless/RAK13801_UWB) | 8 | 1 | Sep 2022 | Product-specific | No |
| jremington/UWB-Indoor-Localization_Arduino | [Link](https://github.com/jremington/UWB-Indoor-Localization_Arduino) | 221 | 48 | Active | Maintained | No |
| Makerfabs-ESP32-UWB | [Link](https://github.com/Makerfabs/Makerfabs-ESP32-UWB) | 249 | 67 | Jan 2023 | Product-specific | No |
| Cerdas-UWB-Tracker | [Link](https://github.com/geraicerdas/Cerdas-UWB-Tracker) | 44 | 12 | May 2025 | **Active** | No |
| Decawave/uwb-dw1000 | [Link](https://github.com/Decawave/uwb-dw1000) | 31 | 15 | 2020 | Official driver | No (not Arduino) |

---

## Detailed Analysis

### 1. thotro/arduino-dw1000 (Original Library)

**GitHub:** https://github.com/thotro/arduino-dw1000
**Stars:** 565 | **Forks:** 288
**Last Release:** v0.9 (June 12, 2019)
**License:** Apache 2.0

**Key Features:**
- Original Arduino DW1000 library
- Stable message transmission between modules
- Device tuning with multiple operational modes
- Interrupt-based handling via `attachSentHandler()` and `attachReceivedHandler()`

**Known Issues:**
1. **IRQ Pin Configuration Bug:** Old versions used interrupt number instead of pin number
2. **SPI.usingInterrupt() ESP32 Issue:** Not supported on ESP32, must be commented out
3. **Receive Interrupt Not Firing:** Common issue where TX interrupts work but RX does not
4. **4 Anchor Limit:** Library has a bug limiting anchors to 4 maximum
5. **Power Issues:** Arduino 3.3V output may not provide enough current

**PlatformIO:** Yes - `lib_deps = thotro/DW1000`

**Interrupt Handling:** Requires interrupt pin. No native polling mode support.

---

### 2. F-Army/arduino-dw1000-ng (Next Generation Fork)

**GitHub:** https://github.com/F-Army/arduino-dw1000-ng
**Stars:** 132 | **Forks:** 67
**Last Update:** November 9, 2023 (ARCHIVED)
**License:** MIT + Apache 2.0 (dual license for forked files)

**Key Differences from thotro:**
- Antenna delay calibration support (missing in original)
- Rewrote ranging logic - provides functions to send TWR frames instead of a ranging loop
- Base driver rewritten for more independent API functions
- Better documented configuration options
- ESP8266 support (done), ESP32 (testing), STM32 BluePill (experimental)
- Includes `initializeNoInterrupt()` function for some examples

**Known Issues:**
1. **ESP32 SPI Concurrency:** `SPI.usingInterrupt()` causes deadlock with "interrupt wdt" error
2. **ESP32 ISR Problem:** Long-term/blocking processes from ISR cause watchdog reboots
3. **Archived Status:** No longer maintained as of Nov 2023

**Proposed Fixes (in PRs):**
- PR #164: Workqueue approach with `intFlag` and `workqueue()` function
- PR #178: Delegate interrupt handling to separate task (ESP32)

**PlatformIO:** Yes - `lib_deps = f-army/DW1000-ng`

**Interrupt Handling:** Primary interrupt-based, but has `initializeNoInterrupt()` for some use cases

---

### 3. arduino-dw1000-lite (Simplified Library)

**GitHub:** https://github.com/Richardn2002/arduino-dw1000-lite
**Stars:** 13 | **Forks:** 1
**Last Update:** February 2021
**License:** MIT

**Key Features:**
- Extreme simplicity - not a proper Arduino library (no ZIP installation)
- Fixed configuration: 6.8 Mbps, 16 MHz PRF, 128 preamble, channel 5
- Multiple module support on single Arduino
- Delayed/Immediate Message Transmission/Reception
- No advanced features (no double buffering, frame filtering, power management)

**Key Differences:**
- "For high-school-ish projects" - intentionally simplified
- No configuration options (fixed settings only)
- Based on arduino-dw1000-ng but not a fork

**Known Issues:**
- Incomplete API documentation
- No reconfiguration capability
- Not suitable for production use

**PlatformIO:** No (not a standard library)

**Interrupt Handling:** Not documented - likely minimal

---

### 4. esp32-dw1000-lite (ESP32 Polling Mode)

**GitHub:** https://github.com/mat6780/esp32-dw1000-lite
**Stars:** 6 | **Forks:** 0
**Last Update:** 2022
**License:** MIT

**Key Features:**
- **POLLING MODE IMPLEMENTATION** - GPIO27 IRQ pin unused
- ESP32-specific (NodeMCU/DevKitC)
- Double-sided two-way ranging (DS-TWR)
- Ranging precision: +/- 10cm (50-500cm lab conditions)
- Uses ESP-NOW for anchor coordination (2.4 GHz, not UWB)
- Formalized antenna calibration

**Key Differences:**
- **Removes interrupt-based handling entirely**
- Three-device architecture: Master/Slave Initiators + Responder/Tag
- Direction estimation via distance differentials

**Hardware Requirements:**
- ESP32 (NodeMCU/DevKitC)
- DWS1000 shield with DWM1000 module
- SPI: GPIO5, 18, 23, 19 + RST: GPIO25

**Known Issues:**
- Limited to DS-TWR only
- Three-device minimum requirement
- ESP32-only (not Arduino Uno compatible)

**PlatformIO:** No

**Interrupt Handling:** **POLLING MODE - NO INTERRUPT NEEDED**

---

### 5. jaarenhoevel/arduino-dw1000-esp32

**GitHub:** https://github.com/jaarenhoevel/arduino-dw1000-esp32
**Stars:** 0 | **Forks:** 0
**Last Update:** December 2022
**License:** Apache 2.0

**Key Differences:**
- Fork of thotro specifically for ESP32
- Minor modifications for ESP32 compatibility

**Known Issues:**
- Same as thotro original
- Not actively maintained
- Very limited community

**PlatformIO:** No

---

### 6. Nightsd01/arduino-dw1000-esp-idf

**GitHub:** https://github.com/Nightsd01/arduino-dw1000-esp-idf
**Stars:** 4 | **Forks:** 2
**Last Update:** December 2022
**License:** MIT

**Key Differences:**
- Fork of arduino-dw1000-ng for ESP-IDF (not Arduino framework)
- Added `component.mk` file for ESP-IDF component use
- Updates examples for custom SPI
- Bug fix in DW1000Ng.cpp

**Known Issues:**
- ESP-IDF only (not Arduino IDE compatible)
- Not actively developed

**PlatformIO:** Potentially with ESP-IDF framework

---

### 7. RAKWireless/RAK13801_UWB

**GitHub:** https://github.com/RAKWireless/RAK13801_UWB
**Stars:** 8 | **Forks:** 1
**Last Update:** September 2022
**License:** MIT

**Key Features:**
- Based on arduino-dw1000-ng
- TOF (Two-way ranging) and TDOA support
- Configurable: data rates, channels, preamble lengths
- Centimeter accuracy distance measurement
- Offloads positioning calculations to server

**Key Differences:**
- RAK13801 module-specific
- Server-side positioning calculation architecture

**Known Issues:**
- Hardware-specific (RAK13801 only)
- Limited community

**PlatformIO:** No

---

### 8. jremington/UWB-Indoor-Localization_Arduino

**GitHub:** https://github.com/jremington/UWB-Indoor-Localization_Arduino
**Stars:** 221 | **Forks:** 48
**Last Update:** Active (2024-2025)
**License:** GPL-3.0

**Key Features:**
- Complete indoor positioning system
- 2D positioning with 3-4 anchors
- 3D positioning with 4+ anchors
- Linear least squares algorithm
- +/- 10cm accuracy with calibration
- Range: 33m (standard) to 50m+ (high-power)
- Includes calibration tools and test simulations

**Key Differences:**
- Uses modified thotro library with fixes
- Fixes for >5 tags/anchors (Pizzolato Davide version)
- ESP32_Arduino IDE compatible
- Complete positioning solution, not just ranging

**Known Issues:**
- Original DW1000 library limited to 4 anchors
- Requires specific library modifications

**PlatformIO:** No (but could be adapted)

---

### 9. Makerfabs-ESP32-UWB

**GitHub:** https://github.com/Makerfabs/Makerfabs-ESP32-UWB
**Stars:** 249 | **Forks:** 67
**Last Update:** January 2023 (v4.3)
**License:** Not specified

**Key Features:**
- Bundled modified DW1000 library (`mf_DW1000.zip`)
- Multiple hardware versions: Basic (45m), Pro (200m), Pro+Display
- Indoor positioning demo
- UDP data transmission
- Python visualization tools

**Key Differences:**
- Hardware-specific modifications
- Includes complete positioning examples
- Library renamed to `mf_DW1000`

**Known Issues:**
- Makerfabs hardware-specific
- Must rename library to `DW1000.zip` before installation

**PlatformIO:** No

---

### 10. Cerdas-UWB-Tracker (Most Recently Updated)

**GitHub:** https://github.com/geraicerdas/Cerdas-UWB-Tracker
**Stars:** 44 | **Forks:** 12
**Last Update:** **May 2025** (Most recent!)
**License:** Not specified

**Key Features:**
- ESP32-S3 support (newer chip)
- Dual UWB module support: DWM1000 (20m) and long-range (120m)
- Ai-Thinker BU01 module compatible
- Qwiik connectors for expansion
- Optional BNO080 IMU and RV3028 RTC
- USB-C charging + LiPo battery support

**Key Differences:**
- **Most recently maintained DW1000 project**
- ESP32-S3 specific modifications
- Modern hardware design

**Known Issues:**
- ESP32-S3 specific (not Arduino Uno compatible)
- Hardware platform-specific

**PlatformIO:** No

---

### 11. Decawave/uwb-dw1000 (Official Driver)

**GitHub:** https://github.com/Decawave/uwb-dw1000
**Stars:** 31 | **Forks:** 15
**Last Update:** 2020
**License:** Proprietary (Decawave)

**Key Features:**
- Official uwb-core driver from Decawave
- Hardware and architecture agnostic
- Decawave Porting Layer (DPL) for OS abstraction
- Used with MyNewt OS primarily
- **Native polling mode support** via `dwt_read32bitreg()`

**Polling Pattern:**
```c
// TX polling
dwt_starttx(DWT_START_TX_IMMEDIATE);
while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS)) { };
dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);

// RX frame length
frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
```

**Key Differences:**
- Not Arduino-native (requires porting)
- Professional-grade driver
- Supports polling and interrupt modes
- Deprecated in favor of uwb-core for new designs

**PlatformIO:** No (not Arduino)

**Arduino Compatibility:** Requires significant porting work. Some users have ported to Arduino Nano but requires clock prescaler management.

---

## Libraries with Polling Mode Support

| Library | Polling Support | Notes |
|---------|-----------------|-------|
| esp32-dw1000-lite | **Yes (native)** | IRQ pin unused, full polling implementation |
| Decawave/uwb-dw1000 | **Yes (native)** | Official driver, requires porting |
| F-Army/arduino-dw1000-ng | Partial | `initializeNoInterrupt()` for some examples |
| thotro/arduino-dw1000 | No | Requires custom modification |

---

## Libraries with Better Interrupt Handling

| Library | Interrupt Improvements | Notes |
|---------|----------------------|-------|
| F-Army/arduino-dw1000-ng | PR #164: Workqueue pattern | Avoids ESP32/ESP8266 issues |
| F-Army/arduino-dw1000-ng | PR #178: Separate task for ISR | ESP32-specific fix |
| jremington/UWB-Indoor-Localization | Modified library fixes | Addresses >4 anchor limit |

---

## Recommendations

### For Arduino Uno (Current Hardware):
1. **Continue with thotro/arduino-dw1000** - Most tested on AVR
2. Consider applying fixes from F-Army PRs if needed
3. Hardware jumper fix (D8->D2) remains necessary

### For ESP32 Migration:
1. **esp32-dw1000-lite** - Best for polling mode (no interrupt issues)
2. **Cerdas-UWB-Tracker** - Most recently maintained (May 2025)
3. **jremington/UWB-Indoor-Localization** - Best for complete positioning system

### For Polling Mode (No Interrupt):
1. **esp32-dw1000-lite** - Ready-to-use polling implementation
2. **Decawave official driver** - Requires porting but most robust
3. **Custom modification** - Apply polling pattern to existing library

### For Production Use:
1. Fork F-Army/arduino-dw1000-ng and apply PR #164 or #178
2. Or use Decawave official API with custom Arduino port
3. Consider ESP32 platform for better SPI/interrupt handling

---

## Action Items

1. [ ] Test esp32-dw1000-lite with existing hardware
2. [ ] Evaluate PR #164 workqueue pattern from F-Army
3. [ ] Consider porting Decawave polling pattern to current library
4. [ ] Test with hardware IRQ jumper fix first before library changes

---

## References

- [thotro/arduino-dw1000](https://github.com/thotro/arduino-dw1000)
- [F-Army/arduino-dw1000-ng](https://github.com/F-Army/arduino-dw1000-ng)
- [arduino-dw1000-lite](https://github.com/Richardn2002/arduino-dw1000-lite)
- [esp32-dw1000-lite](https://github.com/mat6780/esp32-dw1000-lite)
- [jremington/UWB-Indoor-Localization_Arduino](https://github.com/jremington/UWB-Indoor-Localization_Arduino)
- [Makerfabs-ESP32-UWB](https://github.com/Makerfabs/Makerfabs-ESP32-UWB)
- [Cerdas-UWB-Tracker](https://github.com/geraicerdas/Cerdas-UWB-Tracker)
- [Decawave/uwb-dw1000](https://github.com/Decawave/uwb-dw1000)
- [RAKWireless/RAK13801_UWB](https://github.com/RAKWireless/RAK13801_UWB)
- [PlatformIO DW1000](https://registry.platformio.org/libraries/thotro/DW1000)
- [PlatformIO DW1000-ng](https://registry.platformio.org/libraries/f-army/DW1000-ng)
- [DW1000 Device Driver API Guide](https://forum.qorvo.com/uploads/default/original/1X/8b220e1e26fea4ebd83f0b0e5ef42eb9a251310d.pdf)
