# DWS1000_UWB - Todo

> Last updated: 2026-01-17 17:30

## CRITICAL: Hardware Power Issue - Software Cannot Fully Fix

**Root cause confirmed:** The DW1000's RF PLL cannot maintain lock due to power supply noise. DW1000 requires <25mV ripple on 3.3V supply.

**Final test results with DW1000-ng library:**

- TX sends successfully (TXFRS confirmed)
- RX detects 63+ frames in 25 seconds
- **ALL received data is corrupted garbage** (wrong lengths, garbled bytes)
- RFPLL_LL flag remains set (RF PLL losing lock)

**Evidence:**
- With J1 jumper to 3V3_ARDUINO: CPLOCK never sets (Arduino's 3.3V = 2.17V - too low!)
- With NO J1 jumper (DC-DC only): CPLOCK locks initially, but RFPLL_LL flag set
- DW1000-ng improves stability but cannot overcome hardware limitation

See: [DW1000_CPLOCK_ISSUE.md](findings/DW1000_CPLOCK_ISSUE.md)

## User Action Required (Priority Order)

### 1. Add capacitor (if available)

Solder or connect a 10-47µF capacitor between 3V3_DCDC and GND on the shield. This should filter power noise.

### 2. Try different USB setup

- Use shorter USB cable
- Try powered USB hub
- Try different USB ports (rear ports often cleaner than front)

### 3. External 3.3V power (if above fails)

Use a separate 3.3V power supply for the DWM1000, bypassing Arduino entirely.

## Current Hardware Configuration

| Item | Shield A (ACM0) | Shield B (ACM1) |
|------|-----------------|-----------------|
| J1 Jumper | **NONE** (recommended) | **NONE** (recommended) |
| D8→D2 Wire | Yes | Yes |
| CPLOCK | Locks with DW1000-ng | Locks with DW1000-ng |
| RFPLL_LL | SET (losing lock) | SET (losing lock) |
| Stability | **Crashes often** | **Stable** |

**Note:** DEV0 (ACM0) crashes frequently - possible hardware defect on that shield. DEV1 (ACM1) is more stable with DW1000-ng library.

## Voltage Measurements (No J1 Jumper)

| Pin | Voltage | Status |
|-----|---------|--------|
| J1 | 3.2V | OK - DWM1000 getting power |
| 3V3_ARDUINO | 2.17V | **BAD** - Arduino regulator failing |
| 3V3_DCDC | 3.3V | OK - Shield DC-DC working |

## Blocked

- [ ] Stable RF communication — **Blocked by**: RFPLL_LL power instability
- [ ] Ranging measurements — **Blocked by**: RF not stable
- [ ] All project milestones — **Blocked by**: hardware power issue

## Backlog (After Power Fixed)

- [ ] Verify stable PING/PONG communication
- [ ] Antenna delay calibration
- [ ] Ranging accuracy verification (±10-20 cm target)
- [ ] Multi-node swarm testing
- [ ] ESP32 migration (better power, more RAM)

## Library Status

**Using: DW1000-ng** (switched from original DW1000 library)

| Feature | DW1000 (original) | DW1000-ng |
|---------|-------------------|-----------|
| CPLL lock detect | Missing | ✓ Enabled |
| XTAL trim from OTP | Missing | ✓ Applied |
| Slow SPI during init | No | ✓ Yes |
| Stability | Poor | Better |

See: [DW1000_LIBRARY_REVIEW.md](findings/DW1000_LIBRARY_REVIEW.md)

## Recently Completed

- [x] **DW1000-ng TX/RX test** — RF works, 63+ frames detected, all data corrupted — 2026-01-17
- [x] **Switched to DW1000-ng library** — improves CPLOCK stability — 2026-01-17
- [x] **Library code review** — DW1000-ng has better PLL handling — 2026-01-17
- [x] **Root cause: RFPLL_LL due to power noise** — hardware issue, software cannot fix — 2026-01-17
- [x] **RF actually works briefly** (data received, but corrupted) — 2026-01-17
- [x] Voltage measurements: DC-DC=3.3V, Arduino=2.17V, J1=3.2V — 2026-01-17
- [x] Discovered: No J1 jumper = DC-DC powers DWM1000 directly — 2026-01-17
- [x] Found CPLOCK issue with J1→3V3_ARDUINO — 2026-01-17
- [x] Added J1 jumper to second shield — 2026-01-17
- [x] Both devices show DECA device ID — 2026-01-17

---

## Test Files

**DW1000-ng tests (current):**

- `tests/test_dw1000ng_simple.cpp` - Basic DW1000-ng stability test
- `tests/test_dw1000ng_tx.cpp` - TX test with DW1000-ng
- `tests/test_dw1000ng_rx.cpp` - RX test with DW1000-ng

**Legacy tests (original DW1000 library):**

- `tests/test_cplock.cpp` - CPLOCK/RFPLL diagnostic
- `tests/polling_tx.cpp` - TX test with polling
- `tests/polling_rx.cpp` - RX test with polling

## Research Sources

- [Qorvo Forum: CLKPLL_LL issue](https://forum.qorvo.com/t/dw1000-cant-receive-at-all-due-to-clkpll-ll-being-set-constantly/13862)
- [GitHub: arduino-dw1000 Issue #42](https://github.com/thotro/arduino-dw1000/issues/42)

---

_Update every session: start by reading, end by updating._
