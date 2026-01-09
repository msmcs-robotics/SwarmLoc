# DWM3000 Incremental Testing Plan

## Philosophy: Build and Test Feature by Feature

Each test builds on the previous one. We verify each feature works before moving to the next.

---

## Test 1: Basic SPI Communication - Read Chip ID

**Goal**: Verify we can communicate with DWM3000 chip via SPI

**What this tests**:
- SPI wiring is correct
- Power is connected
- Arduino can send/receive SPI data
- DWM3000 chip is responsive

**Success Criteria**:
- Read device ID register (address 0x00)
- Expected value: `0xDECA0302` (DW3110 chip)
- Serial output shows chip ID correctly

**Code Location**: `tests/test_01_chip_id/`

**Hardware Needed**:
- 1x Arduino Uno with DWM3000 shield
- USB cable

**Expected Output**:
```
=== Test 1: Chip ID Read ===
Initializing SPI...
Reading Device ID...
Device ID: 0xDECA0302
✓ SUCCESS: DWM3000 detected!
```

**If Failed**: Check wiring, power, SPI connections

---

## Test 2: GPIO and Hardware Reset

**Goal**: Verify GPIO control and hardware reset functionality

**What this tests**:
- RST pin control works
- Can perform hardware reset
- Chip recovers from reset
- IRQ pin can be read

**Success Criteria**:
- Can toggle RST pin
- Chip responds after reset
- IRQ pin state can be read
- Chip ID still readable after reset

**Code Location**: `tests/test_02_gpio_reset/`

**Expected Output**:
```
=== Test 2: GPIO & Reset ===
Testing RST pin control...
  RST LOW -> HIGH -> LOW -> HIGH
Performing hardware reset...
  Reset complete
Reading chip ID after reset...
  Device ID: 0xDECA0302
Testing IRQ pin read...
  IRQ pin state: LOW
✓ SUCCESS: GPIO and reset working
```

---

## Test 3: Simple Transmit (Initiator Only)

**Goal**: Transmit a simple message from initiator

**What this tests**:
- TX configuration works
- Can queue message for transmission
- TX completes successfully
- TX timestamp can be captured

**Success Criteria**:
- Message transmits without error
- TX done interrupt fires
- TX timestamp is non-zero
- No SPI errors

**Code Location**: `tests/test_03_simple_tx/`

**Hardware Needed**:
- Initiator Arduino only

**Expected Output**:
```
=== Test 3: Simple TX ===
Configuring for TX...
Sending test message: "HELLO"
TX started...
Waiting for TX done...
TX complete!
TX Timestamp: 0x00123456789A
✓ SUCCESS: Transmit working
```

---

## Test 4: Simple Receive (Responder Only)

**Goal**: Receive a message on responder

**What this tests**:
- RX configuration works
- Can enter receive mode
- RX done interrupt fires
- RX timestamp can be captured
- Can read received data

**Success Criteria**:
- Enters RX mode successfully
- Receives message from initiator
- RX timestamp is non-zero
- Received data is correct

**Code Location**: `tests/test_04_simple_rx/`

**Hardware Needed**:
- Both Arduinos
- Initiator must be running Test 3 code

**Expected Output**:
```
=== Test 4: Simple RX ===
Entering RX mode...
Waiting for message...
Message received!
RX Timestamp: 0x00123456800B
Data: "HELLO"
Length: 5 bytes
✓ SUCCESS: Receive working
```

---

## Test 5: Basic Message Exchange (Ping-Pong)

**Goal**: Two-way communication - initiator sends, responder replies

**What this tests**:
- TX/RX mode switching
- Message framing
- Bidirectional communication
- Basic protocol timing

**Success Criteria**:
- Initiator sends PING
- Responder receives PING
- Responder sends PONG
- Initiator receives PONG
- Multiple cycles work

**Code Location**: `tests/test_05_ping_pong/`

**Expected Output**:

