# DWS1000_UWB - Todo

> Last updated: 2026-02-28

## In Progress

_Tasks actively being worked on_

(none)

## Up Next

_Priority queue for immediate work_

- [ ] Wire OLED display (SDA=A4, SCL=A5) and test calibration display
- [ ] Multi-distance validation (~10cm, ~60cm, ~110cm — desk range)
- [ ] Calibration serial output in config.h-ready format (copy-paste values)

## Backlog

_Lower priority, do when time permits_

- [ ] ESP32 migration (if Arduino Uno accuracy insufficient)
- [ ] Temperature compensation for antenna delay
- [ ] NLOS detection / signal quality filtering
- [ ] Integration readiness assessment

## Recently Completed

_For context; clear periodically_

- [x] Separate PIO envs: uno_anchor, uno_tag, uno_calibration — 2026-02-28
- [x] Source refactor: anchor_main.cpp, tag_main.cpp, calibration_main.cpp — 2026-02-28
- [x] config.h with CALIBRATED markers, antenna delay, feature flags — 2026-02-28
- [x] display.h (U8x8 OLED, no-op stubs when disabled) — 2026-02-28
- [x] OLED display calls wired into calibration firmware — 2026-02-28
- [x] U8g2 library copied to lib/ — 2026-02-28
- [x] tools/dev.sh workflow tool + tools/serial_monitor.py — 2026-02-28
- [x] Calibration workflow documented (docs/features/calibration-workflow.md) — 2026-02-28
- [x] README updated with pinout, J1 jumper, setup guide — 2026-02-28
- [x] Cleaned up old backup files from src/ — 2026-02-28
- [x] Live ranging verified with new config.h firmware (37 Hz) — 2026-02-28
- [x] Antenna delay calibration — 16436->16405, +4.6 cm error — 2026-02-27
- [x] TWR ranging — 9.4 Hz, +/-4.4 cm StdDev — 2026-02-27
- [x] J1 jumper root cause — DWM1000 power floating — 2026-02-27

---

## Notes

_Context affecting current tasks_

**Current hardware config:** J1 jumper on pins 1-2 (REQUIRED), D8->D2 IRQ wire, DW1000-ng library.

**TWR config:** 850kbps, 16MHz PRF, Ch5, Preamble 256, Code 3, antenna delay **16405** (calibrated).

**Device roles:** ACM0 = anchor (LDO 0x88, better RX), ACM1 = tag (LDO 0x28).

**Build envs:** `uno_anchor` (ACM0), `uno_tag` (ACM1, default), `uno_calibration` (calibration + OLED).

**Config pattern:** All calibration values in `include/config.h`, copied from calibration serial output.

**OLED:** DSDTECH 0.91" SSD1306 128x32, not yet wired. Pins A4(SDA)/A5(SCL). U8g2 library in lib/.

**Calibration requires 2 devices** — antenna delay measured via TWR, correction split equally.

**Distance range for testing:** Can adjust ~50 cm closer or further from current position.

---

*Update every session: start by reading, end by updating.*
