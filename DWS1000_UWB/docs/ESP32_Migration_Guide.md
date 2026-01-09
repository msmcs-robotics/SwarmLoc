# ESP32 Migration Guide: Arduino Shield to ESP32 DevKit

## Guide for Connecting Qorvo PCL298336 DWM3000EVB Arduino Shield to ESP32

### Date: 2026-01-08

---

## Table of Contents

1. [Can Arduino Shields Be Used with ESP32?](#1-can-arduino-shields-be-used-with-esp32)
2. [ESP32 Arduino Shield Adapter Boards](#2-esp32-arduino-shield-adapter-boards)
3. [Manual Wiring Approach](#3-manual-wiring-approach)
4. [ESP32 Pin Mapping for DWM3000](#4-esp32-pin-mapping-for-dwm3000)
5. [Voltage Level Concerns](#5-voltage-level-concerns)
6. [Ready-Made Solutions](#6-ready-made-solutions)
7. [Best Practices](#7-best-practices)
8. [Complete Wiring Example](#8-complete-wiring-example)
9. [Software Configuration](#9-software-configuration)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Can Arduino Shields Be Used with ESP32?

### Short Answer: Not Directly, But Yes With Adapters or Manual Wiring

**Physical Compatibility Issues:**
- Arduino Uno has a specific header layout (R3 shield format)
- ESP32 DevKit boards do NOT have the same header layout
- Pin spacing and positions are different
- Cannot plug Arduino shields directly into ESP32

**Functional Compatibility:**
- ESP32 supports the same protocols (SPI, I2C, UART, GPIO)
- ESP32 is MORE capable than Arduino Uno (240 MHz vs 16 MHz)
- **Voltage compatibility**: ESP32 is 3.3V native (same as DWM3000)
- **ADVANTAGE**: ESP32 is actually BETTER suited for UWB ranging than Arduino Uno

**Options for Using Arduino Shields with ESP32:**
1. Use an adapter board (mechanically connects shield to ESP32)
2. Manual jumper wire connections (direct pin-to-pin wiring)
3. Design a custom PCB adapter
4. Use breadboard for prototyping

---

## 2. ESP32 Arduino Shield Adapter Boards

### Commercial Adapter Options

While I cannot search for current product availability, based on common solutions in the maker community:

#### Option A: ESP32 DevKit to UNO Adapter Board
**Description:** Adapter boards that provide Arduino Uno R3 header layout on top, ESP32 DevKit socket on bottom

**Typical Features:**
- Arduino Uno R3 shield-compatible headers
- ESP32 DevKit (30-pin) socket
- Pin mapping built-in
- Sometimes includes voltage level shifters (not needed for 3.3V shields)

**Where to Search:**
- Amazon: "ESP32 Arduino UNO adapter"
- AliExpress: "ESP32 UNO shield adapter board"
- eBay: "ESP32 DevKit Arduino shield adapter"
- Tindie: Custom maker adapter boards
- Local electronics suppliers

**Expected Price Range:** $5-15 USD

**Pros:**
- Plug-and-play solution
- Clean appearance
- Reusable for other shields
- No soldering required

**Cons:**
- Additional cost
- Adds height to stack
- May have fixed pin mappings
- Shipping delay if ordering online

#### Option B: Breakout Board Adapters
**Description:** ESP32 mounted on a board with Arduino-compatible headers

**Examples:**
- Some ESP32 boards come pre-designed with Arduino headers
- Custom PCB services (OSH Park, PCBWay) for DIY adapters

---

## 3. Manual Wiring Approach

### Required Materials

- **ESP32 DevKit board** (ESP32-WROOM-32 or similar)
- **Breadboard** (full-size or half-size)
- **Jumper wires** (male-to-female, approximately 10 wires)
- **PCL298336 shield** (your existing DWM3000EVB)
- **Breakaway headers** (optional, for shield access)

### Connection Strategy

Since the PCL298336 is an Arduino shield with female headers designed to plug onto Arduino pins, you have two approaches:

#### Approach 1: Direct Shield Access
If you can access the shield's pin headers from the bottom:
- Use **male-to-female jumper wires**
- Connect directly from ESP32 GPIO pins to shield header pins

#### Approach 2: Shield on Breadboard
If the shield has male header pins underneath or you add breakaway headers:
- Mount shield headers into breadboard
- Use **male-to-male jumpers** from breadboard to ESP32

---

## 4. ESP32 Pin Mapping for DWM3000

### Standard SPI Pin Assignments

ESP32 has two SPI buses available:
- **VSPI** (Virtual SPI) - Default for user applications
- **HSPI** (Hardware SPI) - Alternative

**We'll use VSPI (recommended):**

### Complete Pin Mapping Table

| Function | Arduino Uno Pin | Shield Pin | ESP32 VSPI Pin | ESP32 GPIO# | Notes |
|----------|----------------|------------|----------------|-------------|-------|
| **MOSI** | D11 | MOSI | VSPI MOSI | **GPIO 23** | SPI Master Out Slave In |
| **MISO** | D12 | MISO | VSPI MISO | **GPIO 19** | SPI Master In Slave Out |
| **SCK** | D13 | SCK | VSPI CLK | **GPIO 18** | SPI Clock |
| **CS** | D10 | CS/SS | Configurable | **GPIO 5** | Chip Select (configurable) |
| **IRQ** | D2 | IRQ | Interrupt-capable | **GPIO 4** | Interrupt Request |
| **RST** | D9 | RST | Any GPIO | **GPIO 16** | Hardware Reset |
| **3.3V** | 3.3V | VCC | 3.3V | 3.3V | Power Supply |
| **GND** | GND | GND | GND | GND | Ground |

### Alternative Pin Options

If the recommended pins conflict with other peripherals on your ESP32:

**Alternative CS (Chip Select):**
- GPIO 5 (recommended)
- GPIO 15
- GPIO 2
- Any free GPIO (software configurable)

**Alternative IRQ (Interrupt):**
- GPIO 4 (recommended)
- GPIO 17
- GPIO 21
- GPIO 22
- Any interrupt-capable GPIO (most ESP32 GPIOs support interrupts)

**Alternative RST (Reset):**
- GPIO 16 (recommended)
- GPIO 17
- GPIO 25
- GPIO 26
- Any free GPIO

### Pins to AVOID on ESP32

**Do NOT use these pins for DWM3000:**
- **GPIO 0**: Boot mode selection (pulled HIGH on boot)
- **GPIO 2**: Boot mode selection (must be LOW on boot)
- **GPIO 5**: Boot strapping pin (use with caution)
- **GPIO 12**: Boot voltage selection (MTDI)
- **GPIO 15**: Boot mode selection (MTDO)
- **GPIO 6-11**: Connected to flash memory (DO NOT USE)
- **GPIO 34-39**: Input-only pins (cannot be used for RST/CS)

**Safe GPIO pins for general use:**
- GPIO 4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33

---

## 5. Voltage Level Concerns

### Voltage Compatibility Analysis

#### Arduino Uno Shield Standards
- **Logic Level**: 5V on Arduino Uno **BUT**
- **DWM3000 Module**: Operates at **3.3V** (has onboard regulator)
- **PCL298336 Shield**: Has 3.3V voltage regulator onboard

#### ESP32 Specifications
- **Native Logic Level**: 3.3V
- **GPIO Voltage Tolerance**: **NOT 5V tolerant** (3.6V max)
- **3.3V Output**: Can provide up to 40mA per pin

### Why This Is SAFE

The PCL298336 shield is designed for 3.3V operation:

1. **Shield has onboard 3.3V regulator**
   - Takes 5V from Arduino Uno's 5V pin
   - Regulates down to 3.3V for DWM3000 chip
   - All signal lines (MOSI, MISO, SCK, etc.) operate at **3.3V**

2. **ESP32 provides 3.3V power**
   - ESP32's 3.3V output can power the DWM3000 directly
   - **Do NOT connect to 5V rail on shield** (if exposed)
   - Connect ESP32 3.3V to shield's 3.3V or VCC pin

3. **No level shifters needed**
   - ESP32 3.3V signals → DWM3000 3.3V inputs: SAFE
   - DWM3000 3.3V signals → ESP32 3.3V inputs: SAFE
   - Perfect match!

### CRITICAL: Power Supply Notes

**Current Requirements:**
- **DWM3000 typical consumption**: 100-150 mA during TX
- **ESP32 3.3V regulator**: Typically 600-800 mA capacity
- **Result**: ESP32 can safely power DWM3000

**Power Wiring:**
- Connect ESP32 **3.3V** pin → Shield **3.3V/VCC**
- Connect ESP32 **GND** → Shield **GND**
- **DO NOT** connect anything to 5V on shield (not needed with ESP32)

**Decoupling Capacitors (Recommended):**
- Add 10µF and 100nF capacitors between 3.3V and GND
- Place close to DWM3000 module
- Reduces noise and voltage sag during TX bursts

---

## 6. Ready-Made Solutions

### Hardware Solutions

#### Option 1: Makerfabs ESP32 UWB Module
**Instead of adapting the shield, use integrated ESP32+DWM3000 boards**

- **Product**: Makerfabs ESP32 UWB DW3000
- **Features**: ESP32 and DWM3000 on single board
- **Advantage**: No wiring needed, proven design
- **Library Support**: Excellent (https://github.com/Makerfabs/Makerfabs-ESP32-UWB-DW3000)
- **Cost**: ~$25-35 per board

**Search Terms:**
- "Makerfabs ESP32 UWB DW3000"
- "ESP32 DWM3000 module"
- "ESP32 UWB development board"

#### Option 2: DWM3000 Breakout Boards + ESP32
**Use DWM3000 breakout boards instead of Arduino shield**

- **Products**: Various DWM3000 breakout boards
- **Advantage**: Easier breadboard integration
- **Wiring**: Same as manual approach below

#### Option 3: Custom PCB Adapter
**For production or permanent installation**

- Design a simple adapter PCB
- Services: OSH Park, PCBWay, JLCPCB
- Cost: ~$5-10 for 3 boards (5-10 day shipping)

### Software Solutions

#### Library: Fhilb/DW3000_Arduino
**Best ESP32 library for DWM3000**

- **Repository**: https://github.com/Fhilb/DW3000_Arduino
- **Platform**: ESP32 (Arduino framework)
- **Features**:
  - Full TWR (Two-Way Ranging) support
  - Proven accuracy (5-10 cm)
  - Active development
  - Good documentation

#### Library: Makerfabs ESP32-UWB-DW3000
**Alternative well-documented library**

- **Repository**: https://github.com/Makerfabs/Makerfabs-ESP32-UWB-DW3000
- **Platform**: ESP32
- **Features**:
  - Complete examples
  - Position tracking system
  - Wiki documentation

---

## 7. Best Practices

### Hardware Best Practices

1. **Power Supply**
   - Use USB power for development (stable 3.3V regulation)
   - Add decoupling capacitors (10µF + 100nF near DWM3000)
   - Monitor voltage under load (should stay >3.2V)

2. **Signal Integrity**
   - Keep SPI wires short (<15 cm ideal, <30 cm maximum)
   - Avoid running wires parallel to noisy signals
   - Use twisted pair for clock signals if wires are long
   - Keep ground wire as short as signal wires

3. **Connections**
   - Double-check pin connections before power-up
   - Use color-coded wires (e.g., red=3.3V, black=GND)
   - Document your wiring with photos
   - Use breadboard or perfboard for stable connections

4. **Mechanical Stability**
   - Secure ESP32 and shield to prevent movement
   - Use breadboard or mount on board
   - Ensure antenna has clearance (no metal nearby)

5. **Antenna Placement**
   - Keep antenna away from metal objects (>2 cm clearance)
   - Orient antennas for line-of-sight when testing
   - Avoid touching antenna during operation

### Software Best Practices

1. **Initialization Sequence**
   ```cpp
   // 1. Hardware reset (essential)
   pinMode(PIN_RST, OUTPUT);
   digitalWrite(PIN_RST, LOW);
   delay(10);
   digitalWrite(PIN_RST, HIGH);
   delay(10);

   // 2. SPI initialization
   SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

   // 3. DW3000 initialization
   // (library-specific)
   ```

2. **SPI Configuration**
   - SPI Mode: Typically MODE0
   - SPI Speed: Start at 1 MHz, increase to 4-8 MHz if stable
   - Bit Order: MSB first

3. **Interrupt Handling**
   - Attach interrupt to IRQ pin
   - Use RISING edge trigger
   - Keep interrupt handler short
   - Set flag, process in main loop

4. **Debugging**
   - Enable serial output (115200 baud)
   - Print device ID on startup to verify communication
   - Log timestamp values for ranging analysis
   - Monitor for SPI timeouts

5. **Calibration**
   - Measure known distances (1m, 2m, 5m)
   - Calculate antenna delay correction
   - Apply systematic offset correction
   - Re-test after calibration

### Development Workflow

1. **Start Simple**
   - Test basic SPI communication first
   - Read device ID register
   - Verify interrupt fires
   - Then implement ranging

2. **Incremental Testing**
   - Test transmit-only mode
   - Test receive-only mode
   - Test basic ranging
   - Refine accuracy

3. **Documentation**
   - Photo document your wiring
   - Record pin mappings in code comments
   - Log calibration values
   - Note any modifications to hardware

---

## 8. Complete Wiring Example

### Physical Setup

```
┌─────────────────────────────────────────────────────┐
│  PCL298336 DWM3000EVB Shield (Arduino Format)       │
│  (Sitting on breadboard or with wire access)        │
│                                                      │
│  [Arduino Header Pins - Bottom Side]                │
│   MOSI MISO SCK  CS  IRQ RST  3.3V GND             │
│    │    │    │   │   │   │     │   │               │
└────┼────┼────┼───┼───┼───┼─────┼───┼───────────────┘
     │    │    │   │   │   │     │   │
     │    │    │   │   │   │     │   │  Jumper Wires
     │    │    │   │   │   │     │   │
     ▼    ▼    ▼   ▼   ▼   ▼     ▼   ▼
┌────────────────────────────────────────────────────┐
│           ESP32 DevKit (30-pin)                    │
│                                                    │
│  GND  23  19  18  5   4   16        3.3V   GND    │
│       │   │   │   │   │   │          │      │     │
│      MOSI MISO SCK CS IRQ RST       VCC    GND    │
│                                                    │
│         [USB Cable to Computer]                   │
└────────────────────────────────────────────────────┘
```

### Step-by-Step Wiring Instructions

**Materials needed:**
- 8x male-to-female jumper wires (if accessing shield from bottom)
- OR 8x male-to-male jumper wires (if shield mounted in breadboard)
- Breadboard (optional but recommended)
- ESP32 DevKit board
- PCL298336 shield

**Wiring Steps:**

1. **Prepare the Shield**
   - If shield has accessible header pins on bottom: ready to use
   - If not: solder male header pins to shield's Arduino header positions
   - Or mount shield upside-down in breadboard

2. **Connect Power FIRST (Critical)**
   ```
   Wire 1: Shield 3.3V → ESP32 3.3V pin (red wire)
   Wire 2: Shield GND → ESP32 GND pin (black wire)
   ```

3. **Connect SPI Bus**
   ```
   Wire 3: Shield MOSI (D11) → ESP32 GPIO 23 (yellow wire)
   Wire 4: Shield MISO (D12) → ESP32 GPIO 19 (green wire)
   Wire 5: Shield SCK (D13)  → ESP32 GPIO 18 (blue wire)
   ```

4. **Connect Control Pins**
   ```
   Wire 6: Shield CS (D10)   → ESP32 GPIO 5 (white wire)
   Wire 7: Shield IRQ (D2)   → ESP32 GPIO 4 (orange wire)
   Wire 8: Shield RST (D9)   → ESP32 GPIO 16 (brown wire)
   ```

5. **Verification Checklist**
   - [ ] All 8 connections made
   - [ ] No loose connections
   - [ ] Power connections correct (3.3V to 3.3V, GND to GND)
   - [ ] No shorts between adjacent pins
   - [ ] Shield antenna has clearance

6. **Power Up**
   - Connect ESP32 to computer via USB
   - Check that ESP32 power LED lights
   - Check that shield power LED lights (if present)

---

## 9. Software Configuration

### PlatformIO Configuration

Create or modify `platformio.ini`:

```ini
[env:esp32_dwm3000]
platform = espressif32
board = esp32dev
framework = arduino
upload_speed = 921600
monitor_speed = 115200

build_flags =
    -DDEVICE_ROLE=INITIATOR
    -DDEVICE_ADDRESS=0x01
    -DESP32_PLATFORM=1

lib_deps =
    https://github.com/Fhilb/DW3000_Arduino.git
    SPI
```

### Pin Definition Code

Add to your code (beginning of file):

```cpp
// ESP32 pin definitions for DWM3000 shield
#define PIN_SCK     18    // VSPI SCK
#define PIN_MISO    19    // VSPI MISO
#define PIN_MOSI    23    // VSPI MOSI
#define PIN_CS      5     // Chip Select
#define PIN_IRQ     4     // Interrupt Request
#define PIN_RST     16    // Reset

// SPI settings
#define SPI_SPEED   4000000  // 4 MHz (safe starting point)
```

### Initialization Code Example

```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("ESP32 DWM3000 Initialization");
    Serial.println("=============================");

    // Configure pins
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_IRQ, INPUT);

    // Hardware reset sequence
    Serial.println("Performing hardware reset...");
    digitalWrite(PIN_RST, LOW);
    delay(10);
    digitalWrite(PIN_RST, HIGH);
    delay(10);

    // Initialize SPI
    Serial.println("Initializing SPI...");
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    // Initialize DW3000 library
    Serial.println("Initializing DW3000...");
    // (Library-specific initialization code here)

    Serial.println("Initialization complete!");
}
```

### Testing Communication

First test: Read device ID to verify SPI communication:

```cpp
// Verify SPI communication by reading device ID
// Expected value depends on DWM3000 version
// Typically: 0xDECA0302 for DW3000
uint32_t deviceID = readDeviceID();  // Library function
Serial.print("Device ID: 0x");
Serial.println(deviceID, HEX);

if (deviceID == 0xDECA0302) {
    Serial.println("✓ DWM3000 detected!");
} else {
    Serial.println("✗ Communication failed - check wiring!");
}
```

---

## 10. Troubleshooting

### Common Issues and Solutions

#### Issue 1: No Device ID / SPI Communication Failure

**Symptoms:**
- Cannot read device ID
- Device ID returns 0x00000000 or 0xFFFFFFFF
- No response from module

**Solutions:**
1. **Check wiring**
   - Verify all SPI connections (MOSI, MISO, SCK, CS)
   - Ensure good contact (re-seat wires)
   - Check for reversed MOSI/MISO

2. **Check power**
   - Measure voltage at DWM3000: should be 3.2-3.4V
   - Verify GND connection
   - Check for voltage drop (add capacitors)

3. **Check chip select**
   - CS should be HIGH when idle
   - CS goes LOW during SPI transaction
   - Verify CS pin number in code matches wiring

4. **SPI settings**
   - Try lower SPI speed (1 MHz)
   - Verify SPI mode (usually MODE0)
   - Check bit order (MSB first)

#### Issue 2: Interrupt Not Firing

**Symptoms:**
- No interrupt events
- Ranging doesn't start
- Code hangs waiting for interrupt

**Solutions:**
1. **Check IRQ wiring**
   - Verify IRQ pin connection
   - Ensure good contact

2. **Check interrupt configuration**
   - Verify GPIO number in code
   - Check trigger type (RISING edge)
   - Confirm interrupt is attached

3. **Test with polling**
   - Temporarily disable interrupts
   - Poll interrupt status register
   - Helps isolate wiring vs software issue

#### Issue 3: Module Resets or Unstable

**Symptoms:**
- Module resets randomly
- Inconsistent behavior
- Works intermittently

**Solutions:**
1. **Add decoupling capacitors**
   - 10µF electrolytic + 100nF ceramic
   - Place very close to DWM3000 3.3V/GND pins

2. **Check current capacity**
   - ESP32 3.3V regulator should provide 600+ mA
   - DWM3000 draws ~150 mA peak during TX

3. **Reduce transmit power**
   - Lower TX power in configuration
   - Reduces peak current draw

4. **Check USB power**
   - Use powered USB hub if computer USB is weak
   - Or use external 5V power supply

#### Issue 4: Poor Ranging Accuracy

**Symptoms:**
- Distance measurements have large offset
- Inconsistent readings
- Accuracy worse than 50 cm

**Solutions:**
1. **Calibrate antenna delay**
   - Measure at known distance
   - Adjust antenna delay parameter
   - Typical value: 16400-16500 time units

2. **Environmental factors**
   - Test in open area (reduce multipath)
   - Remove metal objects near antennas
   - Ensure line-of-sight

3. **Check clock drift**
   - Verify Double-Sided TWR is used
   - Check timestamp capture timing
   - Monitor for clock synchronization issues

4. **Software timing**
   - ESP32 at 240 MHz is much better than Arduino Uno 16 MHz
   - Ensure no blocking delays during ranging
   - Process interrupts promptly

#### Issue 5: Ranging Works But Unstable

**Symptoms:**
- Sometimes works, sometimes fails
- Distance jumps erratically
- Packet loss

**Solutions:**
1. **State machine issues**
   - Review state transitions
   - Add timeout handling
   - Reset to known state on error

2. **Buffer management**
   - Clear RX buffer before new transaction
   - Check for buffer overruns
   - Verify message framing

3. **Timing issues**
   - Add delays between state transitions
   - Allow radio to settle after TX
   - Ensure proper TX-to-RX switching

### Debug Checklist

When things don't work:

- [ ] Verify all 8 wire connections
- [ ] Measure 3.3V at DWM3000 module
- [ ] Check continuity of all connections
- [ ] Try lower SPI speed (1 MHz)
- [ ] Read device ID successfully
- [ ] See interrupt pin change state (oscilloscope/logic analyzer)
- [ ] Test with known working example code
- [ ] Add debug serial output everywhere
- [ ] Test each device independently
- [ ] Check for soldering issues on shield
- [ ] Verify ESP32 board is functional (blink test)

---

## 11. Reference Resources

### Hardware Datasheets

- **DWM3000 Datasheet**: https://download.mikroe.com/documents/datasheets/DWM3000_datasheet.pdf
- **DW3000 User Manual**: Available from Qorvo website
- **ESP32 Datasheet**: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf
- **ESP32 Technical Reference**: https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf

### Software Libraries

- **Fhilb/DW3000_Arduino** (Best for ESP32): https://github.com/Fhilb/DW3000_Arduino
- **Makerfabs ESP32-UWB-DW3000**: https://github.com/Makerfabs/Makerfabs-ESP32-UWB-DW3000
- **Circuit-Digest RTLS**: https://github.com/Circuit-Digest/ESP32-DWM3000-UWB-Indoor-RTLS-Tracker

### Tutorials and Guides

- **CircuitDigest Tutorial**: https://circuitdigest.com/microcontrollers-projects/diy-indoor-uwb-positioning-system-using-esp32-and-qorvo-dwm3000
- **Instructables Tutorial**: https://www.instructables.com/UWB-Indoor-Positioning-System-With-ESP32-and-Qorvo/
- **How2Electronics Guide**: https://how2electronics.com/ranging-localization-with-esp32-uwb-dw3000-module/
- **Makerfabs Wiki**: https://wiki.makerfabs.com/ESP32_DW3000_UWB.html

### Community Forums

- **Qorvo UWB Forum**: https://forum.qorvo.com/c/ultra-wideband/13
- **ESP32 Forum**: https://esp32.com/
- **Arduino Forum - DWM3000**: https://forum.arduino.cc/t/dwm-3000-collaborative-group/897672

### Shopping Lists

**For Manual Wiring Approach:**
- 2x ESP32 DevKit boards (ESP32-WROOM-32)
- Breadboard (830 points recommended)
- Jumper wire kit (male-to-female and male-to-male)
- 10µF capacitors (2x minimum)
- 100nF capacitors (2x minimum)
- USB cables (micro-USB or USB-C depending on ESP32 board)

**Search Terms for Adapters:**
- "ESP32 Arduino UNO shield adapter"
- "ESP32 prototype shield"
- "ESP32 DevKit breakout board Arduino compatible"

---

## 12. Summary and Recommendations

### Key Findings

1. **Yes, Arduino shields CAN be used with ESP32** - but not by direct plugging
2. **Adapter boards exist** - search for "ESP32 Arduino UNO shield adapter"
3. **Manual wiring is simple** - only 8 connections needed
4. **Voltage is SAFE** - ESP32 and DWM3000 both use 3.3V (perfect match)
5. **ESP32 is BETTER than Arduino Uno** for UWB ranging (240 MHz vs 16 MHz)
6. **Proven solutions exist** - multiple working libraries and tutorials

### Recommended Approach

**For Development/Prototyping:**
1. Use manual jumper wire approach (fastest, cheapest)
2. Wire as specified in Section 4
3. Use breadboard for stable connections
4. Test with Fhilb library

**For Production/Permanent:**
1. Purchase adapter board OR
2. Design custom PCB adapter OR
3. Switch to integrated ESP32+DWM3000 modules (Makerfabs)

### Expected Results

**With ESP32 (vs Arduino Uno):**
- Better timing precision (240 MHz vs 16 MHz)
- More memory (520 KB SRAM vs 2 KB)
- Better floating point performance
- Built-in WiFi/Bluetooth (bonus)
- **Expected accuracy**: 5-10 cm (vs 20-50 cm on Uno)

### Next Steps

1. Gather materials (ESP32 boards, wires, breadboard)
2. Wire first ESP32 according to Section 8
3. Upload test code from Section 9
4. Verify SPI communication (device ID read)
5. Test basic ranging
6. Wire second ESP32
7. Implement full TWR protocol
8. Calibrate and refine

---

## Appendix A: Alternative Pin Mappings

If you need to use different pins, here are alternatives:

### HSPI Bus (Alternative to VSPI)

| Function | HSPI Pin | GPIO# |
|----------|----------|-------|
| MOSI | HSPI MOSI | GPIO 13 |
| MISO | HSPI MISO | GPIO 12 |
| SCK | HSPI CLK | GPIO 14 |
| CS | Configurable | GPIO 15 |

**Note**: HSPI may conflict with some ESP32 boards' onboard peripherals

### Custom Soft SPI

ESP32 can bit-bang SPI on any GPIO pins if needed (slower, but flexible)

---

## Appendix B: Power Budget Analysis

### ESP32 Power Consumption
- Active mode (WiFi off): ~80 mA
- Peak: ~240 mA

### DWM3000 Power Consumption
- Idle/RX: ~15 mA
- TX (peak): ~120-150 mA
- Average (ranging): ~40-60 mA

### Total System
- Peak: ~400 mA
- Average: ~150-200 mA
- **USB can provide**: 500 mA minimum
- **Result**: Safe with USB power

### Recommendations
- USB power is sufficient for development
- For battery operation: use 3.7V LiPo with LDO or buck converter
- Add 10µF + 100nF decoupling capacitors

---

## Appendix C: Comparison Table

| Feature | Arduino Uno | ESP32 DevKit |
|---------|-------------|--------------|
| CPU Speed | 16 MHz | 240 MHz (15x faster) |
| SRAM | 2 KB | 520 KB (260x more) |
| Flash | 32 KB | 4 MB (125x more) |
| Voltage | 5V logic | 3.3V logic |
| WiFi/BT | No | Yes (built-in) |
| Cost | ~$25 | ~$8-12 |
| UWB Accuracy | ±20-50 cm | ±5-10 cm |
| Library Support | Poor (DWM3000) | Excellent |
| Shield Compatibility | Direct plug | Requires adapter/wiring |

**Conclusion**: ESP32 is superior for UWB ranging despite requiring manual wiring

---

**Document Version**: 1.0
**Last Updated**: 2026-01-08
**Author**: SwarmLoc Project Documentation
**Status**: Complete Migration Guide
