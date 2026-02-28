# DWS1000 UWB Bugs & Issues Tracker

## Active Bugs

### BUG-001: PIN_RST = 9 Instead of 7 (FIXED in v8)
- **Severity:** Critical
- **Status:** Fixed
- **Date Found:** 2026-02-27
- **Root Cause:** All test code used `PIN_RST = 9` (library default for generic breakout boards), but the DWS1000 shield routes DW1000's RST to Arduino pin D7.
- **Impact:** The DW1000 never received a hardware reset during `select()`. The chip started from power-on default state. `softReset()` was also skipped because `_rst != 0xFF`.
- **Fix:** Changed to `PIN_RST = 7` in all code.
- **Evidence:** SYS_CFG changed from `0x41200` (no reset) to `0x441200` (proper reset, DIS_DRXB correctly set). CRC errors appeared for the first time — meaning frames now get past PHY header decode.
- **Reference:** `docs/findings/DWS1000_PINOUT_AND_FIX.md` already specified pin 7 (from 2026-01-11) but it was never applied.

### BUG-002: setDefaults() Appears Harmful But Is Actually a No-Op in Loop
- **Severity:** Info (not a bug)
- **Status:** Investigated — not an issue
- **Date Found:** 2026-02-27
- **Details:** `DW1000.setDefaults()` has empty blocks for `TX_MODE` and `RX_MODE`. Since `newTransmit()` sets `_deviceMode = TX_MODE` and `newReceive()` sets `_deviceMode = RX_MODE`, calling `setDefaults()` after either is a no-op. It only resets config when `_deviceMode == IDLE_MODE` (during setup, before `commitConfiguration()`).
- **Impact:** None. The mode and RXAUTR settings from `commitConfiguration()` are preserved.

### BUG-003: 0xFFFFFFFF SPI Status Treated as Valid Frame
- **Severity:** High
- **Status:** Fixed in v8c
- **Date Found:** 2026-02-27
- **Root Cause:** When SPI returns all-ones (corruption), the status 0xFFFFFFFF has RXFCG (bit 14) and RXDFR (bit 13) both set, passing the "good frame" check.
- **Impact:** In v8b, PLL recovery broke SPI, causing every subsequent read to return 0xFFFFFFFF, which was counted as hundreds of "good" frames.
- **Fix:** Added explicit guard: `if (s == 0xFFFFFFFF) → SPI corruption`. Also updated `forceIdleVerified()` to reject 0xFFFFFFFF as a valid IDLE state.
- **Note:** Near-miss corruption also needs guarding. v9b added extended contradiction checks: FCG+FCE, FCG+RFSL, FCG+SFDTO, FCG+PHE, FCG without PRD/SFD. Also bit-count guard (>20 bits set = corruption).

### BUG-004: PLL Clock Recovery Breaks SPI Communication
- **Severity:** High
- **Status:** Workaround (use hardware reset instead)
- **Date Found:** 2026-02-27
- **Root Cause:** Toggling PMSC_CTRL0 SYSCLKS bits (XTI → AUTO) to re-lock PLL leaves the DW1000 in a state where all SPI reads return 0xFF.
- **Impact:** After PLL recovery in v8b, receiver appeared to work (counting hundreds of "frames") but was reading all-ones.
- **Fix:** Don't toggle clocks at runtime. Use hardware reset (PIN_RST toggle) for recovery instead.

### BUG-005: Receiver Goes Dead After ~20 Seconds
- **Severity:** High
- **Status:** Open (watchdog workaround in place)
- **Date Found:** 2026-02-27
- **Symptoms:** DW1000 stops detecting any preambles after ~20 seconds of active reception. SPI reads return valid non-0xFFFFFFFF values, but no RX event bits are set.
- **Frequency:** Consistent — happens after every hardware reset, takes ~15-25 seconds
- **Workaround:** Hardware reset watchdog (toggle PIN_RST after 50 empty cycles) recovers the receiver.
- **Possible Causes:**
  - CLKPLL_LL (PLL losing lock) accumulates and receiver can't re-lock
  - Receiver state machine gets stuck after multiple error conditions
  - Power supply sag under sustained RX current draw

