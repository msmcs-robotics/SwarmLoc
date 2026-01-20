# Comprehensive RFPLL_LL Research and Software Solutions

> Date: 2026-01-19
> Status: Complete research, pending implementation
> Keywords: DW1000, RFPLL_LL, PLL stability, software fixes, no capacitors

## Executive Summary

Comprehensive research across both libraries (DW1000 and DW1000-ng) and web resources reveals that **RFPLL_LL is fundamentally a hardware issue**, but there are **several unexplored software mitigations** that may help.

**Key Discovery:** Both libraries have a **critical bug** - they read LDO tuning values from OTP but **never apply them**. This could significantly impact PLL stability.

---

## 1. Critical Software Bug Found: LDO Tuning Not Applied

### Location in DW1000 Library
`/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp` (lines 189-191):

```cpp
if(ldoTune[0] != 0) {
    // TODO tuning available, copy over to RAM: use OTP_LDO bit
}
```

### Location in DW1000-ng Library
`/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000-ng/src/DW1000Ng.cpp` (lines 1066-1073):

```cpp
if(ldoTune[0] != 0) {
    // TODO tuning available, copy over to RAM: use OTP_LDO bit
    // *** THIS IS NOT IMPLEMENTED ***
}
```

### The Fix

To apply LDO tuning from OTP, we need to set the `OTP_LDO` bit in the `AON_CTRL` register:

```cpp
if(ldoTune[0] != 0) {
    // Set OTP_LDO bit in AON_CTRL to transfer tuned LDO values
    byte aonCtrl;
    readBytes(AON, AON_CTRL_SUB, &aonCtrl, 1);
    aonCtrl |= 0x40;  // OTP_LDO bit is bit 6
    writeBytes(AON, AON_CTRL_SUB, &aonCtrl, 1);
    delay(1);
    aonCtrl &= ~0x40; // Clear the bit
    writeBytes(AON, AON_CTRL_SUB, &aonCtrl, 1);
}
```

**Impact:** LDO tuning directly affects the internal low-dropout regulators that power the PLL circuitry. Without proper LDO tuning, the PLLs may not have optimal power supply conditions.

---

## 2. FS_PLLTUNE Value May Be Wrong

### Current Configuration for Channel 5

The libraries use `FS_PLLTUNE = 0xBE` for Channel 5/7.

### Alternative from Web Research

Some sources suggest `FS_PLLTUNE = 0xA6` may be more stable for Channel 5:

| Channel | Current FS_PLLTUNE | Alternative |
|---------|-------------------|-------------|
| 1 | 0x1E | 0x1E |
| 2 | 0x26 | 0x26 |
| 3 | 0x56 | 0x56 |
| 4 | 0x26 | 0x26 |
| 5 | 0xBE | **0xA6** |
| 7 | 0xBE | 0xA6 |

**Test Recommendation:** Try setting `FS_PLLTUNE = 0xA6` for Channel 5.

---

## 3. Initialization Sequence Issues

### Problem: SPI Speed Too Fast During Init

Both libraries use 2 MHz during init (slow mode), but the DW1000 datasheet says SPI must be **< 3 MHz during INIT state** (first 5µs after reset). The issue is the **exact timing** of when to switch speeds.

### The Dangerous Timing Window

```
Power-on / Reset
      │
      ▼
   ┌──────────────────┐
   │  INIT State      │ ◄── SPI must be slow AND quiet during transition
   │  (0-5µs)         │
   └───────┬──────────┘
           │ PLL locks
           ▼
   ┌──────────────────┐
   │  IDLE State      │ ◄── Safe to use fast SPI
   └──────────────────┘
```

**Critical:** Do NOT have active SPI transactions at the ~5µs mark when the clock switches from XTI to PLL.

### Recommended Fix

Add an explicit delay after reset before ANY SPI access:

```cpp
void safeReset() {
    // Hard reset
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(2);
    pinMode(PIN_RST, INPUT);  // Hi-Z, internal pull-up brings it high

    // Wait for INIT state to complete and PLL to lock
    delay(10);  // Increased from 5ms to 10ms

    // Now safe to access SPI at slow speed
}
```

---

## 4. PLL Recovery Sequence

When RFPLL_LL is detected, the following recovery sequence may help:

```cpp
void recoverFromPLLError() {
    // 1. Disable sequencing - go to INIT state
    byte pmscctrl0[4];
    readBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, 4);
    pmscctrl0[0] = 0x01;  // XTI_CLOCK
    writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, 2);
    delay(2);

    // 2. Clear PLL error flags
    byte clearStatus[4] = {0, 0, 0, 0x03};  // Clear RFPLL_LL and CLKPLL_LL
    writeBytes(SYS_STATUS, 0x00, clearStatus, 4);

    // 3. Re-apply frequency synthesizer tuning
    _fspll();      // FS_PLLCFG and FS_PLLTUNE
    _fsxtalt();    // Crystal trim

    // 4. Re-enable auto clock
    pmscctrl0[0] = 0x00;  // AUTO_CLOCK
    writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, 2);
    delay(5);

    // 5. Check if recovered
    byte status[4];
    readBytes(SYS_STATUS, 0x00, status, 4);
    if (status[3] & 0x03) {
        // Still failing - hardware issue
        Serial.println("PLL recovery failed - hardware issue");
    }
}
```

---

## 5. Channel Configuration Comparison

