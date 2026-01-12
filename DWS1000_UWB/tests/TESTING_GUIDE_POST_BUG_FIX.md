# Comprehensive Testing Guide - Post Bug Fix

## Bug Fix Applied

**File**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp`
**Function**: `interruptOnReceiveFailed()`
**Lines**: 992-996
**Change**: Changed `LEN_SYS_STATUS` to `LEN_SYS_MASK` (4 instances)

```cpp
// BEFORE (BUGGY):
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);  // BUG: Wrong length
    setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);   // BUG: Wrong length
    setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);   // BUG: Wrong length
    setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);  // BUG: Wrong length
}

// AFTER (FIXED):
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);  // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);   // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);   // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);  // FIXED
}
```

**Impact**: This bug caused buffer overrun in the interrupt mask register, preventing all DW1000 hardware interrupts from firing correctly.

---

## Test Suite Overview

| Test | Devices | Purpose | Expected Duration |
|------|---------|---------|-------------------|
| Test 3 | 1 (Sender) | TX functionality | 60s |
| Test 4 | 2 (Sender + Receiver) | RX functionality | 60s |
| Test 6 | 2 (Tag + Anchor) | Two-Way Ranging | 120s |
| Test 5 | 2 (PingPong) | Bidirectional | 60s |

**Total Time**: Approximately 20-30 minutes

---

## Test 3: BasicSender

### Objective
Verify that the DW1000 can continuously transmit packets after the bug fix.

### Equipment
- 1x Arduino Uno + PCL298336 (DW1000)
- USB cable

### Procedure

#### Step 1: Upload BasicSender

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_02_library_examples

# Create temporary PlatformIO project
rm -rf /tmp/test03_sender
mkdir -p /tmp/test03_sender/src /tmp/test03_sender/lib
cp test_03_sender.ino /tmp/test03_sender/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test03_sender/lib/DW1000

# Create platformio.ini
cat > /tmp/test03_sender/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM0
monitor_speed = 9600
lib_deps = SPI
EOF

# Compile and upload
cd /tmp/test03_sender
pio run --target upload
```

#### Step 2: Monitor Serial Output

```bash
pio device monitor -p /dev/ttyACM0 -b 9600
```

