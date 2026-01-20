# DWS1000_UWB - Todo

> Last updated: 2026-01-19 (Session 4 - TX/RX Debug Session)

## SESSION 4 UPDATE: TX Works, RX Has SPI Corruption

### Summary

| Function | Status | Notes |
|----------|--------|-------|
| TX | ✅ **WORKING 100%** | Both devices transmit reliably with IRQ callbacks |
| RX | ❌ **SPI CORRUPTION** | Fundamental issue during RX mode |
| LDO Tuning | ✅ Applied | Still critical for PLL stability |

### Key Finding: SPI Corruption ONLY in RX Mode

Tested systematically with `test_spi_diagnostic.cpp`:

| Test | Mode | SYS_STATUS Reads | Result |
|------|------|------------------|--------|
| IDLE mode | Chip idle | 100/100 good | ✅ Perfect |
| IDLE mode | Chip idle | 100/100 good | ✅ Perfect |
| TX mode | After TX | 100/100 good | ✅ Perfect |
| **RX mode** | During RX | **~45/100 good** | ❌ **55% corrupt** |

**Conclusion**: SPI communication is only corrupted when the chip is in RX mode. This rules out:
- SPI speed issues (tested with 2MHz slow SPI - same problem)
- IRQ handling issues (tested callback-only approach - same problem)
- Library bugs (tested DW1000Ranging library - same problem)

### Tests Conducted (2026-01-19)

| Test File | Purpose | Result |
|-----------|---------|--------|
| test_spi_diagnostic.cpp | Isolate SPI corruption | Found RX-only corruption |
| test_tx_irq.cpp | TX with IRQ callbacks | ✅ 100% success |
| test_rx_callback_only.cpp | RX using only callbacks | ❌ Corrupted data |
| test_rx_slow_spi.cpp | RX with 2MHz SPI | ❌ Still corrupt |
| test_ranging_anchor.cpp | DW1000Ranging anchor | ❌ Same RX issue |
| test_ranging_tag.cpp | DW1000Ranging tag | ❌ Same RX issue |

### Root Cause Analysis

**Most Likely (80%)**: Hardware power/noise issue during RX
- RX mode draws more current (~110mA vs ~30mA TX)
- Current draw causes voltage dips affecting SPI
- DWM1000 module may have marginal power delivery

**Possible (15%)**: Antenna/RF coupling
- RF energy coupling back into SPI lines during RX
- Poor RF shielding on DWS1000 shields

**Unlikely (5%)**: Defective modules
- Both modules show same issue
- Manufacturing defect possible

## Previous Breakthrough (Still Valid)

**LDO Tuning Fix** - Both devices now PLL-stable after applying OTP LDO tuning:

| Device | LDO Value | RFPLL_LL | Success Rate |
|--------|-----------|----------|--------------|
| DEV0 (ACM0) | 0x88 | **CLEAR** | 98%+ |
| DEV1 (ACM1) | 0x28 | **CLEAR** | 98%+ |

**The fix:** Apply OTP LDO tuning that both libraries had as TODO but never implemented.

See: [LDO_TUNING_FIX_SUCCESS.md](findings/LDO_TUNING_FIX_SUCCESS.md)

## Previous Analysis (Now Resolved)

~~Root cause confirmed: The DW1000's RF PLL cannot maintain lock due to power supply noise.~~

**Actual root cause:** Missing LDO tuning from OTP. The libraries read the values but discarded them.

See: [COMPREHENSIVE_RFPLL_RESEARCH_2026-01-19.md](findings/COMPREHENSIVE_RFPLL_RESEARCH_2026-01-19.md)

## Software Tests to Try (No Capacitors Required)

### Priority 1: LDO Tuning Fix

Both libraries have TODO code that reads OTP LDO values but never applies them:

```cpp
if(ldoTune[0] != 0) {
    // TODO tuning available, copy over to RAM: use OTP_LDO bit
    // *** NEVER IMPLEMENTED IN EITHER LIBRARY ***
}
```

**Test:** `tests/test_ldo_tuning_fix.cpp` - Implements the missing LDO tuning

### Priority 2: Channel Stability Test

Lower frequency channels (1, 2, 4) may have better PLL stability:

| Channel | Frequency | Expected Stability |
|---------|-----------|-------------------|
| 1 | 3.5 GHz | May be better |
| 2 | 4.0 GHz | May be better |
| **5** | 6.5 GHz | Current (problems) |

**Test:** `tests/test_channel_stability.cpp` - Tests all channels for PLL stability

### Priority 3: Alternative FS_PLLTUNE Values

Try `0xA6` instead of `0xBE` for Channel 5 (from web research):

```cpp
// Current (may be suboptimal)
FS_PLLTUNE = 0xBE

// Alternative to try
FS_PLLTUNE = 0xA6
```

