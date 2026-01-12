# DWS1000 Shield Pinout and Critical Fix

**Date:** 2026-01-11
**Part Number:** PCL298336 (DWS1000 Arduino Shield with DWM1000)

## The Problem - Why Ranging Isn't Working

After extensive research, the issue is a **pin configuration mismatch** between:
1. The **DWS1000 shield's physical wiring**
2. The **arduino-dw1000 library defaults**

### Pin Comparison

| Signal | Your Code | Library Default | DWS1000 Shield Actual |
|--------|-----------|-----------------|----------------------|
| RST    | Pin 9     | Pin 9           | **Pin 7**            |
| IRQ    | Pin 2     | Pin 2           | **Pin 8** (PROBLEM!) |
| SS/CS  | Pin 10    | Pin 10          | Pin 10               |
| MOSI   | Pin 11    | Pin 11          | Pin 11               |
| MISO   | Pin 12    | Pin 12          | Pin 12               |
| SCK    | Pin 13    | Pin 13          | Pin 13               |

### Critical Issue: IRQ on Pin 8

**Arduino Uno only supports hardware interrupts on pins 2 and 3.**

The DWS1000 shield routes the IRQ signal to **pin 8**, which is NOT interrupt-capable on Arduino Uno. The DW1000 library uses `attachInterrupt(digitalPinToInterrupt(irq), ...)` which will silently fail or cause undefined behavior with pin 8.

This is why:
- Devices initialize correctly (SPI works)
- No ranging occurs (interrupt never triggers)
- No devices are found (can't receive messages)

---

## Solution: Add a Jumper Wire

You need to connect **pin 8 to pin 2** (or pin 3) on your Arduino Uno/shield.

### Option A: Simple Jumper Wire (Recommended)

1. Use a short jumper wire
2. Connect **D8** to **D2** on the Arduino headers
3. This routes the IRQ signal to an interrupt-capable pin
4. Update code to use PIN_IRQ = 2

### Option B: Hardware Modification

1. Cut the trace from DWM1000 IRQ to D8 (if accessible)
2. Solder a wire from the IRQ pad to D2
3. More permanent but requires soldering

---

## Updated Code Configuration

### For Current DWS1000 Shield (with jumper wire fix)

```cpp
// DWS1000 Shield Correct Pin Configuration
// AFTER adding jumper wire from D8 to D2

const uint8_t PIN_RST = 7;   // DWS1000 uses pin 7 for reset
const uint8_t PIN_IRQ = 2;   // IRQ jumpered from pin 8 to pin 2
const uint8_t PIN_SS = 10;   // SS/CS is correct at pin 10

// In setup():
DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
```

### Alternative: If NOT adding jumper wire

You would need to modify the library to use Pin Change Interrupts (PCINT), which is more complex and not recommended.

---

## Complete DWS1000 Shield Pinout

Based on research from Qorvo forums and documentation:

### Arduino Header to DWM1000 Module

| Arduino Pin | DWM1000 Pin | Signal | Notes |
|-------------|-------------|--------|-------|
| D2          | - | (Available for IRQ jumper) | Interrupt-capable |
| D3          | - | (Available for IRQ jumper) | Interrupt-capable |
| D7          | Pin 3 (RSTn) | Reset | Active low, should be left floating or driven low |
| D8          | Pin 22 (IRQ) | Interrupt Request | Needs jumper to D2 or D3 |
| D10         | Pin 17 (SPICSn) | SPI Chip Select | Active low |
| D11         | Pin 18 (SPIMOSI) | SPI Data In | |
| D12         | Pin 19 (SPIMISO) | SPI Data Out | |
| D13         | Pin 20 (SPICLK) | SPI Clock | |
| 5V          | - | Power Input | Via onboard 3.3V regulator |
| GND         | Pins 8,16,21,23,24 | Ground | |
| 3.3V        | Pins 6,7 (VDD3V3) | 3.3V Power | Shield has DC-DC converter |

### DWM1000 Module 24-Pin Diagram

```
       ┌─────────────────┐
    1  │ EXTON           │ 24  VSS
    2  │ WAKEUP          │ 23  VSS
    3  │ RSTn ────────── │ ──→ D7
    4  │ SYNC/GPIO7      │ 22  IRQ/GPIO8 ──→ D8 (needs jumper to D2)
    5  │ VDDAON          │ 21  VSS
    6  │ VDD3V3          │ 20  SPICLK ──→ D13
    7  │ VDD3V3          │ 19  SPIMISO ──→ D12
    8  │ VSS             │ 18  SPIMOSI ──→ D11
    9  │ GPIO6/EXTRXE    │ 17  SPICSn ──→ D10
   10  │ GPIO5/EXTTXE    │ 16  VSS
   11  │ GPIO4/EXTPA     │ 15  GPIO0/RXOKLED
   12  │ GPIO3/TXLED     │ 14  GPIO1/SFDLED
   13  │ GPIO2/RXLED     │
       └─────────────────┘
```

---

## Verification Steps

### Step 1: Add Jumper Wire
Connect D8 to D2 with a jumper wire on the Arduino headers.

### Step 2: Update Firmware with Correct Pins
Update main.cpp with:
```cpp
const uint8_t PIN_RST = 7;   // Changed from 9
const uint8_t PIN_IRQ = 2;   // Was already 2 (receives from D8 via jumper)
const uint8_t PIN_SS = 10;   // Unchanged
```

### Step 3: Run Connectivity Test
First verify SPI is working before testing ranging.

### Step 4: Test Ranging
- Start ANCHOR first
- Start TAG second
- Both should now detect each other

---

## Sources

- [Qorvo Forum: Arduino Uno with DWS1000 module](https://forum.qorvo.com/t/arduino-uno-with-dws1000-module/10223)
- [Qorvo Forum: Arduino Uno with DWS1000](https://forum.qorvo.com/t/arduino-uno-with-dws1000/10212)
- [GitHub: arduino-dw1000 library](https://github.com/thotro/arduino-dw1000)
- [Qorvo: DWS1000 Product Brief](https://www.mouser.com/datasheet/2/412/DWS1000_Product_Brief_V1_0-1878682.pdf)
- [Arduino: attachInterrupt documentation](https://docs.arduino.cc/language-reference/en/functions/external-interrupts/attachInterrupt/)
