# Quick Reference: Connecting PCL298336 DWM3000EVB Shield to ESP32

## Your Specific Hardware: Qorvo PCL298336 v1.3 Arduino Shield

---

## Question 1: Can Arduino Shields Be Used with ESP32?

### Answer: YES, but not by direct plugging

**Physical Reality:**
- Arduino shields have Arduino Uno R3 header layout
- ESP32 DevKit has different pin layout
- **Cannot plug shield directly onto ESP32**

**But It Works With:**
- Adapter boards (mechanical conversion)
- Manual jumper wiring (8 connections)
- Custom PCB adapters

**The Good News:**
- ESP32 is actually BETTER for UWB ranging than Arduino Uno
- 240 MHz vs 16 MHz = 15x faster processing
- Better timing precision = better accuracy (5-10 cm vs 20-50 cm)
- Your PCL298336 uses 3.3V - perfect match for ESP32!

---

## Question 2: ESP32 Arduino Shield Adapter Boards

### Yes, Adapters Exist!

**Search Terms (for online shopping):**
- "ESP32 Arduino UNO shield adapter"
- "ESP32 DevKit Arduino shield adapter board"
- "ESP32 UNO prototype shield adapter"
- "ESP32 30-pin to Arduino UNO adapter"

**Where to Search:**
- Amazon
- AliExpress
- eBay
- Tindie (maker marketplace)
- Adafruit / SparkFun

**Expected Price:** $5-15 USD

**What to Look For:**
- Arduino Uno R3 header layout on top
- ESP32 DevKit (30-pin) socket on bottom
- 3.3V compatible (most are, but verify)
- Pin mapping documentation included

**Product Types:**

1. **ESP32 to UNO Adapter Board**
   - Direct mechanical adapter
   - Maps ESP32 pins to Arduino headers
   - Usually has silk screen labels

2. **ESP32 Prototype Shield**
   - ESP32 mounted on Arduino-shaped board
   - Has Arduino headers built-in
   - More expensive but cleaner

**Caveat:** Pin mappings may be fixed, so verify SPI pins match what you need.

---

## Question 3: Manual Wiring Approach

### Only 8 Connections Needed!

Your shield connections from Arduino Uno pins:

| Signal | Arduino Pin | Where It Goes |
|--------|-------------|---------------|
| MOSI | D11 | SPI data out |
| MISO | D12 | SPI data in |
| SCK | D13 | SPI clock |
| CS | D10 | Chip select |
| IRQ | D2 | Interrupt |
| RST | D9 | Reset |
| 3.3V | 3.3V | Power |
| GND | GND | Ground |

**How to Access Shield Pins:**

**Method A:** If shield has male headers underneath
- Use male-to-female jumper wires
- Connect directly to shield header pins

**Method B:** If shield only has female headers
- Insert male headers into breadboard
- Shield sits above breadboard
- Use male-to-male jumpers from breadboard to ESP32

**Method C:** Solder wires directly
- For permanent installation
- Solder to shield's header pins

---

## Question 4: ESP32 Pin Mapping

### Recommended Connections

| Shield Signal | Shield Pin | ESP32 GPIO | ESP32 Function |
|---------------|------------|------------|----------------|
| **MOSI** | D11 | **GPIO 23** | VSPI MOSI |
| **MISO** | D12 | **GPIO 19** | VSPI MISO |
| **SCK** | D13 | **GPIO 18** | VSPI SCK |
| **CS** | D10 | **GPIO 5** | Chip Select (configurable) |
| **IRQ** | D2 | **GPIO 4** | Interrupt (any GPIO works) |
| **RST** | D9 | **GPIO 16** | Reset (any GPIO works) |
| **3.3V** | 3.3V | **3.3V** | Power supply |
| **GND** | GND | **GND** | Ground |

**Visual Wiring:**
```
PCL298336 Shield              ESP32 DevKit
─────────────────            ──────────────
MOSI (D11)    ──────────────> GPIO 23
MISO (D12)    ──────────────> GPIO 19
SCK  (D13)    ──────────────> GPIO 18
CS   (D10)    ──────────────> GPIO 5
IRQ  (D2)     ──────────────> GPIO 4
RST  (D9)     ──────────────> GPIO 16
3.3V          ──────────────> 3.3V
GND           ──────────────> GND
```

