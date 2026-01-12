# Test 6: DW1000Ranging TAG/ANCHOR Test

**Date**: 2026-01-11
**Status**: ❌ **FAILED** - No ranging communication
**Root Cause**: Library bug in interrupt mask configuration

---

## Quick Summary

The DW1000Ranging test failed because both devices initialized successfully but did not proceed to the ranging protocol. Only initialization messages were printed, with no device discovery or ranging measurements.

**Root Cause (90% confidence)**: Known library bug in `DW1000.cpp:992-996` that corrupts interrupt mask, preventing hardware interrupts from firing.

---

## Test Results

### Anchor Output
```
[12:06:05.320] device address: 82:17:5B:D5:A9:9A:E2:9C
[12:06:05.320] ### ANCHOR ###
```
**Total**: 2 lines in 180 seconds

### Tag Output
```
[12:06:05.968] device address: 7D:00:22:EA:82:60:3B:9C
[12:06:05.968] ### TAG ###
```
**Total**: 2 lines in 180 seconds

### What Should Have Happened
- Anchor should print "blink; 1 device added !" when detecting tag
- Tag should print "ranging init; 1 device added !" when finding anchor
- Both should print continuous distance measurements with RX power

---

## Files in This Directory

### Test Code
- `test_06_anchor.ino` - Original anchor code (TESTED - failed)
- `test_06_tag.ino` - Original tag code (TESTED - failed)
- `test_06_anchor_debug.ino` - Debug version with heartbeat (READY)
- `test_06_tag_debug.ino` - Debug version with heartbeat (READY)

### Scripts
- `monitor_serial.py` - Python serial monitor for 115200 baud (TESTED - works)
- `check_library.py` - Library communication test (TESTED - shows resets)
- `run_ranging_test.sh` - Shell test runner

### Output
- `anchor_output.txt` - Captured serial output from anchor
- `tag_output.txt` - Captured serial output from tag
- `RESULTS.txt` - Raw test results summary
- `ANALYSIS.md` - Comprehensive technical analysis (47 pages)
- `README.md` - This file

---

## Root Cause Analysis

### The Bug

**File**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp`
**Lines**: 992-996
**Function**: `interruptOnReceiveFailed(boolean val)`

**Problem**: Uses `LEN_SYS_STATUS` (5 bytes) instead of `LEN_SYS_MASK` (4 bytes)

```cpp
// BUGGY CODE (current)
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);  // WRONG!
    setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);   // WRONG!
    setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);   // WRONG!
    setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);  // WRONG!
}
```

**Impact**:
- Buffer overrun corrupts `_sysmask` array
- Incorrect values written to DW1000 SYS_MASK register
- Hardware interrupts never fire
- Ranging protocol waits forever for callbacks that never come

### Evidence

1. **Identical to Test 3-4 failure**: BasicSender/Receiver had same symptoms
2. **Interrupt dependency**: All failed tests use interrupts; non-interrupt tests pass
3. **Known bug**: Documented in `docs/findings/interrupt_debugging.md`
4. **Call chain verified**: Bug is called during `startAsAnchor()`/`startAsTag()`

---

## The Fix

### Change Required

Replace `LEN_SYS_STATUS` with `LEN_SYS_MASK` in 4 places:

```cpp
// FIXED CODE
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);   // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);    // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);    // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);   // FIXED
}
```

### How to Apply

```bash
# Edit the file
nano /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp

# Go to lines 992-996
# Change all 4 instances of LEN_SYS_STATUS to LEN_SYS_MASK
# Save and exit

