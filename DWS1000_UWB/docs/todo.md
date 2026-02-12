# DWS1000_UWB - Todo

> Last updated: 2026-02-12

## In Progress

_Tasks actively being worked on_

- [ ] Improve RX reliability beyond 33% frame detection rate
  - Current: 10/30 frames received with polling + watchdog approach
  - See: [SPI_EDGE_FIX_SESSION_2026-02-12.md](findings/SPI_EDGE_FIX_SESSION_2026-02-12.md)

## Blocked

_Tasks waiting on something_

- [ ] TWR ranging measurements — **Blocked by**: RX reliability too low for protocol handshakes
- [ ] Antenna delay calibration — **Blocked by**: working ranging required
- [ ] Multi-node swarm testing — **Blocked by**: working ranging required

## Up Next

_Priority queue for immediate work_

- [ ] Try SPI_MODE1 or SPI_MODE3 for better timing margins during RX
- [ ] Add CRC verification of received data at application level
- [ ] Test with antennas removed (RF coupling vs power noise)
- [ ] Add bypass capacitors (10-47uF on 3V3_DCDC, 100nF near DWM1000)
- [ ] Test with external 3.3V power supply (bypass Arduino/shield power)
- [ ] Try reduced data rate (850kbps instead of 6.8Mbps)

## Backlog

_Lower priority, do when time permits_

- [ ] ESP32 migration (should eliminate all SPI/power issues)
- [ ] Oscilloscope analysis of 3V3_DCDC during RX mode
- [ ] SPI shielding test (ferrite beads on SPI lines)
- [ ] Try alternative FS_PLLTUNE value (0xA6 instead of 0xBE for Channel 5)

## Recently Completed

_For context; clear periodically_

- [x] SPI_EDGE_BIT fix — root cause of 0xFF reads on AVR — 2026-02-12
- [x] IRQ handler priority fix — isReceiveDone() before isReceiveFailed() — 2026-02-12
- [x] Polling RX with double-read retry and watchdog — 33% frame rate — 2026-02-12
- [x] Non-blocking serial monitoring scripts — 2026-02-12
- [x] BasicSender/BasicReceiver library example tests — 2026-02-12
- [x] SPI diagnostic test — isolated corruption to RX mode only — 2026-01-19
- [x] TX with IRQ callbacks — 100% success both devices — 2026-01-19
- [x] LDO tuning fix — both devices 98%+ PLL stable — 2026-01-19
- [x] DW1000-ng TX/RX test — RF works, data corrupted — 2026-01-17
- [x] J1 jumper investigation — no jumper = best config — 2026-01-17
- [x] USB hub upload issue — resolved by moving to direct port — 2026-01-11
- [x] Critical interrupt bug fix (DW1000.cpp buffer overrun) — 2026-01-11
- [x] Hardware identification (DW1000, not DWM3000) — 2026-01-08

---

## Notes

_Context affecting current tasks_

**Current hardware config:** No J1 jumper, D8→D2 IRQ wire, LDO tuning applied post-config.

**SPI_EDGE_BIT (2026-02-12):** The library's `select()` sets SPI_EDGE_BIT (bit 10 of SYS_CFG), which is incompatible with AVR SPI hardware. This was the root cause of ALL reads returning 0xFF. Fixed with `#if !defined(__AVR__)` guard. See [SPI_EDGE_FIX_SESSION_2026-02-12.md](findings/SPI_EDGE_FIX_SESSION_2026-02-12.md).

**SPI reliability by DW1000 state:** IDLE=100%, TX=~100%, RX=75-90%. The remaining RX corruption is likely EMI from the radio front-end. Mitigation: double-read with retry + watchdog restarts.

**IRQ handler reordered:** `handleInterrupt()` now checks `isReceiveDone()` before `isReceiveFailed()`. PLL sticky bits (`isClockProblem()`) moved to end. Prevents valid frames from being discarded due to SPI glitches.

**LDO tuning is critical:** Must re-apply after `commitConfiguration()` because `tune()` reconfigures PLL. DEV0=0x88, DEV1=0x28.

**Device roles:** ACM0 (LDO 0x88) is better receiver than ACM1 (LDO 0x28).

---

*Update every session: start by reading, end by updating.*
