# DWS1000_UWB - Findings Index

> Last updated: 2026-01-17

This folder contains research, investigations, and discoveries for the DWS1000_UWB project.

**How to use**: Search for keywords (error messages, component names) before doing expensive research. Don't read everything - use grep to find relevant files.

---

## Quick Reference

### Critical Issues (Read First If Stuck)

| File | Topic |
|------|-------|
| [INTERRUPT_ISSUE_SUMMARY.md](INTERRUPT_ISSUE_SUMMARY.md) | DW1000 interrupt bug and fix |
| [QUICK_FIX.md](QUICK_FIX.md) | Step-by-step interrupt fix |
| [CRITICAL_HARDWARE_DISCOVERY.md](CRITICAL_HARDWARE_DISCOVERY.md) | Hardware is DW1000, NOT DWM3000 |
| [DWS1000_PINOUT_AND_FIX.md](DWS1000_PINOUT_AND_FIX.md) | Pin configuration issues |

### Hardware Research

| File | Topic |
|------|-------|
| [hardware-research.md](hardware-research.md) | Hardware specifications |
| [DWM3000_vs_DW1000_COMPARISON.md](DWM3000_vs_DW1000_COMPARISON.md) | Chip comparison |
| [ESP32_MIGRATION_RESEARCH.md](ESP32_MIGRATION_RESEARCH.md) | ESP32 alternative (future) |

### Library Research

| File | Topic |
|------|-------|
| [DW1000_LIBRARY_SETUP.md](DW1000_LIBRARY_SETUP.md) | Library installation guide |
| [DW1000_LIBRARY_ALTERNATIVES.md](DW1000_LIBRARY_ALTERNATIVES.md) | Other library options |
| [library-integration.md](library-integration.md) | Integration notes |
| [LIB_FOLDER_CLEANUP.md](LIB_FOLDER_CLEANUP.md) | Library folder organization |

### Troubleshooting

| File | Topic |
|------|-------|
| [interrupt_debugging.md](interrupt_debugging.md) | Deep interrupt analysis |
| [DW1000_NO_RANGING_TROUBLESHOOTING.md](DW1000_NO_RANGING_TROUBLESHOOTING.md) | Ranging issues |
| [DWS1000_IRQ_AND_COMMUNICATION_DEBUG.md](DWS1000_IRQ_AND_COMMUNICATION_DEBUG.md) | IRQ debugging |
| [ARDUINO_UPLOAD_TROUBLESHOOTING.md](ARDUINO_UPLOAD_TROUBLESHOOTING.md) | Upload issues |
| [ACM1_SPECIFIC_TROUBLESHOOTING.md](ACM1_SPECIFIC_TROUBLESHOOTING.md) | USB port issues |
| [BOOTLOADER_RECOVERY_ISP.md](BOOTLOADER_RECOVERY_ISP.md) | Bootloader recovery |

### Ranging & Calibration

| File | Topic |
|------|-------|
| [DW1000_RANGING_BEST_PRACTICES.md](DW1000_RANGING_BEST_PRACTICES.md) | Ranging implementation |
| [ANTENNA_DELAY_CALIBRATION_2026.md](ANTENNA_DELAY_CALIBRATION_2026.md) | Calibration procedures |
| [TWR_ACCURACY_OPTIMIZATION.md](TWR_ACCURACY_OPTIMIZATION.md) | Accuracy improvement |
| [CALIBRATION_GUIDE.md](CALIBRATION_GUIDE.md) | Calibration guide |

### Architecture & Design

| File | Topic |
|------|-------|
| [DUAL_ROLE_ARCHITECTURE.md](DUAL_ROLE_ARCHITECTURE.md) | Swarm firmware design |
| [MULTILATERATION_IMPLEMENTATION.md](MULTILATERATION_IMPLEMENTATION.md) | Positioning algorithms |
| [UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md](UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md) | Swarm scalability |

### Test Results

| File | Topic |
|------|-------|
| [TEST_RESULTS.md](TEST_RESULTS.md) | Comprehensive test log |
| [RANGING_TEST_SESSION_2026-01-11.md](RANGING_TEST_SESSION_2026-01-11.md) | Test session notes |

### Session Summaries

| File | Topic |
|------|-------|
| [SESSION_SUMMARY_2026-01-11.md](SESSION_SUMMARY_2026-01-11.md) | Jan 11 session |
| [session-summary.md](session-summary.md) | General summary |
| [RESEARCH_SUMMARY.md](RESEARCH_SUMMARY.md) | Research consolidated |

---

## All Files (Alphabetical)

```
ACM1_DIAGNOSIS_FINAL.md
ACM1_SPECIFIC_TROUBLESHOOTING.md
ANTENNA_DELAY_CALIBRATION_2026.md
ARDUINO_UPLOAD_TROUBLESHOOTING.md
BOOTLOADER_RECOVERY_ISP.md
BUG_FIX_GUIDE.md
CALIBRATION_GUIDE.md
CALIBRATION_WEB_RESEARCH.md
code-review.md
CRITICAL_HARDWARE_DISCOVERY.md
DUAL_ROLE_ARCHITECTURE.md
DW1000_LIBRARY_ALTERNATIVES.md
DW1000_LIBRARY_SETUP.md
DW1000_NO_RANGING_TROUBLESHOOTING.md
DW1000_RANGING_BEST_PRACTICES.md
DWM3000_vs_DW1000_COMPARISON.md
DWS1000_IRQ_AND_COMMUNICATION_DEBUG.md
DWS1000_PINOUT_AND_FIX.md
ESP32_MIGRATION_RESEARCH.md
FIX_RANGING_NOW.md
hardware-research.md
interrupt_debugging.md
INTERRUPT_ISSUE_SUMMARY.md
LIB_FOLDER_CLEANUP.md
library-integration.md
LIBRARY_PATCH.md
MULTILATERATION_IMPLEMENTATION.md
QUICK_FIX.md
RANGING_TEST_SESSION_2026-01-11.md
RESEARCH_SUMMARY.md
SESSION_SUMMARY_2026-01-11.md
session-summary.md
summary.md
TEST_RESULTS.md
TWR_ACCURACY_OPTIMIZATION.md
UPLOAD_ISSUE_RESOLUTION.md
UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md
web-research.md
```

---

## Search Tips

```bash
# Find files mentioning "interrupt"
grep -l "interrupt" *.md

# Find files mentioning specific error
grep -l "stk500_getsync" *.md

# Find recent troubleshooting
ls -lt *.md | head -10
```

---

_Add new findings as you learn. Use descriptive names with keywords for searchability._
