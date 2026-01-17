# DW1000 CPLOCK Not Locking - Root Cause Analysis

> Date: 2026-01-17
> Status: **CRITICAL HARDWARE ISSUE IDENTIFIED**
> Keywords: CPLOCK, CLKPLL_LL, PLL, clock, power, 3.3V

## Executive Summary

**Both DWS1000 shields exhibit the same critical issue**: The DW1000 Clock PLL (CPLOCK) never locks after initialization. Without CPLOCK, the RF section cannot operate, making TX/RX communication impossible.

## Symptoms Observed

| Test | Result |
|------|--------|
| Device ID read | `DECA` ✓ (SPI works) |
| SYS_STATUS CPLOCK bit | **Never set** |
| SYS_STATE | Stuck in `INIT` (0x00) |
| TX operation | TXFRS set, but no RF output |
| RX operation | Status always 0x0 |

## Test Results (ACM0 and ACM1 identical)

```
SYS_STATUS = 0x0200000000
  [CPLOCK] Clock PLL NOT locked - BAD!
SYS_STATE = 0x00010000 (PMSC=INIT)

After enabling receiver:
SYS_STATUS = 0x0200000000 (still no CPLOCK)
SYS_STATE = 0x40050500 (PMSC=INIT)

Diagnosis: CPLOCK NOT SET - DW1000 clock PLL not locking!
```

## Root Cause Analysis