### Priority 4: Increased Init Delays

Current init delays are 5ms. Try 10-20ms for better PLL lock time.

### Priority 5: Remove D8→D2 Wire

Test if IRQ is already routed to D2 on the shield (remove wire and test).

## Hardware Options (If Software Fails)

### 1. Add capacitor (if available)

Solder or connect a 10-47µF capacitor between 3V3_DCDC and GND on the shield.

### 2. Try different USB setup

- Use shorter USB cable
- Try powered USB hub
- Try different USB ports (rear ports often cleaner)

### 3. External 3.3V power

Use a separate 3.3V power supply for the DWM1000.

## Current Hardware Configuration

| Item | Shield A (ACM0) | Shield B (ACM1) |
|------|-----------------|-----------------|
| J1 Jumper | **NONE** | **NONE** |
| D8→D2 Wire | Yes | Yes |
| OTP LDO Value | 0x88 | 0x28 |
| CPLOCK | **SET** | **SET** |
| RFPLL_LL | **CLEAR** (with LDO fix) | **CLEAR** (with LDO fix) |
| Stability | **98%+ STABLE** | **98%+ STABLE** |

**Note:** Both devices now work with the LDO tuning fix applied!

## Voltage Measurements (No J1 Jumper)

| Pin | Voltage | Status |
|-----|---------|--------|
| J1 | 3.2V | OK - DWM1000 getting power |
| 3V3_ARDUINO | 2.17V | **BAD** - Arduino regulator failing |
| 3V3_DCDC | 3.3V | OK - Shield DC-DC working |

## Current Status (2026-01-19)

**PLL Issue: RESOLVED** - Both devices show PLL: LOCKED after re-applying LDO post-config

**SPI Corruption Issue: IDENTIFIED** - Root cause is hardware-related, occurring only during RX mode

**Key Discovery:** Must re-apply LDO tuning AFTER `commitConfiguration()` because `tune()` reconfigures PLL

## Next Steps - Hardware Testing Required

### Priority 1: Power Supply Investigation

- [ ] **Test with external 3.3V power supply** - Bypass Arduino/shield power entirely
  - Use quality LDO (AMS1117-3.3 or similar) rated for 500mA+
  - Connect directly to DWM1000's 3.3V and GND pins
  - This will confirm if power noise is the root cause

- [ ] **Add decoupling capacitors** - If available
  - 10-47µF electrolytic between 3V3_DCDC and GND
  - 100nF ceramic as close to DWM1000 as possible
  - Should filter voltage dips during RX current draw

### Priority 2: RF/Antenna Investigation

- [ ] **Check antenna connections** - U.FL connectors can be intermittent
  - Remove and reseat antenna connectors
  - Ensure antennas are properly attached
  - Try swapping antennas between modules

- [ ] **Test with antennas removed** - To check for RF coupling
  - WARNING: Only for brief testing, may damage module
  - If SPI corruption disappears, RF coupling is the cause

### Priority 3: Alternative Hardware

- [ ] **Test with different DWS1000 shields** - If available
  - Confirms whether issue is module-specific or design-related

- [ ] **ESP32 migration** - If hardware fixes don't resolve issue
  - ESP32 has better 3.3V regulation
  - More processing power for timing-critical operations
  - Proven DW1000 library support

## Future Testing Suggestions

### For Continued Arduino Development

1. **Oscilloscope Analysis** (if equipment available)
   - Probe 3V3_DCDC during RX mode
   - Look for voltage dips >25mV
   - Measure SPI signal integrity (MISO, MOSI, SCK)

2. **SPI Shielding Test**
   - Add ferrite beads on SPI lines
   - Twist SPI wires together to reduce pickup
   - Keep SPI wires away from antenna

3. **Reduced Data Rate Test**
   - Try 850kbps instead of 6.8Mbps
   - Longer preamble (4096 symbols)
   - May have better noise immunity

### For ESP32 Migration

1. **Manual wiring from DWS1000 shield**
   ```
   DWS1000 Shield    ESP32
   ─────────────────────────
   MOSI (D11)    →   GPIO 23
   MISO (D12)    →   GPIO 19
   SCK  (D13)    →   GPIO 18
   CS   (D10)    →   GPIO 5
   IRQ  (D8/D2)  →   GPIO 4
   RST  (D9)     →   GPIO 16
   3.3V          →   3.3V
   GND           →   GND
   ```

2. **Use DW1000 library for ESP32**
   - Better timing support
   - More RAM for buffers
   - Active community

## Future Work

- [ ] Complete RX debugging (hardware investigation)
- [ ] Apply LDO fix to DW1000 library permanently
- [ ] Multi-node swarm testing
- [ ] Antenna delay calibration
- [ ] Ranging accuracy verification (±10-20 cm target)
- [ ] ESP32 migration (if Arduino hardware issues persist)