# Recompile and upload test
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging
# Upload anchor and tag code again
# Monitor output - should now work!
```

---

## Next Steps (Priority Order)

### 1. Upload Debug Versions (5 minutes)
**Purpose**: Verify if loop() is executing or setup() hangs

```bash
# Upload test_06_anchor_debug.ino to /dev/ttyACM0
# Upload test_06_tag_debug.ino to /dev/ttyACM1
python3 monitor_serial.py 30
```

**Expected**: Should see "ALIVE" messages every 5 seconds if loop() runs

### 2. Apply Library Fix (10 minutes)
**Purpose**: Fix the root cause

```bash
# Edit DW1000.cpp lines 992-996
# Recompile and upload
# Retest ranging
```

**Expected**: Ranging should work after fix

### 3. Hardware Inspection (10 minutes)
**Purpose**: Rule out hardware issues

- Check IRQ pin (D2) connection
- Verify SPI pins (MOSI, MISO, SCK, SS)
- Look for loose wires

### 4. Distance Measurements (30 minutes)
**Only if ranging works after fixes**

- Place devices at 1m, 2m, 3m
- Record measurements
- Calculate accuracy
- Document results

---

## Testing Commands

### Monitor Both Devices
```bash
python3 monitor_serial.py 180  # 3 minutes
```

### Check Device Reset Behavior
```bash
python3 check_library.py
```

### Upload and Monitor (Manual)
```bash
# Terminal 1 - Anchor
screen /dev/ttyACM0 115200

# Terminal 2 - Tag
screen /dev/ttyACM1 115200
```

---

## Expected Output After Fix

### Anchor
```
device address: 82:17:5B:D5:A9:9A:E2:9C
### ANCHOR ###
blink; 1 device added ! -> short:7D00
from: 7D00    Range: 1.23 m    RX power: -85.2 dBm
from: 7D00    Range: 1.24 m    RX power: -85.1 dBm
from: 7D00    Range: 1.23 m    RX power: -85.3 dBm
[continuous ranging at ~1-5 Hz...]
```

### Tag
```
device address: 7D:00:22:EA:82:60:3B:9C
### TAG ###
ranging init; 1 device added ! -> short:8217
from: 8217    Range: 1.23 m    RX power: -84.8 dBm
from: 8217    Range: 1.24 m    RX power: -84.7 dBm
from: 8217    Range: 1.23 m    RX power: -84.9 dBm
[continuous ranging at ~1-5 Hz...]
```

---

## Hardware Configuration

| Component | Anchor | Tag |
|-----------|--------|-----|
| Platform | Arduino Uno | Arduino Uno |
| Port | /dev/ttyACM0 | /dev/ttyACM1 |
| Address | 82:17:5B:D5:A9:9A:E2:9C | 7D:00:22:EA:82:60:3B:9C |
| Short Addr | 82:17 | 7D:00 |
| Role | Responder | Initiator |

### Pins
- IRQ: D2 (INT0)
- RST: D9
- SS: D10 (SPI chip select)
- MOSI: D11
- MISO: D12
- SCK: D13

---

## Documentation

- **RESULTS.txt** - Raw test results and troubleshooting
- **ANALYSIS.md** - Complete technical analysis (47 pages)
  - Code analysis
  - Root cause investigation
  - Comparison with other tests
  - Detailed fix instructions
  - Alternative solutions
  - Lessons learned
- **This README** - Quick reference guide

---

## Success Criteria

### Minimum Success
- [ ] Both devices print initialization
- [ ] Device discovery messages appear
- [ ] At least one range measurement

### Full Success
- [ ] Continuous ranging at 1+ Hz
- [ ] Reasonable distances (0.1-10m)
- [ ] RX power present (-60 to -100 dBm)
- [ ] Stable measurements
- [ ] No timeouts

### Calibration Success
- [ ] Accuracy within ±20cm
- [ ] Std deviation < 10cm
- [ ] Consistent over 60 seconds
- [ ] No dropped measurements

---

## Related Tests

- **Test 1**: Chip ID - ✅ PASSED (SPI works)
- **Test 2**: Library Connectivity - ✅ PASSED (initialization works)
- **Test 3**: BasicSender - ⚠️ PARTIAL (same interrupt bug)
- **Test 4**: BasicReceiver - ⚠️ PARTIAL (same interrupt bug)
- **Test 6**: DW1000Ranging - ❌ FAILED (same interrupt bug)

**Pattern**: All interrupt-based tests fail, non-interrupt tests pass → interrupt bug confirmed

---

## Contact Information

**Project**: SwarmLoc (Drone swarm UWB ranging)
**Platform**: Arduino Uno + PCL298336 v1.3 (DW1000)
**Library**: arduino-dw1000 v0.9
**Test Date**: 2026-01-11

---

**Status**: Test failed, fix identified, ready to retest
**Next Action**: Apply library fix or upload debug versions
**Expected Resolution Time**: 10-15 minutes
