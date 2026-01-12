## DW1000 Post-Bug-Fix Testing Documentation

## Overview

This directory contains comprehensive testing scripts to verify that the critical bug fix in `DW1000.cpp` resolves the interrupt handling issue that prevented all DW1000 communication.

### Bug Fixed

**File**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp`
**Function**: `interruptOnReceiveFailed()`
**Lines**: 992-996
**Issue**: Buffer overrun in interrupt mask register
**Fix**: Changed `LEN_SYS_STATUS` (5 bytes) to `LEN_SYS_MASK` (4 bytes)

```cpp
// BEFORE (BUGGY):
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);  // WRONG
    setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);   // WRONG
    setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);   // WRONG
    setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);  // WRONG
}

// AFTER (FIXED):
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);   // CORRECT
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);    // CORRECT
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);    // CORRECT
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);   // CORRECT
}
```

**Impact**: This bug caused all hardware interrupts on the DW1000 to fail, preventing:
- TX completion callbacks (`handleSent()`)
- RX callbacks (`handleReceived()`)
- Ranging protocol operation
- All interrupt-driven communication

---

## Quick Start

### Option 1: Run All Tests (Recommended)

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests
./RUN_ALL_TESTS.sh
```

This will:
1. Run Test 3 (BasicSender) - 60s
2. Run Test 4 (Sender+Receiver) - 60s
3. Run Test 6 (Ranging) - 120s (MOST CRITICAL)
4. Generate comprehensive report with analysis

**Total time**: ~6 minutes
**Output**: `test_outputs/MASTER_REPORT_YYYYMMDD_HHMMSS.md`

### Option 2: Run Individual Tests

```bash
# Test 3: BasicSender only (verify TX interrupts work)
./run_test_03_sender_only.sh /dev/ttyACM0 60

# Test 4: Sender + Receiver (verify RX interrupts work)
./run_test_04_tx_rx.sh /dev/ttyACM0 /dev/ttyACM1 60

# Test 6: Ranging (verify ranging protocol works) - MOST IMPORTANT
./run_test_06_ranging.sh /dev/ttyACM0 /dev/ttyACM1 120
```

### Option 3: Manual Testing (Full Control)

See: `TESTING_GUIDE_POST_BUG_FIX.md` for detailed manual testing procedures.

---

## Test Suite

| Test | File | Duration | Purpose | Critical? |
|------|------|----------|---------|-----------|
| **Test 3** | `run_test_03_sender_only.sh` | 60s | Verify TX interrupts | Medium |
| **Test 4** | `run_test_04_tx_rx.sh` | 60s | Verify RX interrupts | High |
| **Test 6** | `run_test_06_ranging.sh` | 120s | Verify ranging protocol | **CRITICAL** |

### Test 3: BasicSender

**Verifies**:
- DW1000 can transmit packets
- `handleSent()` callback fires
- TX interrupts working

**Success Indicator**: "Transmitted successfully" messages appear continuously

**Before bug fix**: Only packet #0 transmitted, then hangs
**After bug fix**: Continuous packet transmission

### Test 4: BasicSender + BasicReceiver

**Verifies**:
- TX device transmits
- RX device receives
- `handleReceived()` callback fires
- Bidirectional interrupt handling

**Success Indicator**: Receiver shows "Received message" continuously

**Before bug fix**: No packets received
**After bug fix**: Continuous packet reception

### Test 6: DW1000Ranging (CRITICAL)

**Verifies**:
- Two-way ranging protocol
- Device discovery
- Range measurements
- Complete interrupt chain
- DW1000Ranging library functionality

**Success Indicator**: "Device found" and continuous "Range: X.XX m" messages

**Before bug fix**:
```
TAG ready
ANCHOR ready
[No further output - devices waiting for interrupts that never fire]
```

**After bug fix**:
```
TAG ready
Device found: 8217
Range: 1.02 m (102 cm) from 8217
Range: 0.98 m (98 cm) from 8217
[Continuous ranging at 1-5 Hz]
```

**This is the definitive test** - if this works, the bug fix is confirmed successful.

---

## Expected Results

### Before Bug Fix (Baseline)

All tests would show:
- ✅ Device initialization successful
- ✅ Configuration committed
- ❌ No callbacks firing
- ❌ No continuous operation
- ❌ Devices appear to hang

**Root cause**: Interrupt mask register corrupted by buffer overrun

### After Bug Fix (Expected)

All tests should show:
- ✅ Device initialization successful
- ✅ Configuration committed
- ✅ Callbacks firing continuously
- ✅ Normal continuous operation
- ✅ All communication features working

**Resolution**: Interrupt mask register correctly configured

---

## Success Criteria

### Minimum Success (Bug Fix Works)
- Test 3: ≥10 "Transmitted successfully" messages in 60s
- Test 4: ≥10 packets received in 60s
- Test 6: ≥20 range measurements in 120s
- **All interrupt callbacks must fire**

### Full Success (Production Ready)
- Test 3: ~60 transmissions (1 Hz continuous)
- Test 4: >90% packet reception rate
- Test 6: 120-600 range measurements (1-5 Hz)
- Stable, no gaps > 5 seconds

---

## Output Files