## Library Status

**Using: DW1000 (original)** - switched back for low-level register access (readBytes, writeBytes, readBytesOTP)

| Feature | DW1000 (original) | DW1000-ng |
|---------|-------------------|-----------|
| CPLL lock detect | Missing | ✓ Enabled |
| XTAL trim from OTP | Missing | ✓ Applied |
| Slow SPI during init | No | ✓ Yes |
| Stability | Poor | Better |

See: [DW1000_LIBRARY_REVIEW.md](findings/DW1000_LIBRARY_REVIEW.md)

## Recently Completed

- [x] **SPI diagnostic test** — Isolated corruption to RX mode only — 2026-01-19
- [x] **TX with IRQ callbacks** — 100% success rate on both devices — 2026-01-19
- [x] **RX callback-only test** — Confirmed callback approach doesn't fix issue — 2026-01-19
- [x] **Slow SPI test (2MHz)** — Confirmed SPI speed is not the cause — 2026-01-19
- [x] **DW1000Ranging library test** — Same RX issue with battle-tested library — 2026-01-19
- [x] **Created serial capture script** — Python script for reliable serial monitoring — 2026-01-19
- [x] **LDO TUNING FIX TESTED - BOTH DEVICES NOW 98%+ STABLE!** — 2026-01-19
- [x] **Comprehensive library code review** — Found LDO tuning bug in both libraries — 2026-01-19
- [x] **Created test_ldo_tuning_fix.cpp** — Implements missing OTP LDO application — 2026-01-19
- [x] **DW1000-ng TX/RX test** — RF works, 63+ frames detected, data corrupted (before LDO fix) — 2026-01-17
- [x] Voltage measurements: DC-DC=3.3V, Arduino=2.17V, J1=3.2V — 2026-01-17
- [x] Discovered: No J1 jumper = DC-DC powers DWM1000 directly — 2026-01-17
- [x] Both devices show DECA device ID — 2026-01-17

---

## Test Files

**NEW - TX/RX Debug Tests (2026-01-19):**

- `tests/test_spi_diagnostic.cpp` - **KEY TEST: Isolates SPI corruption to RX mode**
- `tests/test_tx_irq.cpp` - TX with IRQ callbacks (100% working)
- `tests/test_rx_callback_only.cpp` - RX using only library callbacks
- `tests/test_rx_slow_spi.cpp` - RX with explicit 2MHz SPI
- `tests/test_ranging_anchor.cpp` - DW1000Ranging library anchor test
- `tests/test_ranging_tag.cpp` - DW1000Ranging library tag test

**Software Mitigation Tests (2026-01-19):**

- `tests/test_ldo_tuning_fix.cpp` - **Implements missing LDO tuning from OTP**
- `tests/test_channel_stability.cpp` - Tests all channels for PLL stability

**DW1000-ng tests:**

- `tests/test_dw1000ng_simple.cpp` - Basic DW1000-ng stability test
- `tests/test_dw1000ng_tx.cpp` - TX test with DW1000-ng
- `tests/test_dw1000ng_rx.cpp` - RX test with DW1000-ng

**Legacy tests (original DW1000 library):**

- `tests/test_cplock.cpp` - CPLOCK/RFPLL diagnostic
- `tests/polling_tx.cpp` - TX test with polling
- `tests/polling_rx.cpp` - RX test with polling

**Utility Scripts:**

- `scripts/capture_serial.py` - Reliable serial capture with DTR reset and timeout
- `docs/serial_commands.md` - Serial monitoring command reference

## Research Sources

- [TX_RX_DEBUG_SESSION_2026-01-19.md](findings/TX_RX_DEBUG_SESSION_2026-01-19.md) - **NEW: Comprehensive TX/RX debug session**
- [COMPREHENSIVE_RFPLL_RESEARCH_2026-01-19.md](findings/COMPREHENSIVE_RFPLL_RESEARCH_2026-01-19.md) - Deep library analysis
- [LDO_TUNING_FIX_SUCCESS.md](findings/LDO_TUNING_FIX_SUCCESS.md) - LDO tuning breakthrough
- [Qorvo Forum: CLKPLL_LL issue](https://forum.qorvo.com/t/dw1000-cant-receive-at-all-due-to-clkpll-ll-being-set-constantly/13862)
- [GitHub: arduino-dw1000 Issue #42](https://github.com/thotro/arduino-dw1000/issues/42)
- [DW1000 User Manual v2.17](https://www.sunnywale.com/uploadfile/2021/1230/DW1000%20User%20Manual_Awin.pdf)

---

_Update every session: start by reading, end by updating._
