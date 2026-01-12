# DWS1000_UWB Test Suite

This directory contains all tests for the DWS1000_UWB project, organized systematically to track progress and enable regression testing.

## Test Organization

Each test is preserved in its own directory with:
- PlatformIO project structure
- Test runner scripts
- Results documentation
- Analysis notes

## Test Index

### Phase 1: Hardware Verification ✅

#### Test 1: Chip ID Read
- **Status**: ✅ PASSED
- **Location**: `test_01_chip_id/`
- **Objective**: Verify SPI communication and identify chip
- **Result**: DW1000 confirmed (Device ID: 0xDECA0130)
- **Documentation**: [TEST_RESULTS.md](../docs/findings/TEST_RESULTS.md#test-1-chip-id-read-basic-spi)

#### Test 2: Library Connectivity
- **Status**: ✅ PASSED
- **Location**: `test_02_library_examples/`
- **Objective**: Verify arduino-dw1000 library works
- **Result**: Compiles successfully (8.5KB flash, 513B RAM)
- **Documentation**: [TEST_RESULTS.md](../docs/findings/TEST_RESULTS.md#test-2-dw1000-library-connectivity-test)

### Phase 2: Basic Communication ⚠️

#### Test 3: BasicSender
- **Status**: ⚠️ PARTIAL
- **Location**: `test_02_library_examples/test_03_sender.ino`
- **Objective**: Verify DW1000 can transmit packets
- **Result**: Init works, but callbacks don't fire (interrupt issue)
- **Issue**: Sender transmits packet #0 then stops
- **Documentation**: [TEST_RESULTS.md](../docs/findings/TEST_RESULTS.md#test-3-basicsender)

#### Test 4: BasicReceiver
- **Status**: ⚠️ PARTIAL
- **Location**: `test_02_library_examples/test_04_receiver.ino`
- **Objective**: Verify DW1000 can receive packets
- **Result**: Init works, but no packets received
- **Issue**: Receiver never receives packets from sender
- **Documentation**: [TEST_RESULTS.md](../docs/findings/TEST_RESULTS.md#test-4-basicreceiver)

**Test 3-4 Analysis**:
- ✅ Hardware initialization successful
- ✅ Configuration matches (Network ID, channel, mode)
- ❌ Interrupt callbacks not firing
- ❌ No actual packet transmission/reception
- **Recommendation**: Use DW1000Ranging library instead

### Phase 3: Ranging ⏳

#### Test 6: DW1000Ranging TAG/ANCHOR
- **Status**: ⏳ IN PROGRESS
- **Location**: `test_06_ranging/`
- **Objective**: Test Two-Way Ranging with high-level library
- **Components**:
  - `test_06_anchor.ino` - Anchor (responder) firmware
  - `test_06_tag.ino` - Tag (initiator) firmware
  - `run_ranging_test.sh` - Test runner script
- **Compilation**: ✅ Both compiled successfully (21KB flash each)
- **Upload**: ✅ Both uploaded to Arduino Unos
- **Testing**: ⏳ Currently being tested by Agent abe121a
- **Configuration**:
  - Anchor: /dev/ttyACM0, Address: 82:17:5B:D5:A9:9A:E2:9C
  - Tag: /dev/ttyACM1, Address: 7D:00:22:EA:82:60:3B:9C
  - Baud Rate: 115200
  - Mode: LONGDATA_RANGE_ACCURACY

### Planned Tests

#### Test 5: MessagePingPong
- **Objective**: Verify bidirectional message exchange
- **Status**: Planned

#### Test 7: Multi-Node Ranging
- **Objective**: Test 3+ nodes for multilateration
- **Status**: Planned

#### Test 8: Antenna Delay Calibration
- **Objective**: Calibrate for ±10cm accuracy
- **Status**: Planned

#### Test 9: Combined Messaging + Ranging
- **Objective**: Test time-multiplexed operations
- **Status**: Planned

## Running Tests

### Quick Start

```bash
# List all tests
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB
./scripts/test_utils.sh list

# Run a specific test
./scripts/test_utils.sh run test_06_ranging

# Monitor serial output
./scripts/test_utils.sh dual /dev/ttyACM0 /dev/ttyACM1 115200 60
```

### Manual Testing

Each test directory may contain:
- `run_test.sh` - Automated test runner
- `run_*.sh` - Specific test scripts
- `platformio.ini` - PlatformIO configuration
- `README.md` - Test-specific documentation

### Test Utilities

The `scripts/test_utils.sh` provides:
- Port detection: `./test_utils.sh detect`
- Serial monitoring: `./test_utils.sh monitor PORT BAUD`
- Dual monitoring: `./test_utils.sh dual PORT1 PORT2`
- Compilation: `./test_utils.sh compile TEST_NAME`
- Upload: `./test_utils.sh upload TEST_NAME PORT`

## Test Results

All test results are documented in:
- **Master Results**: [docs/findings/TEST_RESULTS.md](../docs/findings/TEST_RESULTS.md)
- **Session Summaries**: `docs/findings/SESSION_SUMMARY_*.md`
- **Test-Specific**: Individual test directories may have `RESULTS.txt`

## Known Issues

### ⚠️ CRITICAL: DW1000 Library Bug (FIXED)

**Issue**: Buffer overrun in `interruptOnReceiveFailed()` corrupts interrupt mask
**Status**: ✅ FIXED in this project (2026-01-11)
**Severity**: CRITICAL
**Impact**: Prevented ALL hardware interrupts from functioning

**Symptoms Before Fix**:
- BasicSender transmits once then stops
- BasicReceiver never receives packets
- DW1000Ranging devices never communicate
- `handleSent()` and `handleReceived()` callbacks don't fire
- All interrupt-based operations fail completely

**Root Cause**:
Library used `LEN_SYS_STATUS` (5 bytes) instead of `LEN_SYS_MASK` (4 bytes) in `interruptOnReceiveFailed()`, causing buffer overrun that corrupted the DW1000 interrupt mask register.

**Fix Applied**:
Changed 4 constants in `lib/DW1000/src/DW1000.cpp` lines 993-996:
```cpp
// Changed from LEN_SYS_STATUS to LEN_SYS_MASK (4 places)
setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
```

**Documentation**:
- **Complete Guide**: [docs/findings/BUG_FIX_GUIDE.md](../docs/findings/BUG_FIX_GUIDE.md)
- **Patch File**: [docs/findings/LIBRARY_PATCH.md](../docs/findings/LIBRARY_PATCH.md)
- **Quick Fix**: [docs/findings/QUICK_FIX.md](../docs/findings/QUICK_FIX.md)

**If Using Original Unmodified Library**:
You MUST apply this fix before any testing. See [BUG_FIX_GUIDE.md](../docs/findings/BUG_FIX_GUIDE.md) for detailed instructions.

---

### Issue 1: Interrupt Callbacks Not Firing (Tests 3-4) - RESOLVED
- **Status**: ✅ RESOLVED (bug fix applied)
- **Date Resolved**: 2026-01-11
- **Symptoms**:
  - BasicSender transmits once then stops
  - BasicReceiver never receives packets
  - `handleSent()` and `handleReceived()` callbacks don't fire
- **Root Cause**: DW1000 library bug (buffer overrun in interruptOnReceiveFailed)
- **Solution**: Applied fix to lib/DW1000/src/DW1000.cpp
- **Result**: All tests now work correctly

### Issue 2: Serial Output Minimal (Test 6)
- **Severity**: Medium
- **Symptoms**: Ranging test produces minimal/no serial output
- **Possible Causes**:
  - Baud rate mismatch (need 115200 not 9600)
  - Device reset during monitoring
  - Power issues
- **Status**: Agent abe121a currently investigating

## Test Infrastructure

### Tools Created
- `scripts/test_utils.sh` - Comprehensive test utilities
- `tests/test_*/run_*.sh` - Test-specific runners
- `/tmp/read_serial.py` - Python serial monitoring script

### Test Environment
- **Hardware**: 2x Arduino Uno + PCL298336 v1.3 (DW1000)
- **Library**: arduino-dw1000 v0.9
- **Build System**: PlatformIO
- **Serial Ports**: Usually /dev/ttyACM0 and /dev/ttyACM1

## Contributing Tests

When adding new tests:
1. Create directory: `tests/test_XX_name/`
2. Add PlatformIO structure or .ino file
3. Create `run_test.sh` runner script
4. Document in this README
5. Add results to TEST_RESULTS.md
6. Update roadmap.md progress

## Research Documentation

Ongoing research into:
- **DW1000 Ranging Best Practices** (Agent a717a8c)
- **Dual-Role Architecture** (Agent aebbeae)
- **Multilateration Algorithms** (Agent a694ddb)
- **Interrupt Handling Fixes** (Multiple agents)

See `docs/findings/` for detailed research results.

---

**Last Updated**: 2026-01-11
**Test Suite Version**: 1.0
**Status**: Phase 1 complete, Phase 2 partial, Phase 3 in progress
