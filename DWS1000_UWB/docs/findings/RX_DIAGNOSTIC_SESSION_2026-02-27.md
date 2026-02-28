# RX Diagnostic Session — 2026-02-27

## Summary

Comprehensive investigation into 0% RX success rate on DWS1000 shields with Arduino Uno.
Root cause identified: **J1 jumper missing — DWM1000 power rail was floating**.
After installing J1 jumper (pins 1-2, DC-DC → DWM1000): **RX works — 78-100% success rate with DW1000-ng library**.

## Test Matrix

| Test | Config | Good | CRC | PHE | RFSL | CLKPLL_LL | Notes |
|------|--------|------|-----|-----|------|-----------|-------|
| v9 (64MHz PRF) | 64MHz/Ch5/110k, wrong preamble code | 0 | 2 | 1 | 0 | ~100% | BUG-009 found |
| v9b (64MHz fix) | 64MHz/Ch5/110k, code #10 | 0 | 6 | 41 | 1 | 323/~450 | Better preamble detection |
| v9c (Channel 2) | 16MHz/Ch2/110k | 0 | 12 | 58 | 3 | 298/~450 | Not PLL-frequency dependent |
| v9d (swap) | 16MHz/Ch5/110k, ACM0 as RX | 0 | 1 | 51 | 5 | 344/~450 | Not device-specific |
| v9e (PLLLDT wrong) | 16MHz/Ch5/110k, EC_CTRL=0x04 | 0 | 20 | 56 | 7 | ~75% | PLLLDT write failed (wrong offset) |
| v9e (PLLLDT fixed) | 16MHz/Ch5/110k, EC_CTRL=0x00 | 0 | 9 | 28 | 3 | 373/~500 | PLLLDT=ON, no improvement |
| v10 (diagnostics) | 16MHz/Ch5/110k, full diag | 0 | 41 | 67 | 6 | 402/403 | VDD=3.67V discovered |
| v11 (XTAL sweep) | Trim 0-31, 20 cyc each | 0 | 30 | 45 | 5 | 55-100% | All trims fail equally |
| DW1000-ng (ACM1 RX) | ng lib, polling, Ch5/110k | 0 | 16 | 27 | 2 | 330/407 | Identical buffer data |
| DW1000-ng (ACM0 RX) | ng lib, polling, Ch5/110k | 0 | 5 | 40 | 4 | 306/407 | 7 WD resets, worse |
| **--- J1 JUMPER INSTALLED (pins 1-2) ---** | | | | | | | |
| ng + J1 (ACM1 RX) | ng lib, J1 on, Ch5/110k | **35/45** | 18 | 54 | 4 | **0/406** | **78% success!** |
| ng + J1 (ACM0 RX) | ng lib, J1 on, Ch5/110k | **44/44** | **0** | 24 | 0 | **0/404** | **100% success!** |
| thotro + J1 (ACM0 RX) | thotro RX, J1 on | 0 | 0 | 0 | 0 | N/A | thotro RX broken |
| cross-lib + J1 | thotro TX + ng RX, J1 on | 0 | 3 | 35 | 2 | **0/245** | Frame format mismatch |

## Key Findings

### 1. Overvoltage (BUG-010) — ROOT CAUSE CANDIDATE

DW1000 SAR ADC reads:
- Raw Vbat = 255 (8-bit ADC saturated)
- Calibration: OTP Vmeas3V3 = 191 (factory 3.3V reference)
- Calculated: (255 - 191) / 173 + 3.3 = **3.67V**
- DW1000 absolute max: **3.6V**
- Voltage fluctuates: 3.55V to 3.67V during operation

The DWS1000 shield's DC-DC converter (no J1 jumper) is outputting above spec voltage.

### 2. PLL Never Re-Locks During RX

- `Pre-config CPLOCK=YES` — PLL locks correctly at init
- After clearing status and waiting 10ms or 60ms: CPLOCK=NO, CLKPLL_LL=NO
- During RX: CPL:0/403 — CPLOCK **never set** during any RX cycle
- CLKPLL_LL: 402/403 cycles (99.8%) — PLL continuously losing lock

This means: PLL locks once during init, then continuously loses lock during RX and never re-locks.

### 3. PLLLDT Fix — Correct But Insufficient

- Previous attempt used EC_CTRL_SUB = 0x04 (wrong, write went to wrong sub-register)
- Correct offset: EC_CTRL_SUB = 0x00 (confirmed via lab11/dw1000-driver)
- PLLLDT now correctly written (verified by readback: EC_CTRL=0x04, PLLLDT=ON)
- But no improvement because root cause is overvoltage, not missing init step

### 4. Two Distinct Failure Modes Confirmed

1. **PHR Error (RXPHE):** ~60% of RX events. PHR decode fails → wrong/zero length
2. **CRC Error (RXFCE):** ~30% of RX events. PHR "succeeds" but len=0, buffer stale, CRC fails

Both have CLKPLL_LL set. The PLL instability corrupts digital timing:
- PHR decode gets wrong bits → RXPHE
- PHR decode "succeeds" but with wrong length (0) → data never written → RXFCE

### 5. Buffer Data Analysis