### BUG-006: All CRC Error Frames Report len=0 (Root Cause: Signal Saturation)
- **Severity:** High
- **Status:** Root cause identified
- **Date Found:** 2026-02-27
- **Symptoms:** Frames that pass PHY header decode (RXDFR set, no RXPHE, RXFCE = CRC error) all report len=0. TX sends 10-byte "PING#XXXXX" payloads (12 bytes with CRC).
- **Root Cause:** PHY header length field decoded as 0 due to signal saturation at close range (<1m). Confirmed by raw RX_FINFO register reads in v8d — hardware register itself contains 0. RS error correction "corrects" corrupted header to wrong values.
- **Evidence (v8d raw reads):**
  - RX_FINFO raw = 0 for ALL CRC frames (hardware, not library issue)
  - HDR errors show random wrong lengths: 61, 78, 88, 110, 113 (expected: 12)
  - RX buffer contains stale data from previous cycles, no "PING" text visible
  - Buffer data changes coincide with HDR errors that have non-zero finfo_len
- **Library Note:** `getDataLength()` also returns 0 after `idle()` because `_deviceMode` changes to IDLE_MODE. This is a secondary issue; raw reads bypass it but still show 0.

### BUG-007: PHY Header Errors — NOT Signal Saturation, Likely PLL Instability
- **Severity:** Critical
- **Status:** Open — PLL instability suspected
- **Date Found:** 2026-02-27
- **Symptoms:** 0% successful frame reception regardless of TX power:
  - ~60% preamble/SFD detection, ~30% CRC errors (len=0), ~65% PHY header errors
  - CLKPLL_LL (PLL losing lock) on ~80% of error events