**Initiator**:
```
=== Test 5: Ping-Pong (Initiator) ===
Cycle 1:
  Sending PING...
  TX at: 0x001234567890
  Waiting for PONG...
  PONG received at: 0x001234568900
  ✓ Round trip complete
Cycle 2:
  ...
```

**Responder**:
```
=== Test 5: Ping-Pong (Responder) ===
Waiting for PING...
PING received at: 0x001234567895
Sending PONG...
PONG sent at: 0x0012345678F0
Waiting for next PING...
```

---

## Test 6: TX Timestamp Capture

**Goal**: Accurately capture transmit timestamps

**What this tests**:
- Reading TX_TIME register correctly
- Timestamp format is correct
- Timestamps are consistent
- No register read errors

**Success Criteria**:
- TX timestamp is 40-bit value
- Timestamp increments each transmission
- Timestamp is captured immediately after TX
- Values are in valid range

**Code Location**: `tests/test_06_tx_timestamp/`

**Expected Output**:
```
=== Test 6: TX Timestamp Capture ===
Test 1:
  TX complete
  Reading TX timestamp...
  TX_TIME register: 0x001234567890
  Parsed: 305419896 time units
  ✓ Valid 40-bit timestamp

Test 2:
  TX complete
  TX_TIME register: 0x001234580000
  Delta from previous: 10000 units
  ✓ Timestamp incremented

✓ SUCCESS: TX timestamps captured correctly
```

---

## Test 7: RX Timestamp Capture

**Goal**: Accurately capture receive timestamps

**What this tests**:
- Reading RX_TIME register correctly
- RX timestamp captured on receive
- Correlation with TX timestamp
- Timing accuracy

**Success Criteria**:
- RX timestamp captured successfully
- Timestamp difference makes sense (message flight time)
- Consistent across multiple receives

**Code Location**: `tests/test_07_rx_timestamp/`

**Expected Output**:
```
=== Test 7: RX Timestamp Capture ===
Initiator TX at: T1 = 0x001234567890
Responder RX at: T2 = 0x001234567920
Time difference: 48 time units
Calculated flight time: ~0.75 ns (expected for <1m)
✓ SUCCESS: RX timestamps make sense
```

---

## Test 8: Full DS-TWR Protocol

**Goal**: Implement complete Double-Sided Two-Way Ranging

**What this tests**:
- 4-message exchange (POLL, RESP, FINAL, REPORT)
- All 8 timestamps captured correctly
- Protocol state machine
- Timeout handling

**Success Criteria**:
- All 4 messages exchange successfully
- All 8 timestamps captured (T1-T8)
- Protocol completes without errors
- Can run multiple ranging cycles

**Code Location**: `src/initiator/main.cpp` and `src/responder/main.cpp`

**Expected Output**:

**Initiator**:
```
=== DS-TWR Initiator ===
Ranging cycle 1:
  [1] Sending POLL...      T1=0x0012345678A0
  [2] RESPONSE received    T4=0x0012345679B0
  [3] Sending FINAL...     T5=0x001234567AC0
  [4] REPORT received      T8=0x001234567BD0
Timestamps captured: T1, T2, T3, T4, T5, T6, T7, T8
Ready for distance calculation
✓ Protocol complete
```

---

## Test 9: Distance Calculation

**Goal**: Calculate distance from timestamps and calibrate

**What this tests**:
- ToF calculation formula
- Distance conversion
- Accuracy measurement
- Calibration procedures

**Success Criteria**:
- Distance calculation produces reasonable values
- Repeated measurements are consistent
- Known distance test shows systematic offset
- After calibration: accuracy within ±50cm

**Code Location**: `src/initiator/main.cpp` (distance calc function)

**Test Procedure**:
1. Place devices at known distances (1m, 2m, 3m, 5m)
2. Collect 100 measurements at each distance
3. Calculate mean, std dev, error
4. Determine antenna delay correction
5. Apply calibration and re-test