CRC frame buffers across tests:
- v10: `1F 01 F2 CC 7B C6 82 6D` (consistent within test)
- v9e: `44 01 F2 CC 33 C6 82 6D` and `83 5D DA B9 CA 7E EB 7D`
- Data changes between watchdog resets but never contains "PING" text
- This confirms: RX buffer is never written with actual received data

### 6. Configuration Verified Correct

- FS_PLLCFG: 0x800041D (correct for Ch5)
- FS_PLLTUNE: 0xBE (correct for Ch5)
- SYS_CFG: 0x441200 (DIS_DRXB set, proper reset applied)
- XTAL trim: 16 (default midrange, no OTP calibration)
- LDO tuning: 0x88 (applied correctly)

### 7. Temperature Normal

32-35.5C during operation — not a thermal issue.

### 8. XTAL Trim Sweep — No Effect

Swept all 32 crystal trim values (0-31), 20 RX cycles each:
- Zero good frames at ALL trim values
- PLL_LL ranges 55-100% uniformly — trim has no effect on PLL stability
- Best trim value = 16 (already the default midrange)
- Confirms PLL instability is NOT caused by crystal frequency offset

### 9. DW1000-ng Library — Same Result

Tested with DW1000-ng library (known for better PLL initialization):
- DW1000-ng forces XTI clock, configures PLLLDT/XTAL/LDE on crystal clock, then switches to PLL
- **Result: 0 good frames** — same as thotro library
- 16 CRC (identical buffer: `44 11 F2 CC 33 C6 82 6D`), 27 PHE, 2 RFSL
- CLKPLL_LL: 330/407 (81%)
- Previous DW1000-ng test (2026-01-17) got 63+ detections — difference may be power degradation

### 10. J1 Jumper Power Architecture — ROOT CAUSE CONFIRMED

DWS1000 schematic (V1.2) research reveals J1 is a 3-pin header:
- Pin 1: 3V3_DCDC (from Torex XC9282B33E1R-G, 600mA)
- Pin 2: 3V3 (to DWM1000)
- Pin 3: 3V3_ARDUINO (from Arduino's 50-150mA LDO)

**With NO jumper (current configuration), DWM1000 pin 2 is FLOATING.**
DWM1000 only gets parasitic power through 5V SPI ESD protection diodes.
This explains:

- VDD = 3.67V (5V minus diode drops, unregulated)
- PLL instability (noisy, unregulated supply)
- 0% RX success rate

**Fix: Install jumper on J1 pins 1-2** (DC-DC → DWM1000).

### 11. J1 Jumper Fix — RX NOW WORKS

After installing J1 jumper on pins 1-2 (DC-DC → DWM1000) on both shields:

- **CLKPLL_LL dropped from 75-99% to 0%** — PLL is completely stable
- **ACM0 as RX: 44/44 = 100% good frames** (zero CRC errors, zero watchdog resets)
- **ACM1 as RX: 35/45 = 78% good frames** (18 CRC, still good)
- All received data is correct: "PING#00001", "PING#00002", etc.
- No RF PLL losses, no SPI corruption, no watchdog resets needed (ACM0)
- PHE events still occur (~24-54 per 90s) from empty polling cycles, not signal issues

### 12. Library Comparison — DW1000-ng Required

With J1 jumper installed:

- **thotro RX: completely broken** — 0 events in 90s, receiver never detects anything
- **thotro TX + DW1000-ng RX: 0 good frames** — CRC always fails (incompatible frame format)
- **DW1000-ng TX + DW1000-ng RX: 78-100% success**
- Conclusion: **DW1000-ng must be used for both TX and RX**
- thotro library has broken RX initialization and incompatible frame format with DW1000-ng

## What Was Ruled Out

| Hypothesis | Test | Result |
|-----------|------|--------|
| Channel-specific PLL issue | Ch2 vs Ch5 | Same pattern |
| Device-specific hardware | Swap TX/RX | Both identical |
| Signal saturation | Min TX power | Same result |
| Missing PLLLDT init | Applied with correct offset | No improvement |
| PRF mismatch | 16MHz and 64MHz PRF | Both fail |
| Data rate | 110kbps, 850kbps, 6.8Mbps | All fail |
| Frame check mode | CRC on/off | Both fail |
| SPI corruption | Double-read, contradiction guards | Real errors not SPI |

## Recommended Next Steps

### RESOLVED
- J1 jumper installed on pins 1-2 — **RX works**
- DW1000-ng library selected — **both TX and RX functional**
- XTAL trim sweep — ruled out (no effect)
- PLL re-calibration — not needed with proper power

### TWR Ranging — WORKING

Implemented asymmetric TWR using DW1000-ng (850kbps, 16MHz PRF, Ch5, Preamble 256):

- **2487 ranges in 60s = ~41 Hz**
- **97% success rate** (76 timeouts / 2564 polls)
- **Mean: 0.626 m, StdDev: +/-0.079 m (+/-8 cm)**
- RX power: -66 to -70 dBm
- Antenna delay: 16436 (default, not yet calibrated)
- Test files: `tests/test_twr_anchor.cpp`, `tests/test_twr_tag.cpp`

Next: antenna delay calibration for absolute accuracy, then multilateration