Based on web research from [Qorvo Forum](https://forum.qorvo.com/t/dw1000-cant-receive-at-all-due-to-clkpll-ll-being-set-constantly/13862) and [GitHub Issue #42](https://github.com/thotro/arduino-dw1000/issues/42):

### What DW1000 User Manual Says

> "Clock PLL Losing Lock. This event status bit is set to indicate that the system's digital clock PLL is having locking issues. **This should not happen in healthy devices operating in their normal range**. Its occurrence may indicate a bad configuration, a faulty part or a problem in the power or clock inputs to the device."

### Most Likely Causes (in order of probability)

1. **Power Supply Noise/Instability**
   - DW1000 is VERY sensitive to power noise (needs <25mV ripple)
   - Arduino Uno's 3.3V regulator may not be stable enough
   - User measured VDD variations up to 250mV causing this issue

2. **Inadequate Power Filtering**
   - The DWS1000 shield may not have sufficient decoupling capacitors
   - Need 10µF+ capacitor close to DW1000 power pins

3. **J1 Jumper Power Path**
   - Even with J1 jumper connected, power quality may be poor
   - Arduino's 3.3V rail has limited current capacity

4. **Crystal/Clock Issues**
   - The 38.4 MHz crystal on DWM1000 may have problems
   - Could be damaged or have cold solder joints

## Hardware Checks to Perform

### Immediate Checks

1. **Measure 3.3V voltage at DW1000**
   - Should be stable 3.3V ± 50mV
   - Use oscilloscope if possible to check for noise

2. **Check J1 jumper position**
   - Currently: J1 → 3V3_ARDUINO (using Arduino's 3.3V)
   - Alternative: J1 → 3V3_DCDC (using shield's DC-DC converter)
   - **Try switching to 3V3_DCDC position** - this uses the shield's onboard regulator which may be cleaner

3. **Visual inspection**
   - Check crystal (38.4 MHz near DWM1000) for damage
   - Check solder joints on DWM1000 module

### Power Improvements to Try

1. **Add external capacitor**
   - Solder 10-47µF ceramic capacitor between 3.3V and GND
   - Close to the DWM1000 module

2. **Use external 3.3V power supply**
   - Bypass Arduino's 3.3V regulator entirely
   - Use quality LDO like AMS1117-3.3 or similar

3. **Try J1 → 3V3_DCDC position**
   - This uses the shield's onboard DC-DC converter
   - May provide cleaner power than Arduino's regulator

## Software Workaround (Unlikely to Help)

The issue is hardware-related, but we can try:

1. Add delay after reset
2. Retry initialization multiple times
3. Check CPLOCK before proceeding

```cpp
// Wait for CPLOCK with retry
bool waitForCPLOCK(int maxRetries = 10) {
    for (int retry = 0; retry < maxRetries; retry++) {
        // Reset the chip
        DW1000.reset();
        delay(100);

        // Check CPLOCK
        byte status[1];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 1);
        if (status[0] & 0x02) {
            return true;  // CPLOCK set!
        }

        Serial.print("CPLOCK retry ");
        Serial.println(retry);
        delay(100);
    }
    return false;
}
```

## Next Steps

1. **User Action**: Try switching J1 jumper from 3V3_ARDUINO to 3V3_DCDC
2. **User Action**: Measure 3.3V voltage with multimeter
3. **User Action**: Add 10µF capacitor across 3.3V if possible
4. If above fails: Consider external 3.3V power supply

## References

- [Qorvo Forum: CLKPLL_LL constantly set](https://forum.qorvo.com/t/dw1000-cant-receive-at-all-due-to-clkpll-ll-being-set-constantly/13862)
- [GitHub: arduino-dw1000 Issue #42](https://github.com/thotro/arduino-dw1000/issues/42)
- [DW1000 User Manual](https://fcc.report/FCC-ID/2AAXVTNTMOD1/2787937.pdf)

---

## Voltage Measurements (No J1 Jumper Connected)

User measured voltages on the 3-pin header with NO jumper installed:

| Pin | Voltage | Analysis |
|-----|---------|----------|
| J1 | 3.2V | DWM1000 power input - getting power from DC-DC already! |
| 3V3_ARDUINO | 2.17V | **Too low!** Should be 3.3V - Arduino regulator problem |
| 3V3_DCDC | 3.3V | Shield's DC-DC converter - working correctly |

**Key Finding**: The DWM1000 is powered by the shield's DC-DC converter by default (3.2V at J1 with no jumper). The J1 jumper may be for selecting/overriding the power source, not enabling it.

**Arduino 3.3V Issue**: The 2.17V reading suggests Arduino's 3.3V regulator is either overloaded, faulty, or being pulled down.

## Test Results: No J1 Jumper (2026-01-17)

With NO J1 jumper installed (DWM1000 powered solely by shield's DC-DC converter):

```
[DEV0] SYS_STATUS = 0x0002800002
[DEV0] [CPLOCK] Clock PLL LOCKED - GOOD!
[DEV0] [RFPLL_LL] RF PLL LOSING LOCK - BAD!
```

**Findings:**
- **CPLOCK now locks initially!** (set after 1ms)
- **BUT RFPLL_LL is set** - RF PLL is losing lock (unstable)
- After a few seconds, CPLOCK drops to 0
- SYS_STATE becomes erratic (0xFF000000, 0xFFF0F8F8) indicating noise

**Interpretation:**
- Removing the J1 jumper (using only DC-DC power) improves clock stability
- But there's still power noise causing RF PLL instability
- The erratic register reads suggest SPI noise from power fluctuations

## TX/RX Test Results: No J1 Jumper (2026-01-17)

**RF COMMUNICATION IS HAPPENING!** But unstable:

```text
[TX] [TX #2] SENT | Waiting for PONG... GOT len=0 hex= str=""
[TX] [TX #4] SENT | Waiting for PONG... GOT len=76 hex=FF F8 00 00...
[TX] [TX #5] SENT | Waiting for PONG... GOT len=126 hex=C0 C0 C0...
[RX] Device ID: FFFF - model: 255... [FAIL] DW1000 not detected!
[TX] Device ID: 00 - model: 0... [FAIL] DW1000 not detected!
```

**What's happening:**
- TX sends frames successfully (TXFRS triggers)
- RX DOES receive something (RXFCG triggers)
- But data is corrupted (garbage bytes)
- Both chips eventually crash (Device ID becomes FFFF or 00)
- Status shows `0x2000000` = RFPLL_LL (RF PLL losing lock)

**Root cause confirmed:** Power instability causing RF PLL to lose lock, which corrupts data and eventually crashes the chips.

## Recommended Hardware Fixes

### Immediate (try first)
1. **Add 10-47µF capacitor** between 3V3_DCDC and GND on shield
2. **Use shorter USB cable** or powered USB hub
3. **Try different USB port** (front panel ports often have more noise)

### If above fails
1. **External 3.3V power supply** - bypass Arduino entirely
2. **Add ferrite bead** on 3.3V line
3. **Check if Arduino Uno's 5V→3.3V regulator is faulty**

### Long-term
1. **Consider ESP32** - has better 3.3V supply, native support
2. **Add dedicated LDO** (AMS1117-3.3) for DWM1000 power

## DW1000-ng Library Test Results (2026-01-17)

Switched to DW1000-ng library which has better PLL initialization:

| Device | CPLOCK | RFPLL_LL | Stability | Notes |
|--------|--------|----------|-----------|-------|
| DEV1 (ACM1) | **STAYS SET** | Set | **Stable** | Runs for 30+ seconds |
| DEV0 (ACM0) | Drops | Set | **Crashes** | ID becomes 00/FFFF |

**Key finding:**
- DW1000-ng significantly improves stability on DEV1
- DEV0 still crashes - **possible hardware defect on that shield**
- RFPLL_LL still set on both (RF PLL instability persists)

**Interpretation:**
- The shields may have different hardware quality/tolerances
- Software improvements help one device but not the other
- DEV0's shield may have marginal/defective power regulation

## DW1000-ng TX/RX Communication Test (2026-01-17)

With DW1000-ng TX/RX test:

- **TX (ACM0)**: Sends successfully ("SENT" confirmed)
- **RX (ACM1)**: Detects 63+ frames in 25 seconds
- **BUT**: All received data is corrupted garbage

Example received data (expected "PING1234"):
```
len=57 hex=E5 BB C6 C0 00 00 00 00 00 00 00 00 00 FF E0 00
len=1023 (wrong length)
len=0 (no data)
```

**Conclusion:** RF is transmitting and receiving, but RFPLL_LL instability causes massive bit errors. **This is a hardware power issue that software cannot fully resolve.**

## Final Assessment

| Aspect | Status | Notes |
|--------|--------|-------|
| SPI Communication | Working | Device ID reads correctly |
| CPLOCK (Clock PLL) | Partial | Locks with DW1000-ng, no J1 |
| RFPLL (RF PLL) | **FAILING** | RFPLL_LL always set |
| RF Transmission | Working | TXFRS confirms sent |
| RF Reception | Working | Frames detected |
| Data Integrity | **BROKEN** | All data corrupted |

**Root cause:** Power supply noise causing RF PLL instability. DW1000 requires <25mV ripple. Without capacitors or cleaner power, this cannot be fixed in software.

## Update Log

- 2026-01-17: TX/RX test - RF works but ALL data corrupted due to RFPLL_LL
- 2026-01-17: DW1000-ng test - DEV1 stable, DEV0 crashes (hardware issue?)
- 2026-01-17: No J1 jumper - CPLOCK locks but RFPLL_LL shows instability
- 2026-01-17: Voltage measurements show DC-DC powers DWM1000 by default
- 2026-01-17: Initial discovery - both devices stuck with CPLOCK not set