**Expected Output**:
```
=== Distance Calculation Test ===
Actual distance: 1.00 m
Raw measurements (10 samples):
  1.15 m, 1.17 m, 1.14 m, 1.16 m, 1.15 m...
Mean: 1.15 m
Std Dev: 0.02 m
Error: +0.15 m (systematic offset)

Antenna delay calibration:
  Current delay: 16385
  Recommended: 16450

After calibration:
  Mean: 1.01 m
  Error: +0.01 m
  ✓ Accuracy: ±2 cm (excellent!)
```

---

## Test Execution Order

### Phase 1: Single Device Tests
1. Test 1: Chip ID (Initiator) - 10 minutes
2. Test 1: Chip ID (Responder) - 5 minutes
3. Test 2: GPIO/Reset (Both) - 10 minutes
4. Test 3: Simple TX (Initiator) - 15 minutes

### Phase 2: Two-Device Communication
5. Test 4: Simple RX (Both devices) - 20 minutes
6. Test 5: Ping-Pong (Both devices) - 30 minutes

### Phase 3: Timestamp Capture
7. Test 6: TX Timestamps - 20 minutes
8. Test 7: RX Timestamps - 30 minutes

### Phase 4: Full Ranging
9. Test 8: DS-TWR Protocol - 1-2 hours
10. Test 9: Distance Calculation - 2-4 hours

**Total Estimated Time**: 6-10 hours of testing and debugging

---

## Test Automation Scripts

### Python Monitor Script
**File**: `tests/monitor.py`

Monitors serial output from both devices simultaneously, logs to file, parses test results.

```python
# Usage:
python tests/monitor.py --initiator /dev/ttyACM0 --responder /dev/ttyACM1 --test 5
```

### Bash Test Runner
**File**: `tests/run_test.sh`

```bash
# Usage:
./tests/run_test.sh 1  # Run Test 1
./tests/run_test.sh 5  # Run Test 5
```

---

## Success Criteria Summary

| Test | Must Pass Before Next | Critical Metric |
|------|----------------------|-----------------|
| 1 | Yes | Chip ID = 0xDECA0302 |
| 2 | Yes | Reset succeeds |
| 3 | Yes | TX completes, timestamp ≠ 0 |
| 4 | Yes | RX succeeds, data matches |
| 5 | Yes | 10 ping-pongs succeed |
| 6 | Yes | Timestamps increment |
| 7 | Yes | TX/RX time diff < 1000 units |
| 8 | Yes | All 8 timestamps captured |
| 9 | No (optimization) | Distance within ±100cm |

---

## Troubleshooting Guide

### Test 1 Fails (No Chip ID)
**Probable Causes**:
- Wiring issue (MOSI/MISO swapped)
- No power to shield
- Bad SPI configuration
- CS pin wrong

**Debug Steps**:
1. Measure 3.3V at shield
2. Check all 8 wire connections
3. Try slower SPI speed
4. Add debug output to every SPI transaction

### Test 3/4 Fails (TX/RX not working)
**Probable Causes**:
- Wrong radio configuration
- Interrupt not firing
- Wrong register addresses
- Timing issue

**Debug Steps**:
1. Verify IRQ pin toggles (use LED)
2. Check status registers for errors
3. Reduce complexity (remove interrupts, poll instead)
4. Compare with working example code

### Test 8 Fails (Protocol incomplete)
**Probable Causes**:
- Timeout too short
- Message lost
- State machine bug
- Timestamp corruption

**Debug Steps**:
1. Add extensive state logging
2. Increase timeouts
3. Verify each message independently
4. Check for buffer overruns

---

## Documentation

After each test, document in `docs/findings/`:

**test-results-01.md**:
```markdown
# Test 1: Chip ID Read - Results

## Date: 2026-01-08

## Outcome: ✓ PASS

## Device ID Read
- Expected: 0xDECA0302
- Actual: 0xDECA0302
- Match: YES

## SPI Configuration
- Speed: 2 MHz
- Mode: MODE0
- Bit order: MSB first

## Issues Encountered
None

## Next Steps
Proceed to Test 2
```

---

**Status**: Ready to begin Test 1
**Updated**: 2026-01-08