### Alternative Pins (if needed)

**CS (Chip Select):** Any GPIO
- GPIO 5 (recommended)
- GPIO 15, 2, or any free GPIO

**IRQ (Interrupt):** Any interrupt-capable GPIO
- GPIO 4 (recommended)
- GPIO 17, 21, 22, 25, 26, 27

**RST (Reset):** Any GPIO
- GPIO 16 (recommended)
- GPIO 17, 25, 26, 32, 33

### Pins to AVOID

**Never use these ESP32 pins:**
- GPIO 6-11: Connected to flash (will brick ESP32)
- GPIO 34-39: Input-only (can't drive CS or RST)
- GPIO 0, 2, 15: Boot strapping pins (use with caution)

---

## Question 5: Voltage Level Concerns

### Perfect Match - No Level Shifters Needed!

**Your Shield (PCL298336):**
- Operates at **3.3V logic**
- Has onboard voltage regulator (converts 5V to 3.3V)
- All signal pins are 3.3V level

**ESP32:**
- Native **3.3V logic**
- NOT 5V tolerant (max 3.6V)
- 3.3V output on power pin

**Result:** ✓ Perfect Compatibility!

**Why It's Safe:**
1. Shield logic is 3.3V (not 5V like Arduino Uno's AVR chip)
2. ESP32 logic is 3.3V
3. Direct connection is safe
4. **No level shifters required**

**Power Supply Details:**
- Shield expects 3.3V on VCC pin
- ESP32 provides 3.3V (up to 600-800 mA)
- DWM3000 draws ~150 mA peak during TX
- **ESP32 can power it easily**

**Arduino Uno vs ESP32 Voltage:**

| Component | Arduino Uno | ESP32 |
|-----------|-------------|-------|
| MCU Logic | 5V | 3.3V |
| Shield Logic | 3.3V (regulated) | 3.3V (direct) |
| Level Shifter? | No (shield regulates) | No (both 3.3V) |

**CRITICAL WARNING:**
- **Do NOT connect 5V** from anywhere to shield when using ESP32
- Use ESP32's **3.3V pin only**
- Shield's onboard regulator is bypassed when you supply 3.3V

---

## Question 6: Ready-Made Solutions

### Option A: Integrated ESP32 + DWM3000 Boards

Instead of adapting your shield, consider purpose-built boards:

**1. Makerfabs ESP32 UWB DW3000**
- ESP32 + DWM3000 on single board
- No wiring needed
- Proven design
- Cost: ~$25-35 per board
- Search: "Makerfabs ESP32 UWB DW3000"
- Library: https://github.com/Makerfabs/Makerfabs-ESP32-UWB-DW3000

**2. Other ESP32 UWB Modules**
- Various manufacturers make ESP32+DWM3000 boards
- Search: "ESP32 DWM3000 module" or "ESP32 UWB development board"

**Pros:**
- Professionally designed
- No wiring errors
- Better signal integrity
- Often include antenna

**Cons:**
- Additional cost ($25-35 vs $8 ESP32)
- You already have the shields

### Option B: DWM3000 Breakout Boards

**Instead of using Arduino shield format:**
- Buy DWM3000 in breakout board format
- Easier breadboard integration
- Same DWM3000 chip, different form factor

### Option C: DIY Solutions

**1. Breadboard Prototyping (Recommended for testing)**
- Cost: ~$3 breadboard + $2 jumper wires
- Fast setup (10 minutes)
- Easy to modify
- See detailed guide in ESP32_Migration_Guide.md

**2. Perfboard Adapter**
- Solder permanent connections
- More stable than breadboard
- Cost: ~$5-10 in materials

**3. Custom PCB Adapter**
- Design simple adapter PCB
- Services: OSH Park, PCBWay, JLCPCB
- Cost: ~$5-10 for 3 boards (5-10 day shipping)
- Professional appearance

---

## Question 7: Best Practices

### Hardware Best Practices

**1. Power Supply**
- ✓ Use USB power for development
- ✓ Add decoupling capacitors: 10µF + 100nF near DWM3000
- ✓ Monitor voltage: should stay >3.2V under load
- ✗ Don't use weak USB ports (some laptops provide <500mA)

**2. Wiring Quality**
- ✓ Keep SPI wires SHORT (<15 cm ideal, <30 cm max)
- ✓ Use color-coded wires (red=3.3V, black=GND)
- ✓ Double-check connections before power-up
- ✓ Take photos of your wiring for documentation
- ✗ Don't use damaged or oxidized jumper wires

**3. Signal Integrity**
- ✓ Avoid running wires parallel to noisy signals
- ✓ Keep clock wire away from other signals
- ✓ Use twisted pair for long clock signals
- ✓ Keep ground wire same length as signal wires

**4. Mechanical Stability**
- ✓ Secure ESP32 and shield to prevent movement
- ✓ Use breadboard or mount on board
- ✓ Ensure antenna has clearance (>2 cm from metal)
- ✓ Don't touch antenna during operation

**5. Antenna Placement**
- ✓ Keep antenna away from metal objects (>2 cm)
- ✓ Orient antennas for line-of-sight when testing
- ✓ Avoid hand contact during measurements
- ✓ Test in open area first

### Software Best Practices

**1. Use Proven Libraries**
- **Best for ESP32**: https://github.com/Fhilb/DW3000_Arduino
- Proven 5-10 cm accuracy
- Active community support
- Good documentation

**2. Initialization Sequence**
```cpp
// Always do hardware reset first
pinMode(PIN_RST, OUTPUT);
digitalWrite(PIN_RST, LOW);
delay(10);
digitalWrite(PIN_RST, HIGH);
delay(10);

// Then initialize SPI and library
```

**3. Start Simple**
- First: Test SPI communication (read device ID)
- Then: Test basic TX/RX
- Finally: Implement ranging

**4. Debugging**
- Enable serial output (115200 baud)
- Print device ID on startup
- Log all timestamp values
- Monitor for SPI timeouts

**5. Calibration**
- Measure at known distances (1m, 2m, 5m)
- Calculate antenna delay correction
- Apply systematic offset correction
- Expected accuracy: 5-10 cm after calibration

### Development Workflow

**Phase 1: Hardware Setup**
1. Wire first ESP32 (10 minutes)
2. Upload test code
3. Verify SPI communication
4. Read device ID successfully

**Phase 2: Basic Testing**
1. Test interrupt firing
2. Test basic TX mode
3. Test basic RX mode
4. Verify radio operation

**Phase 3: Ranging**
1. Wire second ESP32
2. Implement initiator code
3. Implement responder code
4. Test ranging at 1m

**Phase 4: Calibration**
1. Test multiple distances
2. Calculate errors
3. Apply calibration
4. Achieve 5-10 cm accuracy

---

## Shopping List

### Minimum Required (Manual Wiring)

| Item | Quantity | Est. Cost |
|------|----------|-----------|
| ESP32 DevKit board | 2 | $16-24 |
| Breadboard (830 point) | 1-2 | $3-6 |
| Male-to-Female jumper wires | 20 pack | $3-5 |
| 10µF capacitors | 2 | $1 |
| 100nF capacitors | 2 | $1 |
| USB cables (micro or USB-C) | 2 | $4-8 |
| **Total** | | **~$28-45** |

### Optional But Recommended

| Item | Purpose | Est. Cost |
|------|---------|-----------|
| ESP32 Arduino shield adapter | Cleaner connection | $5-15 |
| Multimeter | Debugging | $10-20 |
| Logic analyzer | Advanced debugging | $10-50 |
| Powered USB hub | Stable power | $10-15 |

### Alternative: Integrated Boards

| Item | Quantity | Est. Cost |
|------|----------|-----------|
| Makerfabs ESP32 UWB DW3000 | 2 | $50-70 |
| USB cables | 2 | $4-8 |
| **Total** | | **~$54-78** |

---

## Quick Start Guide

### For Manual Wiring (30 minutes total)

**1. Gather Materials (5 min)**
- 2x ESP32 DevKit boards
- Breadboard
- 16x jumper wires (8 per device)
- Capacitors (optional but recommended)

**2. Wire First ESP32 (10 min)**
```
Shield Pin  →  ESP32 Pin
─────────────────────────
MOSI (D11)  →  GPIO 23
MISO (D12)  →  GPIO 19
SCK  (D13)  →  GPIO 18
CS   (D10)  →  GPIO 5
IRQ  (D2)   →  GPIO 4
RST  (D9)   →  GPIO 16
3.3V        →  3.3V
GND         →  GND
```

**3. Test Connection (5 min)**
- Connect ESP32 to USB
- Upload test code (see ESP32_Migration_Guide.md Section 9)
- Verify device ID reads correctly
- Expected: 0xDECA0302

**4. Wire Second ESP32 (10 min)**
- Repeat wiring for second device
- Use same pin mapping

**5. Test Ranging**
- Upload initiator code to first ESP32
- Upload responder code to second ESP32
- Open serial monitors
- Should see distance measurements

---

## Expected Results

### With ESP32 (vs Arduino Uno)

| Metric | Arduino Uno | ESP32 |
|--------|-------------|-------|
| CPU Speed | 16 MHz | 240 MHz |
| Processing Power | Limited | 15x faster |
| Timing Precision | Poor (~1-2 µs) | Excellent (~40 ns) |
| Expected Accuracy | ±20-50 cm (if it works) | ±5-10 cm |
| Library Support | Poor (unproven TWR) | Excellent (proven) |
| Community Examples | Few | Many working examples |
| Success Probability | Low (~30%) | High (~95%) |

**Bottom Line:** ESP32 is vastly superior for UWB ranging despite requiring manual wiring.

---

## Troubleshooting Quick Reference

### Can't Read Device ID
- [ ] Check all 8 wire connections
- [ ] Verify 3.3V at DWM3000 (measure with multimeter)
- [ ] Check MOSI/MISO not swapped
- [ ] Try lower SPI speed (1 MHz)

### Interrupt Not Firing
- [ ] Check IRQ wire connection
- [ ] Verify interrupt attached in code
- [ ] Test with LED on IRQ pin

### Module Resets Randomly
- [ ] Add decoupling capacitors (10µF + 100nF)
- [ ] Use powered USB hub
- [ ] Check voltage doesn't drop below 3.2V

### Poor Accuracy
- [ ] Calibrate antenna delay
- [ ] Test in open area (reduce multipath)
- [ ] Verify Double-Sided TWR is used
- [ ] Check for timing delays in code

---

## Additional Resources

**Detailed Guide:**
- See `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/ESP32_Migration_Guide.md` for complete documentation

**Your Project Documentation:**
- Roadmap: `DWS1000_UWB/docs/roadmap.md`
- Hardware Research: `DWS1000_UWB/docs/findings/hardware-research.md`
- Web Research: `DWS1000_UWB/docs/findings/web-research.md`

**Online Resources:**
- Fhilb ESP32 Library: https://github.com/Fhilb/DW3000_Arduino
- Makerfabs Library: https://github.com/Makerfabs/Makerfabs-ESP32-UWB-DW3000
- CircuitDigest Tutorial: https://circuitdigest.com/microcontrollers-projects/diy-indoor-uwb-positioning-system-using-esp32-and-qorvo-dwm3000

---

## Conclusion

**Your Questions Answered:**

1. **Can Arduino shields work with ESP32?** YES - with adapters or manual wiring
2. **Are adapters available?** YES - search "ESP32 Arduino shield adapter"
3. **Manual wiring?** EASY - only 8 connections needed
4. **Which pins?** GPIO 23,19,18,5,4,16 + 3.3V + GND (see table above)
5. **Voltage concerns?** NONE - both 3.3V, perfect match
6. **Ready-made solutions?** YES - Makerfabs integrated boards or adapters
7. **Best practices?** Short wires, decoupling caps, test incrementally

**Recommendation:**
Start with manual breadboard wiring (cheapest, fastest). If you like the setup, either purchase an adapter board or design a custom PCB for permanent installation.

ESP32 will give you MUCH better results than Arduino Uno for UWB ranging!

---

**Document Created:** 2026-01-08
**For Hardware:** Qorvo PCL298336 v1.3 DWM3000EVB Arduino Shield
**Target Platform:** ESP32 DevKit (ESP32-WROOM-32)
