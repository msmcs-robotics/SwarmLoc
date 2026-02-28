# Antenna Delay Calibration Session — 2026-02-27

## Summary

Calibrated antenna delay for DWM1000 modules using iterative TWR measurement at a known distance. Adjusted from default 16436 to **16405** (-31 ticks per device). Verification pending.

## Background

After the J1 jumper fix and TWR implementation (same session), the ranging system was measuring ~0.31m mean when devices were approximately 24" (0.61m) apart. This ~30cm systematic error is caused by uncalibrated antenna delay — the DW1000's internal RF propagation delay that must be set in firmware.

## Calibration Method

### Setup

- Two DWS1000 shields on Arduino Unos
- ACM0 = anchor (`tests/test_twr_anchor.cpp`)
- ACM1 = tag (`tests/test_calibration_tag.cpp`)
- Config: 850kbps, 16MHz PRF, Ch5, Preamble 256, Code 3
- Known distance: 24 inches (0.6096 m), measured antenna-to-antenna, ±2 inches

### Algorithm

The calibration tag firmware (`test_calibration_tag.cpp`) implements:

1. Perform 200 TWR measurements at current antenna delay
2. Compute mean, stddev, min, max of measured distances
3. Compute error: `error = mean_measured - known_distance`
4. Compute per-device adjustment: `adj = (error / 2) / DISTANCE_OF_RADIO`
5. Apply new delay: `new_delay = current_delay + adj`
6. Repeat until error < 5 cm

The division by 2 accounts for both TX and RX antenna delays contributing equally. The adjustment must be applied to **both** devices for the full correction.

### Key Formula

```
new_delay = current_delay + (measured - actual) / (2 * DISTANCE_OF_RADIO)

DISTANCE_OF_RADIO = 0.004692 m/tick (DW1000 time unit to meters)
```

**Sign convention (empirically verified):**
- Measuring TOO SHORT → DECREASE antenna delay
- Measuring TOO LONG → INCREASE antenna delay

This is because decreasing the delay effectively adds time to the measured ToF, increasing the computed distance.

## Calibration Results

### Round 0: Initial Measurement (delay 16436)

Both devices at default antenna delay 16436.

| Metric | Value |
| --- | --- |
| Known distance | 0.610 m (24") |
| Mean measured | 0.3121 m |
| StdDev | ±0.0401 m (±4.0 cm) |
| Min | 0.229 m |
| Max | 0.558 m |
| Error | -0.298 m (-29.7 cm) |
| Samples | 200/200 |
| Timeouts | 65 |

Computed adjustment: -31 ticks per device → new delay: **16405**

### Round 1: Tag-Only Adjustment (tag=16405, anchor=16436)

Only the tag was adjusted (anchor firmware unchanged). Expected half-correction.

| Metric | Value |
| --- | --- |
| Anchor mean | ~0.44 m |
| Expected (half correction) | ~0.44 m |
| Improvement | +0.13 m (from 0.31 to 0.44) |

This confirms:
- Changing ONE device by 31 ticks shifts measurement by ~0.13m
- 31 ticks × DISTANCE_OF_RADIO = 0.145m per device (close match)
- Full correction requires BOTH devices at 16405

### Round 2: Verification (both at 16405)

Both devices updated to antenna delay 16405. 60-second capture.

| Metric | Value |
| --- | --- |
| Known distance | 0.610 m (24") |
| Mean measured | 0.656 m |
| StdDev | ±0.044 m (±4.4 cm) |
| Min | 0.52 m |
| Max | 0.99 m |
| Error | +0.046 m (+4.6 cm) |
| Samples | 564/565 (1 startup glitch excluded) |
| Range rate | ~9.4 Hz (565 ranges / 60s) |
| Timeouts | ~91 (anchor resets) |
| RX power | -59 to -67 dBm typical |

**Result: +4.6 cm residual error — well within ±10-20 cm target accuracy.**

The small positive bias is consistent with the ±2 inch (±5 cm) measurement uncertainty in the known distance. Outliers at 0.80-0.99m correlate with low RX power (-80 to -93 dBm), indicating multipath or momentary signal degradation. One startup glitch (R#2 = -65.90m) was excluded.

### Calibration Outcome

| Parameter | Before | After |
| --- | --- | --- |
| Antenna delay | 16436 | **16405** |
| Mean measured (at 24") | 0.312 m | 0.656 m |
| Error | -29.7 cm | **+4.6 cm** |
| Within target? | No | **Yes (±10-20 cm)** |

## Technical Notes

### Why Both Devices Must Be Updated

In asymmetric TWR, timestamps from both devices contribute to the ToF calculation:

```
ToF = (Round1 × Round2 - Reply1 × Reply2) / (Round1 + Round2 + Reply1 + Reply2)
```

Although antenna delay mostly cancels within each device's Round/Reply calculation (since TX and RX delays are equal), the delayed transmit in the RANGE message and the cross-device timestamp exchange create a net dependence on the antenna delay setting. Empirically, each device contributes approximately equally to the total error.

### Calibration Firmware Memory Usage

The calibration tag firmware stores 200 float samples (800 bytes), using 54.6% of the Arduino Uno's 2KB RAM. This is near the limit — increasing sample count beyond ~250 would risk stack overflow.

### Measurement Uncertainty

The ±2 inch (±5 cm) uncertainty in the known distance limits calibration precision. For production-quality calibration, a laser-measured distance with ±1mm accuracy would be ideal.

## Files

- `tests/test_calibration_tag.cpp` — Calibration tag firmware (auto-iterating)
- `tests/test_twr_anchor.cpp` — Standard anchor (used during calibration)
- `tests/test_twr_tag.cpp` — Standard tag (updated with calibrated delay)

## Reference

- [ANTENNA_DELAY_CALIBRATION_2026.md](ANTENNA_DELAY_CALIBRATION_2026.md) — General calibration theory and procedures
- [RX_DIAGNOSTIC_SESSION_2026-02-27.md](RX_DIAGNOSTIC_SESSION_2026-02-27.md) — J1 fix and TWR results from same session