All test outputs are saved to: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_outputs/`

**File naming convention**:
- `test03_sender_YYYYMMDD_HHMMSS.txt` - Test 3 sender output
- `test04_sender_YYYYMMDD_HHMMSS.txt` - Test 4 sender output
- `test04_receiver_YYYYMMDD_HHMMSS.txt` - Test 4 receiver output
- `test06_tag_YYYYMMDD_HHMMSS.txt` - Test 6 TAG output
- `test06_anchor_YYYYMMDD_HHMMSS.txt` - Test 6 ANCHOR output
- `MASTER_REPORT_YYYYMMDD_HHMMSS.md` - Comprehensive test report

---

## Troubleshooting

### "No such file or directory: /dev/ttyACM0"

```bash
# Find your Arduino ports
ls /dev/ttyACM* /dev/ttyUSB*

# Then specify correct ports
./run_test_04_tx_rx.sh /dev/ttyUSB0 /dev/ttyUSB1
```

### "Permission denied" on serial port

```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Log out and back in, or:
sudo chmod 666 /dev/ttyACM0 /dev/ttyACM1
```

### Tests still failing after bug fix

1. **Verify bug fix was applied**:
   ```bash
   grep -A 4 "interruptOnReceiveFailed" /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp
   ```
   Should show `LEN_SYS_MASK`, NOT `LEN_SYS_STATUS`

2. **Force library recompilation**:
   ```bash
   rm -rf /tmp/dw1000_test*
   ```
   Then re-run tests

3. **Check hardware**:
   - Verify IRQ pin connected (Arduino Pin 2)
   - Check SPI connections (MOSI, MISO, SCK, SS)
   - Ensure both devices powered properly

4. **Check library**:
   ```bash
   ls -la /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/
   ```

### Test hangs or never completes

- Press Ctrl+C to abort
- Check Arduino is responding: `pio device list`
- Try resetting Arduino manually
- Disconnect and reconnect USB

---

## Interpreting Results

### Test 3 Output Analysis

**Good** (Bug fix working):
```
[12:00:00] Transmitting packet ... #0
[12:00:01] Transmitted successfully
[12:00:02] Transmitting packet ... #1
[12:00:03] Transmitted successfully
...
```

**Bad** (Bug still present):
```
[12:00:00] Transmitting packet ... #0
[No further output]
```

### Test 6 Output Analysis

**Excellent** (Bug fix fully successful):
```
[12:00:00] TAG ready
[12:00:02] Device found: 8217
[12:00:03] Range: 1.02 m (102 cm) from 8217
[12:00:04] Range: 0.99 m (99 cm) from 8217
[12:00:05] Range: 1.01 m (101 cm) from 8217
[Continues continuously for 120s]
```

**Bad** (Bug still present):
```
[12:00:00] TAG ready
[12:00:00] ANCHOR ready
[No further output for 120s]
```

---

## Next Steps After Testing

### If Tests Pass (Bug Fix Successful)

1. **Update TEST_RESULTS.md** with successful results
2. **Document the fix** in project documentation
3. **Proceed with project**:
   - Implement multi-anchor ranging
   - Add trilateration/multilateration
   - Calibrate antenna delays
   - Integration with swarm system

### If Tests Fail (Bug Not Fixed)

1. **Verify fix was applied correctly**:
   - Check DW1000.cpp lines 992-996
   - Confirm LEN_SYS_MASK is used

2. **Check compilation**:
   - Ensure library was recompiled
   - Clear build cache

3. **Hardware verification**:
   - Test IRQ pin with multimeter/oscilloscope
   - Verify all SPI connections

4. **Alternative debugging**:
   - Try polling mode instead of interrupts
   - Test with different library version
   - Review library issue tracker

---

## Documentation References

- **Bug Analysis**: `../docs/findings/INTERRUPT_ISSUE_SUMMARY.md`
- **Detailed Testing Guide**: `TESTING_GUIDE_POST_BUG_FIX.md`
- **Historical Results**: `../docs/findings/TEST_RESULTS.md`
- **DW1000 Best Practices**: `../docs/findings/DW1000_RANGING_BEST_PRACTICES.md`

---

## Quick Reference Commands

```bash
# Check available Arduino ports
ls /dev/ttyACM* /dev/ttyUSB*

# Run all tests (recommended)
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests
./RUN_ALL_TESTS.sh

# Run individual test
./run_test_06_ranging.sh /dev/ttyACM0 /dev/ttyACM1 120

# View latest report
cat test_outputs/MASTER_REPORT_*.md | tail -1

# View latest ranging output
cat test_outputs/test06_anchor_*.txt | tail -1

# List all test outputs
ls -lht test_outputs/

# Verify bug fix applied
grep -n "LEN_SYS_MASK" ../lib/DW1000/src/DW1000.cpp | grep -A 3 "interruptOnReceiveFailed"
```

---

## Test Execution Log

Keep a log of your test runs:

```markdown
### Test Run YYYY-MM-DD HH:MM

**Environment**:
- Devices: 2x Arduino Uno + PCL298336
- Ports: /dev/ttyACM0, /dev/ttyACM1
- Bug fix: Applied ✓

**Results**:
- Test 3: [PASS/FAIL] - [X transmissions]
- Test 4: [PASS/FAIL] - [X/Y packets, Z% success]
- Test 6: [PASS/FAIL] - [X ranges in 120s]

**Observations**:
[Notes]

**Conclusion**:
[Overall assessment]
```

---

**Last Updated**: 2026-01-11
**Created By**: Claude Code Testing Framework v1.0
