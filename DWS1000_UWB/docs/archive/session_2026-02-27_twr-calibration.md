# Session 2026-02-27: TWR Ranging + Antenna Delay Calibration

## What Was Accomplished

### J1 Jumper Root Cause Fix
- Identified that missing J1 jumper (pins 1-2) was the root cause of all RX failures
- DWM1000 power rail was floating; module powered parasitically through SPI ESD diodes
- With J1 installed: PLL clock loss dropped from 75-99% to 0%, RX now works reliably

### DW1000-ng Library Selection
- Switched from thotro DW1000 library to DW1000-ng
- thotro library had broken RX (0 events) and incompatible frame format
- DW1000-ng has proper init sequence (XTI clock, PLLLDT, clock sequencing)
- RX success: ACM0=100%, ACM1=78%

### TWR Ranging Implementation
- Implemented asymmetric Two-Way Ranging (4-message protocol)
- Anchor: `tests/test_twr_anchor.cpp` — responds to POLL, computes distance
- Tag: `tests/test_twr_tag.cpp` — initiates ranging, receives distance
- Performance: 9.4 Hz ranging rate, 97% success, 0 RX failures

### Antenna Delay Calibration
- Default delay 16436 produced 30 cm systematic error at 24" known distance
- Created `tests/test_calibration_tag.cpp` — auto-iterating calibration firmware
- Calibrated to **16405** (-31 ticks per device)
- Verification: mean 0.656 m at 0.610 m known distance, **+4.6 cm error**
- StdDev ±4.4 cm, well within ±10-20 cm target accuracy

## Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Library | DW1000-ng | thotro RX broken, incompatible frames |
| J1 jumper | Pins 1-2 installed | Root cause of all RX failures |
| Antenna delay | 16405 | Calibrated from default 16436 at 24" |
| TWR config | 850kbps, 16MHz PRF, Ch5, Preamble 256 | Best balance of speed/reliability |

## Scope Status

**All scope requirements are now met:**
- [x] Both devices send/receive UWB frames
- [x] TWR ranging produces distance measurements
- [x] Measurements consistent and repeatable (±4.4 cm StdDev)
- [x] Serial output shows distance in human-readable format
- [x] Target accuracy ±10-20 cm after calibration (+4.6 cm achieved)

## What's Next

- Multi-distance validation (0.5m, 1m, 2m, 3m, 5m) — optional
- Integration readiness assessment — if this project feeds into other SwarmLoc components
- ESP32 migration — only if Arduino Uno accuracy proves insufficient

## Files Created/Modified This Session

### New Files
- `tests/test_calibration_tag.cpp` — auto-iterating calibration firmware
- `tests/test_twr_anchor.cpp` — TWR anchor (responder)
- `tests/test_twr_tag.cpp` — TWR tag (initiator)
- `docs/findings/ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md` — calibration results
- `docs/findings/RX_DIAGNOSTIC_SESSION_2026-02-27.md` — J1 fix + RX diagnostics
- `docs/findings/IRQ_PIN_INVESTIGATION_2026-02-27.md` — IRQ pin investigation
- `docs/bugs.md` — bug tracker (BUG-010: DC-DC overvoltage)
- `docs/known_issues.md` — known issues list
- Many `tests/test_rx_*.cpp` and `tests/test_tx_*.cpp` — diagnostic test variants

### Modified Files
- `docs/roadmap.md` — added TWR + calibration session (v7.0)
- `docs/scope.md` — all requirements marked complete
- `docs/todo.md` — calibration complete, updated notes
- `lib/DW1000-ng/src/DW1000Ng.cpp` — added public readBytes/writeBytes
- `lib/DW1000-ng/src/DW1000Ng.hpp` — corresponding header changes
- `platformio.ini` — added uno_ng environment
- `scripts/upload_dual_and_capture.sh` — improvements
- `src/main.cpp` — current TWR tag firmware