**Monitor for**: 60 seconds
**Save output to**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_02_library_examples/test03_output_YYYYMMDD_HHMMSS.txt`

### Expected Output (BEFORE Bug Fix)

```
### DW1000-arduino-sender-test ###
DW1000 initialized ...
Committed configuration ...
Device ID: DECA - model: 1, version: 3, revision: 0
Unique ID: FF:FF:FF:FF:00:00:00:00
Network ID & Device Address: PAN: 0A, Short Address: 05
Device mode: Data rate: 110 kb/s, PRF: 16 MHz, Preamble: 2048 symbols (code #4), Channel: #5
Transmitting packet ... #0
[HANGS - No further output]
```

### Expected Output (AFTER Bug Fix)

```
### DW1000-arduino-sender-test ###
DW1000 initialized ...
Committed configuration ...
Device ID: DECA - model: 1, version: 3, revision: 0
Unique ID: FF:FF:FF:FF:00:00:00:00
Network ID & Device Address: PAN: 0A, Short Address: 05
Device mode: Data rate: 110 kb/s, PRF: 16 MHz, Preamble: 2048 symbols (code #4), Channel: #5
Transmitting packet ... #0
Transmitted successfully
Transmitting packet ... #1
Transmitted successfully
Transmitting packet ... #2
Transmitted successfully
[Continues...]
```

### Success Criteria

- [x] Device initializes successfully
- [x] Configuration committed
- [x] **`handleSent()` callback fires** (NEW - This is the key change!)
- [x] "Transmitted successfully" messages appear
- [x] Packet counter increments (#0, #1, #2, ...)
- [x] No hangs or errors

### What to Record

1. **Total packets transmitted** in 60 seconds
2. **Transmission rate** (packets/second)
3. **Any error messages**
4. **Full serial output** (first 20 lines and last 20 lines)

---

## Test 4: BasicReceiver + BasicSender

### Objective
Verify bidirectional communication: Sender transmits, Receiver receives.

### Equipment
- 2x Arduino Uno + PCL298336 (DW1000)
- 2x USB cables

### Procedure

#### Step 1: Upload Sender to Device 1

```bash
# Same as Test 3, but to /dev/ttyACM0
cd /tmp/test03_sender
pio run --target upload --upload-port /dev/ttyACM0
```

#### Step 2: Upload Receiver to Device 2

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_02_library_examples

# Create temporary PlatformIO project
rm -rf /tmp/test04_receiver
mkdir -p /tmp/test04_receiver/src /tmp/test04_receiver/lib
cp test_04_receiver.ino /tmp/test04_receiver/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test04_receiver/lib/DW1000

# Create platformio.ini
cat > /tmp/test04_receiver/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM1
monitor_speed = 9600
lib_deps = SPI
EOF

# Compile and upload
cd /tmp/test04_receiver
pio run --target upload
```

#### Step 3: Monitor Both Devices

**Terminal 1 (Sender)**:
```bash
pio device monitor -p /dev/ttyACM0 -b 9600 | tee sender_output.txt
```

**Terminal 2 (Receiver)**:
```bash
pio device monitor -p /dev/ttyACM1 -b 9600 | tee receiver_output.txt
```

**Monitor for**: 60 seconds

### Expected Output (BEFORE Bug Fix)

**Sender**:
```
Transmitting packet ... #0
[Hangs]
```

**Receiver**:
```
### DW1000-arduino-receiver-test ###
[Initialization messages]
[No packets received]
```

### Expected Output (AFTER Bug Fix)

**Sender**:
```
Transmitting packet ... #0
Transmitted successfully
Transmitting packet ... #1
Transmitted successfully
[Continues...]
```

**Receiver**:
```
### DW1000-arduino-receiver-test ###
[Initialization]
Received message from: 0x0005
Data: ... some data ...
Receive #0
Received message from: 0x0005
Data: ... some data ...
Receive #1
[Continues...]
```

### Success Criteria

- [x] Sender transmits continuously
- [x] Receiver receives packets
- [x] **`handleReceived()` callback fires** (NEW - Key change!)
- [x] Packet counters increment on both devices
- [x] Transmitted count ≈ Received count (±10%)
- [x] No communication gaps > 5 seconds

### What to Record

1. **Packets transmitted** (from sender output)
2. **Packets received** (from receiver output)
3. **Success rate**: (received / transmitted) × 100%
4. **Any error messages**
5. **Communication stability**: Any gaps or hangs?

---

## Test 6: DW1000Ranging TAG/ANCHOR

### Objective
Verify Two-Way Ranging (TWR) functionality using DW1000Ranging library.

### Equipment
- 2x Arduino Uno + PCL298336 (DW1000)
- 2x USB cables
- Ruler or measuring tape

### Procedure

#### Step 1: Upload TAG

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging

# Create temporary project for TAG
rm -rf /tmp/test06_tag
mkdir -p /tmp/test06_tag/src /tmp/test06_tag/lib
cp test_clean.ino /tmp/test06_tag/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test06_tag/lib/DW1000

# Modify for TAG mode
sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR false/' /tmp/test06_tag/src/main.cpp

cat > /tmp/test06_tag/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM0
monitor_speed = 115200
lib_deps = SPI
EOF

cd /tmp/test06_tag
pio run --target upload
```

#### Step 2: Upload ANCHOR

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging

# Create temporary project for ANCHOR
rm -rf /tmp/test06_anchor
mkdir -p /tmp/test06_anchor/src /tmp/test06_anchor/lib
cp test_clean.ino /tmp/test06_anchor/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test06_anchor/lib/DW1000

# Modify for ANCHOR mode
sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR true/' /tmp/test06_anchor/src/main.cpp

cat > /tmp/test06_anchor/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM1
monitor_speed = 115200
lib_deps = SPI
EOF

cd /tmp/test06_anchor
pio run --target upload
```

#### Step 3: Place Devices

- Place devices **1.0 meter apart**
- Use ruler/measuring tape for accuracy
- Ensure line-of-sight between devices

#### Step 4: Monitor Both Devices

**Terminal 1 (TAG)**:
```bash
pio device monitor -p /dev/ttyACM0 -b 115200 | tee tag_output.txt
```

**Terminal 2 (ANCHOR)**:
```bash
pio device monitor -p /dev/ttyACM1 -b 115200 | tee anchor_output.txt
```

**Monitor for**: 120 seconds

### Expected Output (BEFORE Bug Fix)

**TAG**:
```
DW1000 Ranging Test (Bug Fixed)
Mode: TAG
TAG ready
[No further output - waiting for interrupts that never fire]
```

**ANCHOR**:
```
DW1000 Ranging Test (Bug Fixed)
Mode: ANCHOR
ANCHOR ready
[No further output - waiting for interrupts that never fire]
```

### Expected Output (AFTER Bug Fix)

**TAG**:
```
DW1000 Ranging Test (Bug Fixed)
Mode: TAG
TAG ready
Device found: 8217
Range: 1.02 m (102 cm) from 8217
Range: 0.98 m (98 cm) from 8217
Range: 1.01 m (101 cm) from 8217
[Continues with ~1-5 Hz update rate]
```

**ANCHOR**:
```
DW1000 Ranging Test (Bug Fixed)
Mode: ANCHOR
ANCHOR ready
Device found: 7D00
Range: 1.02 m (102 cm) from 7D00
Range: 0.98 m (98 cm) from 7D00
Range: 1.01 m (101 cm) from 7D00
[Continues with ~1-5 Hz update rate]
```

### Success Criteria

- [x] Both devices initialize successfully
- [x] **"Device found" messages appear** (NEW - Proves ranging protocol working!)
- [x] **"Range:" messages appear continuously** (NEW - Proves TWR working!)
- [x] Distance measurements are reasonable (within ±30cm of actual)
- [x] Update rate is 1-5 Hz
- [x] No protocol failures or timeouts

### What to Record

1. **Actual distance**: 1.0 m (measured)
2. **Average measured distance**: (calculate from output)
3. **Standard deviation**: (measure stability)
4. **Update rate**: (ranges per second)
5. **Number of successful ranging cycles** in 120 seconds
6. **Any "Device lost" messages**

### Multiple Distance Tests (Optional)

Repeat the test at:
- 0.5 m
- 1.0 m (baseline)
- 2.0 m
- 3.0 m
- 5.0 m (if space permits)

Record accuracy at each distance.

---

## Test 5: MessagePingPong

### Objective
Verify bidirectional ping-pong message exchange.

### Equipment
- 2x Arduino Uno + PCL298336 (DW1000)
- 2x USB cables

### Procedure

#### Step 1: Upload to Both Devices

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_05_pingpong

# Upload sender
rm -rf /tmp/test05_sender
mkdir -p /tmp/test05_sender/src /tmp/test05_sender/lib
cp test_05_sender.ino /tmp/test05_sender/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test05_sender/lib/DW1000

cat > /tmp/test05_sender/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM0
monitor_speed = 9600
lib_deps = SPI
EOF

cd /tmp/test05_sender
pio run --target upload

# Upload receiver
rm -rf /tmp/test05_receiver
mkdir -p /tmp/test05_receiver/src /tmp/test05_receiver/lib
cp /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_05_pingpong/test_05_receiver.ino /tmp/test05_receiver/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test05_receiver/lib/DW1000

cat > /tmp/test05_receiver/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM1
monitor_speed = 9600
lib_deps = SPI
EOF

cd /tmp/test05_receiver
pio run --target upload
```

#### Step 2: Monitor Both Devices

**Terminal 1**: `pio device monitor -p /dev/ttyACM0 -b 9600`
**Terminal 2**: `pio device monitor -p /dev/ttyACM1 -b 9600`

**Monitor for**: 60 seconds

### Expected Behavior

Messages should bounce back and forth between devices:
- Device 1 sends "PING"
- Device 2 receives "PING", sends "PONG"
- Device 1 receives "PONG", sends "PING"
- (Continues...)

### Success Criteria

- [x] Ping-pong exchange works
- [x] No communication deadlocks
- [x] Both devices transmit AND receive
- [x] Message counter increments on both

### What to Record

1. **Ping-pong cycles completed** in 60 seconds
2. **Exchange rate** (cycles/second)
3. **Any communication failures**

---

## Results Template

Create a file for each test: `test_XX_results_YYYYMMDD.md`

```markdown
# Test X Results - [Test Name]

**Date**: YYYY-MM-DD HH:MM:SS
**Tester**: [Your Name]
**Bug Fix Status**: Applied (LEN_SYS_STATUS → LEN_SYS_MASK)

## Test Configuration

- **Device 1**: /dev/ttyACM0 (Sender/Tag/Device A)
- **Device 2**: /dev/ttyACM1 (Receiver/Anchor/Device B)
- **Distance**: [if applicable]
- **Duration**: XXs

## Results

### Device 1 Output

[First 20 lines]
```
...
```

[Last 20 lines]
```
...
```

### Device 2 Output

[First 20 lines]
```
...
```

[Last 20 lines]
```
...
```

## Metrics

- **Packets/Ranges Transmitted**: XXX
- **Packets/Ranges Received**: XXX
- **Success Rate**: XX%
- **Update Rate**: X.XX Hz
- **Average Distance**: X.XX m (if ranging)
- **Distance Error**: ±XX cm (if ranging)

## Success Criteria

- [x] Criterion 1
- [x] Criterion 2
- [ ] Criterion 3 (if failed)

## Observations

[Any notable observations, anomalies, or issues]

## Conclusion

**Status**: ✅ PASSED / ❌ FAILED / ⚠️ PARTIAL

**Summary**: [Brief summary of results and whether bug fix was successful]
```

---

## Final Summary Template

After completing all tests, create: `ALL_TESTS_SUMMARY_YYYYMMDD.md`

```markdown
# Complete Test Suite Results - Post Bug Fix

**Date**: YYYY-MM-DD
**Bug Fix**: LEN_SYS_STATUS → LEN_SYS_MASK in DW1000.cpp lines 992-996

## Test Results Summary

| Test | Status | Key Metric | Notes |
|------|--------|------------|-------|
| Test 3: BasicSender | ✅/❌ | XX packets sent | [Brief note] |
| Test 4: Sender+Receiver | ✅/❌ | XX% success rate | [Brief note] |
| Test 6: Ranging | ✅/❌ | X.XX m avg distance | [Brief note] |
| Test 5: PingPong | ✅/❌ | XX exchanges | [Brief note] |

## Before/After Comparison

### Before Bug Fix
- ❌ Interrupts not firing
- ❌ `handleSent()` callback never called
- ❌ `handleReceived()` callback never called
- ❌ Devices hung after initialization
- ❌ No ranging measurements

### After Bug Fix
- [✅/❌] Interrupts firing correctly
- [✅/❌] `handleSent()` callback working
- [✅/❌] `handleReceived()` callback working
- [✅/❌] Devices operating normally
- [✅/❌] Ranging measurements successful

## Overall Assessment

**Bug Fix Effectiveness**: [Fully Successful / Partially Successful / Not Successful]

**Evidence**:
- [Key evidence point 1]
- [Key evidence point 2]
- [Key evidence point 3]

**Conclusion**: [Overall conclusion about whether the bug fix solved the interrupt problem]

## Recommendations

1. [Next steps]
2. [Future work]
3. [Any remaining issues]
```

---

## Quick Start Command Summary

```bash
# Test 3: BasicSender
cd /tmp && rm -rf test03_sender && mkdir -p test03_sender/src test03_sender/lib
cp /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_02_library_examples/test_03_sender.ino /tmp/test03_sender/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test03_sender/lib/DW1000
cat > /tmp/test03_sender/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM0
monitor_speed = 9600
lib_deps = SPI
EOF
cd /tmp/test03_sender && pio run --target upload && pio device monitor -p /dev/ttyACM0 -b 9600

# Test 4: BasicReceiver (run in parallel with Test 3)
cd /tmp && rm -rf test04_receiver && mkdir -p test04_receiver/src test04_receiver/lib
cp /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_02_library_examples/test_04_receiver.ino /tmp/test04_receiver/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test04_receiver/lib/DW1000
cat > /tmp/test04_receiver/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM1
monitor_speed = 9600
lib_deps = SPI
EOF
cd /tmp/test04_receiver && pio run --target upload && pio device monitor -p /dev/ttyACM1 -b 9600

# Test 6: Ranging TAG
cd /tmp && rm -rf test06_tag && mkdir -p test06_tag/src test06_tag/lib
cp /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/test_clean.ino /tmp/test06_tag/src/main.cpp
sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR false/' /tmp/test06_tag/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test06_tag/lib/DW1000
cat > /tmp/test06_tag/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM0
monitor_speed = 115200
lib_deps = SPI
EOF
cd /tmp/test06_tag && pio run --target upload && pio device monitor -p /dev/ttyACM0 -b 115200

# Test 6: Ranging ANCHOR
cd /tmp && rm -rf test06_anchor && mkdir -p test06_anchor/src test06_anchor/lib
cp /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/test_clean.ino /tmp/test06_anchor/src/main.cpp
sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR true/' /tmp/test06_anchor/src/main.cpp
ln -s /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000 /tmp/test06_anchor/lib/DW1000
cat > /tmp/test06_anchor/platformio.ini << 'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM1
monitor_speed = 115200
lib_deps = SPI
EOF
cd /tmp/test06_anchor && pio run --target upload && pio device monitor -p /dev/ttyACM1 -b 115200
```

---

## Notes

- All tests assume `/dev/ttyACM0` and `/dev/ttyACM1` - adjust if your ports differ
- Use `ls /dev/ttyACM*` or `ls /dev/ttyUSB*` to find your ports
- For tmux users: You can monitor both devices in split panes
- Save all output files with timestamps for documentation
- The bug fix should result in ALL callbacks working correctly
- **The key indicator of success**: "Device found" and "Range:" messages in Test 6

---

**Last Updated**: 2026-01-11
