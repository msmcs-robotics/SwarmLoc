# LDO Tuning Fix - SUCCESSFUL!

> Date: 2026-01-19
> Status: **BREAKTHROUGH** - Both devices now stable
> Keywords: DW1000, LDO tuning, RFPLL_LL, OTP, PLL stability

## Executive Summary

**The missing LDO tuning from OTP was the root cause of the RFPLL_LL issues.**

After implementing the LDO tuning fix that both libraries had as TODO but never implemented, both DWS1000 shields now operate with **98%+ PLL stability** and **RFPLL_LL = 0**.

## The Fix

The fix is simple - just 10 lines of code that were marked as TODO in both libraries:

```cpp
// Read LDO tune from OTP address 0x04
byte ldoTune[4];
DW1000.readBytesOTP(0x04, ldoTune);

if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
    // Apply the LDO tuning - SET OTP_LDO bit in AON_CTRL
    byte aonCtrl[4];
    DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
    aonCtrl[0] |= 0x40;  // Set OTP_LDO bit (bit 6)
    DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
    delay(1);
    aonCtrl[0] &= ~0x40;  // Clear the bit
    DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
}
```

## Test Results

### DEV0 (ACM0) - Previously "unstable, crashes often"

```
OTP LDO Tune: 0x88
RFPLL_LL: CLEAR (was SET before!)
Success rate: 98.0%
Status: STABLE
```

### DEV1 (ACM1) - Previously "more stable"

```
OTP LDO Tune: 0x28
RFPLL_LL: CLEAR (was SET before!)
Success rate: 98.0%
Status: STABLE
```

## Key Observations

1. **Different LDO values per device**: DEV0 has `0x88`, DEV1 has `0x28`
   - This explains why devices behaved differently!
   - Each DWM1000 module is factory-calibrated with its own LDO value

2. **Initial CLKPLL_LL error is normal**: One bad sample at startup, then clears
   - This is expected behavior during PLL lock sequence
   - Not a concern

3. **RFPLL_LL now stays CLEAR**: The main problem is solved!
   - RF PLL maintains lock
   - Data integrity should now be possible

## Why This Wasn't Done Before

Both libraries had this code pattern:

```cpp
// In DW1000.cpp / DW1000Ng.cpp:
if(ldoTune[0] != 0) {
    // TODO tuning available, copy over to RAM: use OTP_LDO bit
}
```

The TODO was never implemented. The libraries read the LDO value but discarded it.

## What LDO Tuning Does

LDO = Low Dropout Regulator

The DW1000 has internal LDO regulators that power the PLL circuits. Each chip has factory-calibrated optimal settings stored in OTP (One-Time Programmable) memory at address 0x04.

By setting the `OTP_LDO` bit in the AON_CTRL register, these calibrated values are transferred from OTP to the active LDO configuration registers, ensuring optimal power supply conditions for the PLLs.

## Impact

| Before LDO Fix | After LDO Fix |
|----------------|---------------|
| RFPLL_LL = SET (always) | RFPLL_LL = CLEAR |
| Data corrupted | Potentially stable |
| DEV0 crashed often | DEV0 stable |
| 0% success | 98% success |

## Next Steps

1. **Run TX/RX test** with LDO tuning enabled to verify data integrity
2. **Apply fix to libraries** permanently
3. **Test ranging** now that PLLs are stable

## Files

- Test code: `src/main.cpp` (LDO tuning test)
- Original test: `tests/test_ldo_tuning_fix.cpp`

## Recommendation

**Apply this fix to the DW1000 and/or DW1000-ng libraries permanently.**

The fix should be added to the initialization sequence right after LDE microcode loading:

```cpp
// In manageLDE() or initialize():
if(ldoTune[0] != 0) {
    // Apply LDO tuning from OTP
    byte aonCtrl[1];
    readBytes(AON, AON_CTRL_SUB, aonCtrl, 1);
    aonCtrl[0] |= 0x40;  // OTP_LDO bit
    writeBytes(AON, AON_CTRL_SUB, aonCtrl, 1);
    delay(1);
    aonCtrl[0] &= ~0x40;
    writeBytes(AON, AON_CTRL_SUB, aonCtrl, 1);
}
```