Different channels may have different PLL stability characteristics:

| Channel | Freq (GHz) | Bandwidth | PLL Complexity | Stability |
|---------|------------|-----------|----------------|-----------|
| 1 | 3.5 | 500 MHz | Lower | Good |
| 2 | 4.0 | 500 MHz | Lower | Good |
| 3 | 6.5 | 500 MHz | Medium | Medium |
| 4 | 4.0 | 900 MHz | Lower | Good |
| **5** | 6.5 | 500 MHz | Higher | **Default** |
| 7 | 6.5 | 1.1 GHz | Highest | Medium |

**Recommendation:** Try Channel 2 or 4 (lower frequency, simpler PLL) instead of Channel 5.

---

## 6. Data Rate Impact on PLL

Lower data rates may allow more time for PLL to stabilize between symbols:

| Data Rate | Symbol Time | PLL Margin |
|-----------|-------------|------------|
| 110 kbps | ~9µs | Maximum |
| 850 kbps | ~1.2µs | Medium |
| 6.8 Mbps | ~150ns | Minimum |

**Current:** Using 110 kbps (good choice for stability)

---

## 7. PRF (Pulse Repetition Frequency) Impact

| PRF | Stability | Power |
|-----|-----------|-------|
| 16 MHz | Better | Lower |
| 64 MHz | Worse | Higher |

**Current:** Using 16 MHz (good choice)

---

## 8. Jumper Wire Alternatives

### Remove D8→D2 Wire Test

The IRQ may already be routed to D2 on some shield revisions. Test without the jumper wire.

### Try D8→D3 Instead

D3 is INT1 on Arduino Uno. Change code to use INT1:

```cpp
const uint8_t PIN_IRQ = 3;  // Use D3 instead of D2
```

### J1 Power Configuration

| J1 Position | Power Source | Notes |
|-------------|--------------|-------|
| Open (current) | DC-DC converter | May have noise |
| To 3V3_ARDUINO | Arduino regulator | Only 50mA, not enough |
| External wire | External 3.3V | Best if available |

---

## 9. Software Tests to Run

### Test 1: LDO Tuning Implementation

Create a test that:
1. Reads OTP address 0x04 for LDO tune values
2. Applies OTP_LDO bit if values exist
3. Monitors PLL stability

### Test 2: Increased Init Delays

Modify init sequence:
- 10ms after reset (instead of 5ms)
- 10ms after XTI_CLOCK enable
- 10ms after AUTO_CLOCK enable

### Test 3: Alternative FS_PLLTUNE

Try `0xA6` instead of `0xBE` for Channel 5.

### Test 4: Channel 2 Test

Switch to Channel 2 with matching preamble code.

### Test 5: Remove IRQ Wire

Test with library defaults, no D8→D2 wire.

### Test 6: Aggressive PLL Recovery

Implement auto-recovery on RFPLL_LL detection.

---

## 10. Implementation Priority

| Priority | Fix | Effort | Impact |
|----------|-----|--------|--------|
| 1 | **LDO tuning bug** | Medium | High |
| 2 | Increased init delays | Low | Medium |
| 3 | PLL recovery handler | Medium | Medium |
| 4 | Alternative FS_PLLTUNE | Low | Low-Medium |
| 5 | Channel 2 test | Low | Unknown |
| 6 | Remove IRQ wire | Low | Unknown |

---

## 11. Testing Progress (Updated)

### LDO Tuning Fix - SUCCESS
The LDO tuning fix has been implemented and tested:
- **DEV0 (ACM0)**: LDO value = 0x88, PLL now LOCKED
- **DEV1 (ACM1)**: LDO value = 0x28, PLL now LOCKED

**Key Finding**: LDO tuning must be re-applied AFTER `commitConfiguration()` because `tune()` reconfigures the PLL registers.

### TX/RX Testing - IN PROGRESS
Current status:
- TX successfully transmits frames (TXFRS status bit set)
- RX receives frames (RXFCG status bit set with CRC enabled)
- **BUT**: Received data does not match transmitted data

Observation from RX:
- Transmitted: "T00001" (hex: 54 30 30 30 30 31)
- Received: 15 C3 A1 EE E1 27 7D 38... (completely different)

The received pattern `15 C3 A1` is consistently appearing, suggesting:
1. RF link is working (frames are detected)
2. Data encoding/decoding has a mismatch

### Polling Mode Bug Fix - SUCCESS
Discovered that `DW1000.isReceiveDone()` and `DW1000.isTransmitDone()` check cached `_sysstatus` that's only updated during interrupt handling. In polling mode, must read `SYS_STATUS` register directly before checking flags.

---

## 12. Conclusion

The most promising software-only fixes are:

1. **Implement missing LDO tuning** - Both libraries have this TODO that was never done
2. **Increase initialization delays** - More time for PLL to stabilize
3. **Add automatic PLL recovery** - Detect RFPLL_LL and re-tune
4. **Try different FS_PLLTUNE values** - 0xA6 vs 0xBE for Channel 5

If none of these work, the issue is likely:
- Hardware damage to the DWM1000 module
- Fundamental power supply noise that requires capacitors
- Manufacturing defect

---

## Sources

- DW1000-ng library code analysis
- DW1000 library code analysis
- Qorvo Tech Forum threads
- DW1000 User Manual v2.17
- GitHub arduino-dw1000 issues
- Decawave Application Notes
