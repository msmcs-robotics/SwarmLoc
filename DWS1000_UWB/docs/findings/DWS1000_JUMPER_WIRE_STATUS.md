# DWS1000 Jumper Configuration Status

> Date: 2026-01-17
> Status: **CRITICAL DISCOVERY - J1 POWER JUMPER**
> Keywords: jumper, D8, D2, IRQ, interrupt, J1, 3V3, power

## Summary

Two different jumper issues on the DWS1000 shields:

1. **D8→D2 Jumper Wire** - Routes IRQ signal (NOW CONNECTED on both)
2. **J1 Power Jumper** - Routes 3.3V power to DWM1000 module (**CRITICAL!**)

---

## CRITICAL: J1 Power Jumper Discovery (2026-01-17)

### What the User Observed

Two DWS1000 shields have **different J1 jumper configurations**:

| Shield | J1 Jumper Status | Issue |
|--------|------------------|-------|
| Shield A | **Has jumper** (J1 → 3V3_ARDUINO) | Correctly powered |
| Shield B | **No jumper** | **MAY NOT BE POWERED!** |

### J1 Pin Group (3 pins near LED)

```
┌─────────────────────────────────┐
│  J1    3V3_ARDUINO   3V3_DCDC   │
│  [●]      [●]          [●]      │
│   └────────┘                    │
│     JUMPER (if present)         │
└─────────────────────────────────┘
```

### Official Documentation (from DWS1000 Product Brief)

> **J1: Switch between onboard 3V3 DC/DC and host provided 3V3 supply.**
> Can be used for measuring current consumption of DWM1000 module.

### What This Means

| J1 Position | Power Source | Use Case |
|-------------|--------------|----------|
| J1 → 3V3_ARDUINO | Arduino's 3.3V regulator | **Default for Arduino Uno** |
| J1 → 3V3_DCDC | Shield's onboard DC-DC | Better for high current |
| **No jumper** | **NO POWER TO DWM1000!** | Current measurement only |

### This Could Explain Our Issues!

If one shield has no J1 jumper:
- SPI may partially work (registers readable via Arduino 5V→3.3V level shift)
- But DWM1000 RF section has no power
- TX would fail silently or transmit garbage
- RX would receive corrupted data or nothing

### Recommended Fix

**For the shield WITHOUT the J1 jumper:**
1. Add a jumper between J1 and 3V3_ARDUINO (middle pin)
2. A small piece of wire bridging these two pins will work
3. This powers the DWM1000 from Arduino's 3.3V rail

---

## D8→D2 Jumper Wire Status

### Current Hardware State (Updated 2026-01-17)

- **Both Arduinos** now have D8→D2 jumper wire connected ✅
- Both devices show Device ID: DECA (SPI working)
- TX sends frames (confirmed by TXFRS status bit)
- RX detects frames (RXFCG set = good CRC)
- **BUT**: Data is corrupted (length shows 1021 instead of 8)

## Why Jumper Wire Might Be Needed

1. **DWS1000 Shield Design**: The Qorvo PCL298336 v1.3 shield may route IRQ to D8
2. **Library Expectation**: arduino-dw1000 library expects IRQ on D2
3. **Forum Advice**: User was advised to connect D8→D2 on both Arduinos

## Why Jumper Wire Might NOT Be Needed

1. **Archive Code**: User's previous working code used `PIN_IRQ = 2` (library default)
2. **DW1000-ng Examples**: All use PIN_RST=9, PIN_IRQ=2 (library defaults)
3. **Some shields may already route to D2**

## Testing Plan

### Test 1: No Jumper Wire (Current State)
- [x] TX sends, RX doesn't receive → **RF not working without jumper**

### Test 2: Jumper on RX Only
- [ ] Add D8→D2 jumper to RX Arduino only
- [ ] Test if RX can now receive

### Test 3: Jumper on TX Only
- [ ] Remove from RX, add to TX Arduino only
- [ ] Test if TX→RX communication works

### Test 4: Jumper on Both
- [ ] Add D8→D2 jumper to both Arduinos
- [ ] Test bidirectional communication

## Hardware Configuration Reference

| Signal | Library Default | Possible Shield Routing | With Jumper |
|--------|-----------------|------------------------|-------------|
| RST | D9 | D7 or D9 | - |
| IRQ | D2 | D8 (needs jumper to D2) | D8→D2 |
| SS/CS | D10 | D10 | - |
| MOSI | D11 | D11 | - |
| MISO | D12 | D12 | - |
| SCK | D13 | D13 | - |

## Physical Jumper Wire Instructions

**To add the D8→D2 jumper:**
1. Use a short jumper wire (male-to-male)
2. Connect Arduino digital pin 8 to digital pin 2
3. The wire bridges the Arduino header pins, not the shield

```
Arduino Uno Header View:

  D0  D1  D2  D3  D4  D5  D6  D7  D8  D9  D10 D11 D12 D13
   o   o  [o]  o   o   o   o   o  [o]  o   o   o   o   o
               ^                   ^
               |___________________|
                    Jumper Wire
```

## Recommendation

**Test with jumper wire on both Arduinos first** since:
1. User was specifically advised to do this
2. Current state (no jumper) shows no RF communication
3. Both TX and RX may need IRQ for proper operation

---

## Test Results (2026-01-17) - CONFIRMS J1 ISSUE

### With D8→D2 jumpers connected, but one shield missing J1 jumper:

| Device | Port | Device ID | Status |
|--------|------|-----------|--------|
| TX | /dev/ttyACM0 | `DECA` ✓ | **Working** - has J1 jumper |
| RX | /dev/ttyACM1 | `EFCA` ✗ | **FAILED** - corrupted ID, missing J1 jumper! |

**The RX shield reads `EFCA` instead of `DECA`** - this is bit corruption typical of a chip not receiving proper power.

### Conclusion

**The shield on ACM1 (RX) that is MISSING the J1 jumper cannot communicate properly with the DWM1000 module because it has no 3.3V power to the UWB chip!**

### USER ACTION REQUIRED

Add a jumper wire between **J1** and **3V3_ARDUINO** (the middle pin) on the second shield (the one currently without a jumper).

A small piece of wire bridging these two pins will work. This provides 3.3V power from the Arduino to the DWM1000 module.

---

_Update this document as tests are completed._
