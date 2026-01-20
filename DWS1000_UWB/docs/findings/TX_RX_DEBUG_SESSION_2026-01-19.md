# TX/RX Debug Session - 2026-01-19

## Summary
This document captures the debugging process for TX/RX communication after the LDO tuning fix was applied.

## Background
- LDO tuning fix from OTP was discovered and implemented
- Both devices showed 98%+ PLL stability after the fix
- Next step was to verify TX/RX data communication works

## Issues Encountered

### Issue 1: RX Receiving Massive Corrupted Data
**Symptoms:**
- RX constantly receiving data with `len=0`, `len=1023`, or random lengths
- Status register showing `0xFFFFFFFF` (all bits set)
- "[RX error]" flooding the serial output
- Data like `D6 EF 56 EF 03 00 00 EF...` - clearly garbage

**Analysis:**
- With frame check (CRC) enabled, receiver was still triggering constantly
- RX was picking up RF noise or interference
- The corrupted status registers (`0xFFFFFFFF`) indicated SPI issues or chip errors

### Issue 2: TX Showing "PLL ERR" and Skipping Transmissions
**Symptoms:**
- TX reporting "PLL ERR" on every transmission attempt
- Actually, the PLL was locked but the check function was wrong

**Root Cause:**
- `checkPLL()` was checking "PLL lost lock" sticky bits (RFPLL_LL, CLKPLL_LL)
- These bits were set from previous operations but not cleared
- The fix: Clear sticky bits first, then just check CPLOCK (current lock status)

### Issue 3: TX "TIMEOUT" - Transmit Never Completing
**Symptoms:**
- TX showed `S:800012` then `S:0` then "TIMEOUT"
- First TX had more status bits, subsequent ones had less
- Status going to 0 before TXFRS (TX Frame Sent) bit was seen

**Analysis:**
- Status `0x10` = TXFRB (TX Frame Begins) - TX was starting
- But TXFRS (bit 7) was never seen
- Status would go to 0 mysteriously

**Root Cause Found:**
- The DW1000 library attaches an interrupt handler to the IRQ pin
- In `handleInterrupt()`, it calls `clearAllStatus()` which writes 0xFF to SYS_STATUS
- This clears ALL status bits including our TX complete bit!
- We had D8→D2 jumper which routes IRQ to Arduino interrupt pin

**The Fix:**
```cpp
// Disable IRQ during TX to prevent handleInterrupt() from clearing status
noInterrupts();

DW1000.newTransmit();
DW1000.setDefaults();
DW1000.setData((byte*)data, strlen(data));
DW1000.startTransmit();

// Poll for complete
while (!timeout) {
    uint32_t status = readStatus();
    if (status & (1UL << TXFRS_BIT)) {
        sent = true;
        break;
    }
    delayMicroseconds(100);
}

// Re-enable IRQ
interrupts();
DW1000.clearTransmitStatus();
```

**Result:** TX now works 100% (9/9 transmissions successful)

### Issue 4: Power Brownout During TX
**Symptoms:**
- Serial output showing garbage characters after TX
- Device ID sometimes read incorrectly

**Analysis:**
- TX current draw might be causing voltage droop
- Lowered TX power to `0x15151515` (very low)
- Did NOT fully fix the issue, but reducing power consumption helped

## Status Values Observed

| Status Value | Bits Set | Meaning |
|-------------|----------|---------|
| `0x8000F3` | 0,1,4,5,6,7,23 | CPLOCK, TXFRB, TXPRS, TXPHS, TXFRS, HSRBP - TX complete |
| `0xF1` | 0,4,5,6,7 | TXFRB, TXPRS, TXPHS, TXFRS - TX complete (no CPLOCK shown?) |
| `0x800012` | 1,4,23 | CPLOCK, TXFRB, HSRBP - TX just started |
| `0x10` | 4 | TXFRB only - TX beginning |
| `0x0` | none | Status cleared by IRQ handler |

## Key Findings

1. **IRQ Handler Clears Status:** The DW1000 library's interrupt handler automatically clears all status bits. When polling for TX/RX completion, you must either:
   - Disable interrupts during polling
   - Use interrupt callbacks instead of polling
   - Detach the interrupt handler

