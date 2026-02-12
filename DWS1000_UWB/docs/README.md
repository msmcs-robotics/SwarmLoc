# DWS1000_UWB Project Documentation

## UWB Ranging System with Arduino Uno and DW1000

**Last Updated**: 2026-02-12
**Status**: TX 100%, RX 33% frame detection (SPI_EDGE fix applied, EMI during RX remains)
**Hardware**: Arduino Uno + PCL298336 v1.3 (DW1000 chip, Device ID: 0xDECA0130)
**Libraries**: arduino-dw1000 v0.9 (primary, with 3 AVR fixes), DW1000-ng (alternative)

**Development Approach**: Open to using multiple libraries and editing them as needed to understand the DWS1000 module.

---

## Quick Start

**New to this project? Start here:**

1. Read [scope.md](scope.md) — project boundaries and constraints
2. Read [todo.md](todo.md) — current tasks and blockers
3. Read [CRITICAL_HARDWARE_DISCOVERY.md](findings/CRITICAL_HARDWARE_DISCOVERY.md) — hardware is DW1000, NOT DWM3000
4. Read [LDO_TUNING_FIX_SUCCESS.md](findings/LDO_TUNING_FIX_SUCCESS.md) — critical PLL stability fix

---

## Project Documentation

| Document | Purpose |
|----------|---------|
| [scope.md](scope.md) | What this project is and isn't |
| [roadmap.md](roadmap.md) | Feature progress, session history, milestones |
| [todo.md](todo.md) | Current tasks and blockers |

## Key Findings

| Document | Purpose |
|----------|---------|
| [CRITICAL_HARDWARE_DISCOVERY.md](findings/CRITICAL_HARDWARE_DISCOVERY.md) | Hardware is DW1000, not DWM3000 |
| [INTERRUPT_ISSUE_SUMMARY.md](findings/INTERRUPT_ISSUE_SUMMARY.md) | Critical bug fix in DW1000.cpp |
| [LDO_TUNING_FIX_SUCCESS.md](findings/LDO_TUNING_FIX_SUCCESS.md) | LDO tuning from OTP — PLL stability fix |
| [SPI_EDGE_FIX_SESSION_2026-02-12.md](findings/SPI_EDGE_FIX_SESSION_2026-02-12.md) | SPI_EDGE root cause fix, IRQ reorder, 33% RX |
| [TX_RX_DEBUG_SESSION_2026-01-19.md](findings/TX_RX_DEBUG_SESSION_2026-01-19.md) | SPI corruption in RX mode analysis |
| [DW1000_CPLOCK_ISSUE.md](findings/DW1000_CPLOCK_ISSUE.md) | Power supply and PLL root cause |

## Hardware Reference

| Document | Purpose |
|----------|---------|
| [DW1000_LIBRARY_SETUP.md](findings/DW1000_LIBRARY_SETUP.md) | Library guide and API reference |
| [DW1000_LIBRARY_REVIEW.md](findings/DW1000_LIBRARY_REVIEW.md) | DW1000 vs DW1000-ng comparison |
| [DWS1000_PINOUT_AND_FIX.md](findings/DWS1000_PINOUT_AND_FIX.md) | Shield pinout and D8→D2 IRQ fix |
| [overview_DW1000.md](findings/overview_DW1000.md) | DW1000 chip overview |

## Architecture and Design

| Document | Purpose |
|----------|---------|
| [DUAL_ROLE_ARCHITECTURE.md](findings/DUAL_ROLE_ARCHITECTURE.md) | Swarm firmware design |
| [MULTILATERATION_IMPLEMENTATION.md](findings/MULTILATERATION_IMPLEMENTATION.md) | Positioning algorithms |
| [DW1000_RANGING_BEST_PRACTICES.md](findings/DW1000_RANGING_BEST_PRACTICES.md) | Ranging implementation guide |
| [TWR_ACCURACY_OPTIMIZATION.md](findings/TWR_ACCURACY_OPTIMIZATION.md) | Accuracy improvement strategies |

## ESP32 Migration (Future Reference)

| Document | Purpose |
|----------|---------|
| [ESP32_Migration_Guide.md](findings/ESP32_Migration_Guide.md) | Complete ESP32 guide |
| [ESP32_Connection_Summary.md](findings/ESP32_Connection_Summary.md) | Quick ESP32 wiring |
| [ESP32_Migration_Index.md](findings/ESP32_Migration_Index.md) | ESP32 documentation index |

## Session Archives

Session summaries and status reports are in [archive/](archive/).

---

## Hardware Configuration

### Arduino Uno Pin Mapping

| Function | Arduino Pin | DW1000 Connection |
|----------|-------------|-------------------|
| MOSI | D11 | SPI MOSI |
| MISO | D12 | SPI MISO |
| SCK | D13 | SPI CLK |
| CS | D10 | Chip Select |
| IRQ | D2 | Interrupt (via D8→D2 wire) |
| RST | D9 | Hardware Reset |

### Current Config

- **J1 Jumper**: NONE (DC-DC powers DWM1000 directly)
- **D8→D2 Wire**: Required on each shield (IRQ routing)
- **LDO Tuning**: Must apply from OTP after `commitConfiguration()`
- **Ports**: /dev/ttyACM0, /dev/ttyACM1

---

## Quick Commands

```bash
# Build, upload, and capture serial (non-blocking, recommended)
scripts/upload_and_capture.sh /dev/ttyACM0 tests/test_basic_sender.cpp 12

# Capture serial only (no recompile)
scripts/capture_only.sh /dev/ttyACM0 12

# Dual device: upload different firmware + capture both
scripts/upload_dual_and_capture.sh /dev/ttyACM0 tests/test_basic_sender.cpp \
                                    /dev/ttyACM1 tests/test_basic_receiver.cpp 12

# Legacy: PlatformIO direct
pio run -e uno -t upload --upload-port /dev/ttyACM0
python3 scripts/capture_serial.py /dev/ttyACM0 -n 50
```

See [scripts/COMMANDS_REFERENCE.md](../scripts/COMMANDS_REFERENCE.md) for full serial monitoring reference.

---

**Last Updated**: 2026-02-12
