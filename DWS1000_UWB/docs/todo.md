# DWS1000_UWB - Todo

> Last updated: 2026-02-27

## In Progress

_Tasks actively being worked on_

(none)

## Up Next

_Priority queue for immediate work_

- [ ] Multi-distance validation (0.5m, 1m, 2m, 3m, 5m)
- [ ] Integration readiness assessment

## Backlog

_Lower priority, do when time permits_

- [ ] ESP32 migration (if Arduino Uno accuracy insufficient)
- [ ] Temperature compensation for antenna delay
- [ ] NLOS detection / signal quality filtering

## Recently Completed

_For context; clear periodically_

- [x] Antenna delay calibration — 16436→16405, error +4.6 cm, ±10-20 cm target met — 2026-02-27
- [x] TWR ranging — 565 ranges in 60s, 9.4 Hz, ±4.4 cm StdDev — 2026-02-27
- [x] J1 jumper root cause — DWM1000 power floating, fixed with J1 pins 1-2 — 2026-02-27
- [x] DW1000-ng library selected — thotro RX broken, incompatible frames — 2026-02-27
- [x] RX working — ACM0 100%, ACM1 78% with J1 + DW1000-ng — 2026-02-27
- [x] XTAL trim sweep — no effect, ruled out — 2026-02-27
- [x] PLLLDT enable — correct but insufficient without J1 — 2026-02-27
- [x] SPI_EDGE_BIT fix — root cause of 0xFF reads on AVR — 2026-02-12
- [x] IRQ handler priority fix — 2026-02-12
- [x] Non-blocking serial monitoring scripts — 2026-02-12
- [x] TX with IRQ callbacks — 100% success both devices — 2026-01-19
- [x] LDO tuning fix — both devices PLL stable — 2026-01-19
- [x] USB hub upload issue — resolved — 2026-01-11
- [x] Critical interrupt bug fix (DW1000.cpp buffer overrun) — 2026-01-11
- [x] Hardware identification (DW1000, not DWM3000) — 2026-01-08

---

## Notes

_Context affecting current tasks_

**Current hardware config:** J1 jumper on pins 1-2 (REQUIRED), D8→D2 IRQ wire, DW1000-ng library.

**TWR config:** 850kbps, 16MHz PRF, Ch5, Preamble 256, Code 3, antenna delay **16405** (calibrated).

**Device roles:** ACM0 = anchor (LDO 0x88, better RX), ACM1 = tag (LDO 0x28).

**Test files:** `tests/test_twr_anchor.cpp`, `tests/test_twr_tag.cpp`.

---

*Update every session: start by reading, end by updating.*