2. **PLL Sticky Bits:** RFPLL_LL and CLKPLL_LL are sticky error bits. They indicate the PLL lost lock at some point, but don't mean it's currently unlocked. Always clear them before checking current PLL state.

3. **LDO Tuning Location:** The LDO tuning must be applied:
   - After `DW1000.select()` which resets the chip
   - Optionally again after `DW1000.commitConfiguration()` since `tune()` modifies PLL settings

4. **TX Power:** The default TX power settings are quite high. Lowering power can help with current draw issues on USB-powered setups.

## Next Steps
- Apply same IRQ fix to RX code
- Test TX/RX communication together
- Verify data integrity (correct "PING" messages received)
- If successful, proceed to ranging tests

## Update: RX Device SPI Issues

After fixing the TX (which now works 100%), the RX device (ACM1/DEV1) exhibits major SPI communication issues:

**Symptoms:**
- Status register reads return garbage like 0xFFFFFFFF, 0xE0E0E0F0, 0xC0C0C0C0
- Data length returns invalid values (0, 122, 253, 766, 1021)
- Device ID reads correctly (DECA - model: 1, version: 3, revision: 0)
- OTP LDO value reads correctly (0x28)

**Observations:**
1. The TX device (ACM0/DEV0) works perfectly:
   - Status reads cleanly: 0x8000F3 (all TX bits set)
   - 100% TX success rate
   - Different LDO value: 0x88 vs 0x28 for RX

2. The RX device (ACM1/DEV1) has issues:
   - Status register corrupted
   - Only some registers read correctly (DEV_ID, OTP)
   - SYS_STATUS register reads as near-all-1s or repeating patterns

3. Patterns like 0xE0E0E0E0, 0xF0F0F0F8, 0xC0C0C0C0 suggest:
   - Possible clock/data timing issues
   - EMI/noise on SPI lines
   - Defective connection or component

**Possible Causes:**
1. Hardware difference between DEV0 and DEV1
2. Bad solder joint or connection on DEV1's DWS1000 shield
3. Different board revisions or configurations
4. Power supply issues specific to DEV1

**Next Steps:**
1. Swap the two Arduino/shield pairs and see if the problem follows the shield or the Arduino
2. Check physical connections on DEV1's shield
3. Try slowing down SPI clock speed
4. Test with a logic analyzer to see actual SPI signals

## Update 2: SPI Diagnostic Test Results

### Breakthrough Finding

Created `tests/test_spi_diagnostic.cpp` to isolate where SPI corruption occurs. Results from ACM1:

| Test | Description | Result |
|------|-------------|--------|
| Test 1 | DEV_ID register reads | 10/10 OK |
| Test 2 | SYS_STATUS in IDLE | 10/10 OK |
| Test 3 | SYS_STATUS after config | 10/10 OK |
| Test 4 | SYS_STATUS during RX | **11/20 OK (55%)** |
| Test 5 | SYS_STATUS back in IDLE | 10/10 OK |

**Critical Finding: SPI corruption ONLY happens when the DW1000 is in RX mode!**

### Status Values During RX Mode

Observed status patterns during RX:
- `FF FF FF FF FF` - Complete corruption (SPI bus contention?)
- `00 00 00 00 00` - All zeros (receiver in specific state?)
- `03 04 84 02 02` - Valid status (RX enabled, CPLOCK, etc.)
- `00 FF FF FF F8` - Partial corruption

### Analysis

The corruption is NOT caused by:
- Bad wiring (DEV_ID reads fine always)
- SPI timing (works in IDLE, after config, etc.)
- Specific device (occurs on BOTH devices in RX mode)

The corruption IS correlated with:
- RX mode being active
- Polling during active receive operation

### Hypotheses

1. **DW1000 RX activity interferes with SPI bus**
   - When receiving RF energy, internal circuitry might cause SPI glitches
   - The receiver's AGC, ADC, or demodulator could be causing noise

2. **Double buffering issue**
   - The DW1000 has double-buffered RX
   - Reading status during buffer swap might cause glitches