- **Disproven:** Signal saturation — TX_POWER=0x00 (min) vs 0x48 (default) produced identical behavior
- **Suspected Root Cause:** CLKPLL_LL = PLL can't maintain lock during RX. Oscillator drift corrupts bit timing for header/data decode while preamble detection (more frequency-tolerant) still works.
- **Evidence:** finfo_len=11 seen once (expected 12) — consistent with timing-induced bit errors. Receiver dies after ~20s (PLL can't recover).
- **Next Steps:** Try 64MHz PRF, crystal trimming, power supply check, different channel

### BUG-008: Two Distinct RX Failure Modes Discovered (Status Decode Analysis)
- **Severity:** Info (analysis finding)
- **Status:** Confirmed via status bit decode
- **Date Found:** 2026-02-27
- **Details:** Re-analyzing 6.8Mbps test (v8e) status codes with precise bit decode revealed that "HDR errors" are actually TWO different failure modes:
  1. **PHR Error (RXPHE, bit 12):** PHR decode fails. Status `0x2001301` = RXPRD + RXSFDD + RXPHE + CLKPLL_LL. Frame length decoded as wrong value.
  2. **Data Sync Loss (RXRFSL, bit 16):** PHR succeeds (RXPHD set, no RXPHE), but data portion loses synchronization. Status `0x2050B01` = RXPRD + RXSFDD + RXPHD + RXRFSL + LDEERR + CLKPLL_LL.
- **Key Insight:** SFD (Start of Frame Delimiter) is consistently detected (bit 9) across ALL error types. The problem occurs AFTER SFD, during PHR or data decode. Preamble detection (analog) and SFD detection both work — the failure is purely in digital decode.
- **At 6.8Mbps:** `fl=13` appeared (expected 12) with PHR detected flag — the PHR was almost correct but had uncorrectable bit errors under SECDED.
- **SYS_STATUS bit map reference:**
  - Bit 8: RXPRD (preamble detected)
  - Bit 9: RXSFDD (SFD detected)
  - Bit 10: LDEDONE (LDE processing done)
  - Bit 11: RXPHD (PHR detected OK)
  - Bit 12: RXPHE (PHR error)
  - Bit 13: RXDFR (data frame ready)
  - Bit 14: RXFCG (frame check good)
  - Bit 15: RXFCE (frame check error)
  - Bit 16: RXRFSL (frame sync loss — data portion)
  - Bit 18: LDEERR (LDE error)
  - Bit 24: RFPLL_LL (RF PLL losing lock)
  - Bit 25: CLKPLL_LL (system clock PLL losing lock)
- **PLL Note:** Only CLKPLL_LL (system clock PLL, 125MHz from 38.4MHz XTAL) is flagged. RFPLL_LL (RF carrier PLL) appears stable. System clock PLL instability affects all digital processing but not analog preamble detection.
- **Impact:** v9 test adds detailed status decode to properly categorize these failure modes.

### BUG-010: DWS1000 DC-DC Overvoltage — VDD = 3.67V (Above 3.6V Absolute Max)

- **Severity:** Critical
- **Status:** Open (hardware issue)
- **Date Found:** 2026-02-27
- **Root Cause:** DWS1000 shield's onboard DC-DC converter outputs ~3.67V to the DWM1000, exceeding the DW1000's absolute maximum rating of 3.6V (recommended 2.8–3.6V).
- **Evidence (v10 diagnostic test):**
  - SAR ADC raw = 255 (8-bit saturated), OTP Vmeas3V3 cal = 191 → calculated 3.67V
  - Voltage fluctuates 3.55V to 3.67V during operation (dips under load)
  - Multiple readings across 120s: 3.67, 3.67, 3.55, 3.62, 3.67, 3.67, 3.67, 3.60, 3.67, 3.67, 3.67V
  - Since raw=255 is max ADC value, actual voltage could be even higher
- **Impact:** Operating above absolute max rating causes:
  - PLL instability: CLKPLL_LL on 402/403 RX cycles (99.8%)
  - PLL locks once during init (CPLOCK=YES) but never re-locks during RX (CPLOCK=0 on all 403 cycles)
  - 0% successful frame reception despite correct preamble+SFD detection
  - Potential long-term damage to DWM1000 IC
- **PLLLDT:** Correctly applied (EC_CTRL=0x04, PLLLDT=ON) but doesn't help because root cause is overvoltage
- **Additional data:**
  - XTAL trim = 16 (default midrange, no factory calibration in OTP)
  - FS_PLLCFG = 0x800041D, FS_PLLTUNE = 0xBE (correct for Ch5)
  - Temperature: 32–35.5°C (normal)
  - LDO tuning: 0x88 (Device 1 on ACM1)
- **Hardware config:** J1 jumper not populated → DC-DC powers DWM1000 directly
- **Recommended fixes:**
  1. Add external LDO regulator (3.3V) between DC-DC and DWM1000
  2. Or add J1 jumper to switch power path (if shield supports LDO bypass)
  3. Add 100nF + 10µF decoupling capacitors at DWM1000 VDD pins
  4. Verify DC-DC output with oscilloscope for ripple measurement
- **Relationship to other bugs:** Likely root cause of BUG-005 (receiver death), BUG-006 (len=0), BUG-007 (PHE errors)

### BUG-009: Library enableMode() Doesn't Update Preamble Code for New PRF

- **Severity:** High
- **Status:** Workaround applied in v9b
- **Date Found:** 2026-02-27
- **Root Cause:** `enableMode()` calls `setPulseFrequency()` to change PRF (e.g., 16MHz to 64MHz) but doesn't call `setChannel()` to update the preamble code. The preamble code was already set by `setDefaults()` / `setChannel()` when PRF was still at the default 16MHz value.
- **Impact:** When using 64MHz PRF modes (`MODE_LONGDATA_RANGE_ACCURACY`, `MODE_*_ACCURACY`), the preamble code is wrong. For Channel 5: uses code 4 (16MHz) instead of code 10 (64MHz). Per DW1000 User Manual Table 61, codes 1-8 are for 16MHz PRF, codes 9-24 for 64MHz PRF.
- **Evidence:** v9 test showed `Mode: ... (code #4)` when using 64MHz PRF.
- **Workaround:** Call `DW1000.setChannel(5)` after `enableMode()` to re-derive the preamble code with the correct PRF value.
- **Note:** Both TX and RX had the same bug, so they matched each other (both used code 4). Still, using a 16MHz preamble code with 64MHz PRF is non-standard and may reduce preamble detection robustness.

## Resolved / Closed

### RESOLVED: IRQ Pin Never Asserts for RX Events
- **Date:** 2026-02-27
- **Status:** Won't fix — hardware limitation
- **Details:** DWS1000 shield's IRQ pin (D8→D2) works for TX events (30/30) but never asserts for RX events. Tested with manual SYS_MASK writes, library callbacks, both D2 and D8 pins.
- **Workaround:** Use SPI polling in IDLE mode for RX status.
- **Reference:** `docs/findings/IRQ_PIN_INVESTIGATION_2026-02-27.md`

### RESOLVED: RXAUTR Causes Corrupted Status Reads
- **Date:** 2026-02-27
- **Status:** Fixed
- **Details:** With `setReceiverAutoReenable(true)` (library default), the DW1000 re-enters RX mode before we can read status in IDLE. All status reads return corrupted values (0xFFFFFFFF).
- **Fix:** Explicitly call `DW1000.setReceiverAutoReenable(false)` before `commitConfiguration()`.

### RESOLVED: USB Upload Failure on Bus 3 Port 5
- **Date:** 2026-02-27 (originally 2026-01-11)
- **Status:** Fixed (software workaround)
- **Details:** ACM0 on USB Bus 3 Port 5 fails all uploads (stk500_getsync).
- **Fix:** USB sysfs device reset: `echo 0|sudo tee /sys/bus/usb/devices/3-5/authorized; sleep 2; echo 1|sudo tee /sys/bus/usb/devices/3-5/authorized`
