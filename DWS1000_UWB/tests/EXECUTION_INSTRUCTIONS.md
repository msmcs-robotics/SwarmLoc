# Testing Execution Instructions - Post Bug Fix

## Quick Reference Card

### Bug Fixed
- **File**: `lib/DW1000/src/DW1000.cpp`
- **Lines**: 992-996
- **Change**: `LEN_SYS_STATUS` → `LEN_SYS_MASK`
- **Impact**: Fixes ALL interrupt handling

### Run All Tests (One Command)
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests
./RUN_ALL_TESTS.sh
```
**Duration**: ~6 minutes | **Output**: `test_outputs/MASTER_REPORT_*.md`

---

## Individual Test Commands

### Test 3: BasicSender (60 seconds)
```bash
./run_test_03_sender_only.sh /dev/ttyACM0 60
```
**Verifies**: TX interrupts working

### Test 4: Sender + Receiver (60 seconds)
```bash
./run_test_04_tx_rx.sh /dev/ttyACM0 /dev/ttyACM1 60
```
**Verifies**: RX interrupts working

### Test 6: Ranging (120 seconds) ★ MOST CRITICAL ★
```bash
./run_test_06_ranging.sh /dev/ttyACM0 /dev/ttyACM1 120
```
**Verifies**: Complete ranging protocol (THE definitive test)

---

## What to Look For

### Success (Bug Fix Works)
- **Test 3**: "Transmitted successfully" messages appear continuously
- **Test 4**: "Received message" appears on receiver
- **Test 6**: "Device found" AND "Range: X.XX m" messages appear

### Failure (Bug Still Present)
- **Test 3**: Hangs after "Transmitting packet #0"
- **Test 4**: Receiver shows no received packets
- **Test 6**: Only shows "TAG ready" / "ANCHOR ready", then nothing

---

## Minimum Success Criteria

| Test | Metric | Threshold | Meaning |
|------|--------|-----------|---------|
| Test 3 | TX events | ≥10 | TX interrupts work |
| Test 4 | RX events | ≥10 | RX interrupts work |
| Test 6 | Ranges | ≥20 | **Bug fix successful!** |

**If Test 6 ≥20 ranges: Project unblocked, all features working**

---

## File Locations

**Test Scripts**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/`
- `RUN_ALL_TESTS.sh` (master runner)
- `run_test_03_sender_only.sh`
- `run_test_04_tx_rx.sh`
- `run_test_06_ranging.sh`

**Output Files**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_outputs/`
- `test03_sender_*.txt`
- `test04_sender_*.txt`, `test04_receiver_*.txt`
- `test06_tag_*.txt`, `test06_anchor_*.txt`
- `MASTER_REPORT_*.md` (comprehensive analysis)

**Documentation**:
- `TESTING_GUIDE_POST_BUG_FIX.md` (detailed manual procedures)
- `README_POST_BUG_FIX_TESTING.md` (quick reference)
- `TESTING_SUITE_SUMMARY.md` (framework overview)
- `EXECUTION_INSTRUCTIONS.md` (this file - quick start)

---

## Troubleshooting

### Port not found
```bash
ls /dev/ttyACM* /dev/ttyUSB*  # Find your ports
./run_test_06_ranging.sh /dev/ttyUSB0 /dev/ttyUSB1  # Use actual ports
```

### Permission denied
```bash
sudo chmod 666 /dev/ttyACM0 /dev/ttyACM1
```

### Verify bug fix applied
```bash
grep -A 4 "interruptOnReceiveFailed" ../lib/DW1000/src/DW1000.cpp
# Should show LEN_SYS_MASK (not LEN_SYS_STATUS)
```

---

## Expected Runtime

- **Test 3**: 60 seconds
- **Test 4**: 60 seconds
- **Test 6**: 120 seconds
- **Total (all tests)**: ~6 minutes

---

## After Testing

### View Results
```bash
cd test_outputs
cat MASTER_REPORT_*.md | less  # View comprehensive report
cat test06_anchor_*.txt | grep "Range:"  # See ranging measurements
```

### If Tests Pass
1. Review `MASTER_REPORT_*.md`
2. Update project documentation
3. Proceed with multi-anchor ranging
4. Implement trilateration

### If Tests Fail
1. Review test output files
2. Verify bug fix: `grep LEN_SYS_MASK ../lib/DW1000/src/DW1000.cpp`
3. Check hardware: IRQ pin on Arduino Pin 2
4. Re-run with: `rm -rf /tmp/dw1000_test*` first

---

**Last Updated**: 2026-01-11