3. **Power supply noise**
   - RX draws significant current
   - Could cause voltage droops affecting SPI levels

4. **Internal state machine conflict**
   - Reading SYS_STATUS while receiver is actively processing could conflict

### Potential Solutions

1. **Filter corrupt reads** - Skip status reads that return 0xFFFFFFFF
2. **Use slower SPI** - The library has `slowSPI` mode (2MHz vs 16MHz)
3. **Add delays** - Wait longer between reads during RX
4. **Use IRQ-based RX** - Don't poll, let IRQ signal when frame received
5. **Enable frame filtering** - Reduce false triggers from RF noise

### Next Steps

1. Try enabling the IRQ handler for RX (don't disable it like we did for TX)
2. Only check status on IRQ, not by polling
3. Enable frame filtering to reject noise
4. Use RXFCG bit (RX Frame Check Good) as the primary success indicator

## Update 3: IRQ-Based RX Testing

### TX Working, RX Not Receiving

Created IRQ-based tests (`tests/test_tx_irq.cpp` and `tests/test_rx_irq.cpp`) to use the library's interrupt handler instead of polling.

**TX Results:** 100% success rate (PING#00001, PING#00002, etc.)
**RX Results:** 0 frames received

### Debug Findings

Created `tests/test_rx_debug.cpp` with manual IRQ handling:

1. **IRQ Pin Starts HIGH** after `startReceive()`:
   ```
   IRQ pin after RX start: 1
   ```

2. **IRQ Flood:** Using Arduino's `attachInterrupt()` with RISING edge caused 1173+ IRQs in first few seconds - mostly spurious

3. **SYS_MASK is correct:** `88 F0 05 00` - RX interrupts enabled

4. **Status During IRQ Flood:**
   - `01 00 04 00 00` - Just IRQS bit
   - `0F FF FF FF F8` - All error bits (corruption)
   - `00 00 00 00 00` - Empty

5. **Using Level Polling:** IRQ pin never goes HIGH, suggesting no frames being received

### Analysis

The TX is transmitting successfully (confirmed by its status bits showing TXFRS), but the RX is not receiving anything. Possible causes:

1. **Antenna Issue** - One antenna might not be working properly
2. **Mode Mismatch** - Despite using same `MODE_SHORTDATA_FAST_LOWPOWER`, something might differ
3. **Distance/Attenuation** - Devices too close or too far
4. **CRC Failure** - TX data format might not match what RX expects

### Test Files Created

- `tests/test_tx_irq.cpp` - TX with IRQ callbacks, sends "PING#XXXXX" every 2s
- `tests/test_rx_irq.cpp` - RX with IRQ callbacks, uses `receivePermanently(true)`
- `tests/test_rx_debug.cpp` - Debug version with manual IRQ handling
- `tests/test_rx_debug2.cpp` - Level polling version
- `tests/test_rx_v3.cpp` - Comprehensive debug with status decoding

### Serial Monitoring Scripts

Created `scripts/capture_serial.py` for reliable serial capture:
- Properly resets device via DTR
- Captures specified number of lines with timeout
- Avoids hanging on serial port

## Update 4: Role Swap Test - CRITICAL FINDING

### Swapped TX/RX Roles Between Devices

- ACM0: Now TX (was RX), LDO=0x88
- ACM1: Now RX (was TX), LDO=0x28

### Results

**TX on ACM0:** 100% success (PING#00001 through PING#00005 all OK)
**RX on ACM1:** Same SPI corruption, false positives, no real frames

### Key Observation

The RX on ACM1 shows WORSE SPI corruption than ACM0 did:

```
0xDEADDE00 - Unusual pattern
0xC0C0C0C0 - Repeating pattern
0xFFFFFFFF - All ones
"Good" frames with len=238, len=766 (impossible values)
```

### Confirmed Problem

**BOTH devices can TX successfully, but NEITHER can RX properly.**

This rules out:
- Hardware defect on one device
- Wiring issue on one device
- Antenna issue on one device

This suggests:
- **Systematic RX issue** affecting all DWS1000 modules
- **Possibly related to RF environment** - both devices in same location
- **Possibly related to mode configuration** - RX mode has inherent SPI issues
- **Possibly need different approach** - don't poll during RX, use IRQ only

### Hypothesis

The DW1000 might have a known issue where SPI reads during active RX can cause corruption. The solution might be:

1. **Never poll status during RX** - only respond to IRQ
2. **Use the library's IRQ handler** - it handles status reading internally
3. **Check if IRQ line is working** - might be hardware issue with IRQ routing
4. **Try different RX configuration** - longer preamble, lower data rate

## Update 5: Long Range Mode and Callback-Only Testing

### Changed to Long Range Mode

Changed both TX and RX to use `MODE_LONGDATA_RANGE_LOWPOWER`:

- 110 kb/s data rate (vs 6800 kb/s)
- 2048 symbol preamble (vs 128)
- More robust, but slower

### Results with Long Range Mode

**TX:** Works with repeated "[IRQ: TX Error]" messages between transmissions

**RX:**

- Error:7 (Clock/PLL issues - sticky bits)
- Failed:1 with `len=1021`
- Good:0

### Critical Finding: RX_FINFO Register Corruption

The `len=1021` (0x3FD) is suspicious - TX payload is only ~12 bytes.

`getDataLength()` reads from RX_FINFO register (0x10) and the value is corrupted.

### Testing Without CRC Check

When `suppressFrameCheck(true)`: Good:0, Failed:0, Error:1 (receiver never triggers)
When `suppressFrameCheck(false)`: Failed:1 (CRC check fails on corrupted data)

### Conclusion So Far

1. TX works perfectly on both devices
2. RX mode has fundamental SPI corruption on both devices
3. The corruption affects ALL register reads during RX mode
4. Even the IRQ handler reads corrupted data from RX_FINFO
5. No real frames are being received - "failed" frames are SPI corruption

## Update 6: DW1000Ranging Library Test

### Tested with Battle-Tested Ranging Library

Created `tests/test_ranging_anchor.cpp` and `tests/test_ranging_tag.cpp`.

Both devices initialize correctly with same settings but neither sees the other.

### LDO Tuning Lost

LDO tuning is lost after DW1000Ranging.startAs*():

- First read: 0x88 or 0x28 (correct)
- After startAs*: 0xFF or 0x0 (invalid)

### Root Cause Analysis

The problem is consistent across all test methods, pointing to a fundamental RX issue:

1. SPI bus corruption during RX
2. Not receiving any real frames
3. Same issue on both devices

### Possible Causes to Investigate

1. Power supply inadequate
2. Missing decoupling capacitors
3. ~~SPI clock too fast during RX~~ (TESTED - still fails with 2MHz)
4. Antenna issues
5. DW1000 silicon errata

## Update 7: Slow SPI Test

### Tested with 2MHz SPI

Created `tests/test_rx_slow_spi.cpp` with explicit 2MHz SPI reads.

### Results

Still getting corrupt data:

- `S=0xD5BBB5F5` - Corrupt status
- `S=0x3900E67F` with `len=0` - RXFCG set but length is 0
- Events:3 Good:0 Bad:3

### Conclusion

**SPI speed is NOT the issue.** The corruption occurs even at 2MHz (slow mode).

This points to either:

1. **Power supply noise** - RX draws ~100mA, might cause voltage droop
2. **Antenna/RF issue** - Not receiving actual frames, just noise
3. **Hardware defect** - Both shields might have the same manufacturing issue
4. **Missing initialization** - Some register not being set correctly for RX

### Next Steps

1. Try external power supply (not USB)
2. Add decoupling capacitors near DW1000 VDD pins
3. Check antenna connections
4. Try loopback test (TX and RX on same device if possible)

## Code References

- Main test file: `src/main.cpp`
- SPI diagnostic test: `tests/test_spi_diagnostic.cpp`
- Serial capture script: `scripts/capture_serial.py`
- Ranging tests: `tests/test_ranging_anchor.cpp`, `tests/test_ranging_tag.cpp`
- LDO tuning function: `applyLDOTuning()` - reads OTP address 0x04, sets OTP_LDO bit
- TX Power register: 0x1E (TX_POWER)
