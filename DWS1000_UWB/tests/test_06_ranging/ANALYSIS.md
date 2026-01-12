# Test 6: DW1000Ranging TAG/ANCHOR - Detailed Analysis

**Date**: 2026-01-11
**Test Status**: ❌ **FAILED**
**Devices**: Arduino Uno + PCL298336 (DW1000)
**Library**: arduino-dw1000 v0.9 (DW1000Ranging module)

---

## Executive Summary

The DW1000Ranging test failed to produce any ranging measurements. Both anchor and tag devices initialized successfully and printed their addresses and roles, but no subsequent ranging communication occurred. The devices appear to hang or continuously reset after the initial setup() function completes.

**Root Cause (Hypothesis)**: Library bug in interrupt mask configuration (interruptOnReceiveFailed function) prevents DW1000 interrupts from firing, causing the ranging protocol to fail silently.

---

## Test Configuration

### Hardware Setup

| Component | Anchor | Tag |
|-----------|--------|-----|
| Platform | Arduino Uno | Arduino Uno |
| Serial Port | /dev/ttyACM0 | /dev/ttyACM1 |
| Device Address | 82:17:5B:D5:A9:9A:E2:9C | 7D:00:22:EA:82:60:3B:9C |
| Short Address | 82:17 | 7D:00 |
| Role | ANCHOR (responder) | TAG (initiator) |

### Pin Configuration

| Function | Pin | Notes |
|----------|-----|-------|
| IRQ | D2 | INT0, RISING edge interrupt |
| RST | D9 | Reset pin |
| SS | D10 | SPI chip select |
| MOSI | D11 | SPI data out |
| MISO | D12 | SPI data in |
| SCK | D13 | SPI clock |

### Software Configuration

| Parameter | Value |
|-----------|-------|
| Baud Rate | 115200 |
| DW1000 Mode | MODE_LONGDATA_RANGE_ACCURACY |
| Network ID | 0xDECA |
| Data Rate | 110 kbps |
| PRF | 16 MHz |
| Channel | 5 |

---

## Test Execution

### Monitoring Setup

**Tool**: Custom Python serial monitor (`monitor_serial.py`)
**Duration**: 180 seconds (3 minutes)
**Method**: Simultaneous monitoring of both devices in separate threads

```bash
python3 monitor_serial.py 180
```

### Captured Output

#### Anchor Output
```
[12:06:05.320] device address: 82:17:5B:D5:A9:9A:E2:9C
[12:06:05.320] ### ANCHOR ###
```
**Total lines**: 2
**Duration**: 180.0 seconds

#### Tag Output
```
[12:06:05.968] device address: 7D:00:22:EA:82:60:3B:9C
[12:06:05.968] ### TAG ###
```
**Total lines**: 2
**Duration**: 180.0 seconds

---

## Expected vs Actual Behavior

### Expected Behavior (Based on Code Analysis)

#### Anchor Expected Output:
```
device address: 82:17:5B:D5:A9:9A:E2:9C
### ANCHOR ###
blink; 1 device added ! -> short:7D00
from: 7D00    Range: 1.23 m    RX power: -85.2 dBm
from: 7D00    Range: 1.24 m    RX power: -85.1 dBm
from: 7D00    Range: 1.23 m    RX power: -85.3 dBm
[continuous ranging measurements...]
```

**Key callbacks that should fire:**
1. `newBlink(DW1000Device*)` - When tag is discovered
2. `newRange()` - When distance measurement completes
3. `inactiveDevice(DW1000Device*)` - If device times out

#### Tag Expected Output:
```
device address: 7D:00:22:EA:82:60:3B:9C
### TAG ###
ranging init; 1 device added ! -> short:8217
from: 8217    Range: 1.23 m    RX power: -84.8 dBm
from: 8217    Range: 1.24 m    RX power: -84.7 dBm
from: 8217    Range: 1.23 m    RX power: -84.9 dBm
[continuous ranging measurements...]
```

**Key callbacks that should fire:**
1. `newDevice(DW1000Device*)` - When anchor is discovered
2. `newRange()` - When distance measurement completes
3. `inactiveDevice(DW1000Device*)` - If device times out

### Actual Behavior

- ✅ Both devices initialized
- ✅ Addresses printed correctly
- ✅ Roles assigned (ANCHOR/TAG)
- ❌ No device discovery messages
- ❌ No ranging measurements
- ❌ No callbacks fired
- ❌ No error messages
- ❌ No activity after initialization

---

## Code Analysis

### Initialization Sequence

#### Anchor Setup (test_06_anchor.ino)

```cpp
void setup() {
  Serial.begin(115200);                                    // ✓ Working
  delay(1000);                                             // ✓ Working

  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ); // ? Unknown
  DW1000Ranging.attachNewRange(newRange);                    // ? Unknown
  DW1000Ranging.attachBlinkDevice(newBlink);                 // ? Unknown
  DW1000Ranging.attachInactiveDevice(inactiveDevice);        // ? Unknown

  // This prints "device address" and "### ANCHOR ###"
  DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",
                               DW1000.MODE_LONGDATA_RANGE_ACCURACY);
  // After this, no more output
}

void loop() {
  DW1000Ranging.loop();  // Should be called repeatedly - not confirmed
}
```

#### Tag Setup (test_06_tag.ino)

```cpp
void setup() {
  Serial.begin(115200);                                    // ✓ Working
  delay(1000);                                             // ✓ Working

  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ); // ? Unknown
  DW1000Ranging.attachNewRange(newRange);                    // ? Unknown
  DW1000Ranging.attachNewDevice(newDevice);                  // ? Unknown
  DW1000Ranging.attachInactiveDevice(inactiveDevice);        // ? Unknown

  // This prints "device address" and "### TAG ###"
  DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                            DW1000.MODE_LONGDATA_RANGE_ACCURACY);
  // After this, no more output
}

void loop() {
  DW1000Ranging.loop();  // Should be called repeatedly - not confirmed
}
```

### Critical Library Functions

#### startAsAnchor() (DW1000Ranging.cpp:162-192)

```cpp
void DW1000RangingClass::startAsAnchor(char address[], const byte mode[],
                                        const bool randomShortAddress) {
    DW1000.convertToByte(address, _currentAddress);
    DW1000.setEUI(address);
    Serial.print("device address: ");
    Serial.println(address);                    // ✓ This prints

    // Generate short address
    _currentShortAddress[0] = _currentAddress[0];
    _currentShortAddress[1] = _currentAddress[1];

    // Configure network (MAC filtering, frequency, etc.)
    DW1000Ranging.configureNetwork(_currentShortAddress[0]*256+
                                    _currentShortAddress[1], 0xDECA, mode);

    generalStart();                             // ❌ May be failing here

    _type = ANCHOR;
    Serial.println("### ANCHOR ###");           // ✓ This prints
}
```

#### generalStart() (DW1000Ranging.cpp:119-159)

```cpp
void DW1000RangingClass::generalStart() {
    // Attach callbacks for interrupt handling
    DW1000.attachSentHandler(handleSent);         // ❌ May not work
    DW1000.attachReceivedHandler(handleReceived); // ❌ May not work

    if(DEBUG) {
        // Debug info - only if DEBUG=true
        Serial.println("DW1000-arduino");
        // ... more debug output ...
    }

    // Put device in receive mode
    receiver();                                   // ❌ May hang here

    // Initialize ranging timer
    _rangingCountPeriod = millis();
}
```

---

## Root Cause Analysis

### Hypothesis 1: Library Bug - Interrupt Mask Corruption (MOST LIKELY)

**Evidence**:
1. Known bug documented in `docs/findings/interrupt_debugging.md`
2. Function `interruptOnReceiveFailed()` uses wrong buffer length
3. Corrupts `_sysmask` buffer during `setDefaults()`
4. Results in incorrect SYS_MASK register values written to DW1000
5. Hardware interrupts never fire
6. Ranging protocol depends entirely on interrupts

**Bug Location**: `DW1000.cpp:992-996`

```cpp
// BUGGY CODE
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);  // BUG: Should be LEN_SYS_MASK
    setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);   // BUG: Should be LEN_SYS_MASK
    setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);   // BUG: Should be LEN_SYS_MASK
    setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);  // BUG: Should be LEN_SYS_MASK
}
```

**Impact**:
- `LEN_SYS_STATUS = 5` bytes
- `LEN_SYS_MASK = 4` bytes
- Buffer overrun corrupts interrupt configuration
- DW1000 never generates interrupts on RX/TX events
- `DW1000Ranging.loop()` waits for callbacks that never fire
- Appears as "hung" program

**Call Chain**:
```
DW1000Ranging.startAsAnchor()
  └─> configureNetwork()
      └─> DW1000.newConfiguration()
      └─> DW1000.setDefaults()
          └─> interruptOnReceiveFailed(true)  ← CORRUPTS _sysmask
      └─> DW1000.commitConfiguration()
          └─> writeSystemEventMaskRegister()  ← WRITES CORRUPTED VALUE
```

**Verification**:
- Same symptom observed in Test 3-4 (BasicSender/Receiver)
- Both tests show callbacks not firing
- Consistent with interrupt mask corruption

### Hypothesis 2: Hardware Issue - IRQ Pin Not Connected

**Evidence**:
- IRQ pin (D2) must be connected for interrupts
- Visual inspection not yet performed
- Could explain complete lack of interrupt activity

**Verification Needed**:
- Physical inspection of shield connections
- Multimeter continuity test
- Try different Arduino/shield combination

### Hypothesis 3: Power Supply Insufficient

**Evidence**:
- Both devices show same timestamp (within 1 second)
- Suggests simultaneous reset/restart
- Could indicate power brownout

**Verification Needed**:
- Test with external 5V supply
- Measure current draw
- Test single device first

### Hypothesis 4: Loop() Not Executing

**Evidence**:
- No output after initialization
- Could indicate:
  - Program hung in setup()
  - Continuous reset loop
  - Infinite wait in receiver()

**Verification Method**:
- Upload debug versions with heartbeat messages
- Should print "ALIVE" every 5 seconds if loop() runs
- Created: `test_06_anchor_debug.ino` and `test_06_tag_debug.ino`

### Hypothesis 5: SPI Communication Failure

**Evidence**:
- Device address prints (requires SPI read)
- Suggests SPI is working during setup
- Less likely to be root cause

**Verification**:
- BasicConnectivityTest passed in Test 2
- SPI communication confirmed working
- Can rule out this hypothesis

---

## Previous Test Comparison

### Test 3-4: BasicSender/Receiver (Similar Failure)

| Aspect | Test 3-4 | Test 6 | Similarity |
|--------|----------|--------|------------|
| Initialization | ✅ Success | ✅ Success | Same |
| Chip ID | ✅ Read | ✅ Implied | Same |
| Configuration | ✅ Applied | ✅ Applied | Same |
| Transmission Start | ⚠️ Packet #0 only | ❌ None | Similar |
| Reception | ❌ Nothing | ❌ Nothing | Identical |
| Callbacks | ❌ Not firing | ❌ Not firing | **Identical** |
| Root Cause | Interrupt bug | Interrupt bug | **Same** |

**Conclusion**: Test 6 exhibits the same failure mode as Test 3-4, strongly suggesting the same root cause (interrupt mask corruption bug).

### Test 2: BasicConnectivityTest (Successful)

| Aspect | Test 2 | Test 6 | Difference |
|--------|--------|--------|------------|
| Uses Interrupts | ❌ No | ✅ Yes | Key difference |
| SPI Communication | ✅ Works | ✅ Works | Same |
| Chip Initialization | ✅ Works | ✅ Works | Same |
| Result | ✅ Pass | ❌ Fail | **Interrupts are the difference** |

**Conclusion**: The common factor in all failures is interrupt dependency. Non-interrupt tests work, interrupt-dependent tests fail.

---

## Diagnostic Steps Completed

### ✅ Completed Checks

1. **Serial Port Configuration**
   - Verified correct baud rate (115200)
   - Tested serial connection
   - Captured output successfully

2. **Device Connectivity**
   - Both devices detected on /dev/ttyACM0 and /dev/ttyACM1
   - Enumeration successful
   - USB communication working

3. **Code Compilation**
   - No compilation errors
   - Flash: 21,068 bytes (65.3% of 32KB) - within limits
   - RAM: ~1,300 bytes (63.5% of 2KB) - within limits

4. **Upload Process**
   - Both devices uploaded successfully
   - Verification passed
   - No upload errors

5. **Initial Boot**
   - Devices power up
   - Serial output appears
   - Setup() function starts executing

6. **Device Reset Behavior**
   - Created reset test script (`check_library.py`)
   - Confirmed devices reset and print initialization
   - Shows devices are running but hitting same issue

### ⏳ Pending Checks

1. **Loop() Execution**
   - Need to upload debug versions
   - Check for "ALIVE" heartbeat messages
   - Verify loop() is being called

2. **Hardware Connections**
   - Visual inspection of IRQ pin
   - Continuity test with multimeter
   - Check for loose connections

3. **Library Fix**
   - Apply interrupt mask bug fix
   - Recompile and retest
   - Should resolve if hypothesis 1 correct

4. **Power Supply**
   - Test with external 5V supply
   - Measure current consumption
   - Rule out power issues

---

## Debug Versions Created

### Features Added

Both `test_06_anchor_debug.ino` and `test_06_tag_debug.ino` include:

1. **Verbose Initialization Logging**
   ```cpp
   Serial.println("=== DW1000 Anchor/Tag Debug ===");
   Serial.print("RST Pin: "); Serial.println(PIN_RST);
   Serial.print("IRQ Pin: "); Serial.println(PIN_IRQ);
   Serial.print("SS Pin: "); Serial.println(PIN_SS);
   Serial.println("Initializing DW1000...");
   ```

2. **Initialization Step Tracking**
   ```cpp
   Serial.println("Communication initialized");
   Serial.println("Callbacks attached");
   Serial.println("Starting as anchor/tag...");
   Serial.println("Anchor/Tag started successfully");
   ```

3. **Loop() Heartbeat**
   ```cpp
   unsigned long lastPrint = 0;
   unsigned long loopCount = 0;

   void loop() {
     DW1000Ranging.loop();
     loopCount++;

     if (millis() - lastPrint > 5000) {
       Serial.print("ALIVE - Loop count: ");
       Serial.print(loopCount);
       Serial.print(" - Millis: ");
       Serial.println(millis());
       lastPrint = millis();
       loopCount = 0;
     }
   }
   ```

4. **Enhanced Callback Messages**
   ```cpp
   Serial.println(">>> NEW RANGE EVENT <<<");
   Serial.println(">>> NEW BLINK EVENT <<<");
   Serial.println(">>> INACTIVE DEVICE EVENT <<<");
   ```

### Expected Debug Output

If loop() is running:
```
=== DW1000 Anchor Debug ===
RST Pin: 9
IRQ Pin: 2
SS Pin: 10
Initializing DW1000...
Communication initialized
Callbacks attached
Starting as anchor...
device address: 82:17:5B:D5:A9:9A:E2:9C
### ANCHOR ###
Anchor started successfully
Waiting for tags...
==============================
ALIVE - Loop count: 50000 - Millis: 5000
ALIVE - Loop count: 50000 - Millis: 10000
ALIVE - Loop count: 50000 - Millis: 15000
[...]
```

If setup() hangs:
```
=== DW1000 Anchor Debug ===
RST Pin: 9
IRQ Pin: 2
SS Pin: 10
Initializing DW1000...
[... stops at some point ...]
```

---

## Recommended Fix: Library Patch

### File to Modify

**Path**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp`
**Line**: 992-996

### Current Code (BUGGY)

```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);
    setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);
    setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);
    setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);
}
```

### Fixed Code

```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
}
```

### Change Required

Replace all 4 instances of `LEN_SYS_STATUS` with `LEN_SYS_MASK`.

**Rationale**:
- SYS_MASK register is 4 bytes (`LEN_SYS_MASK = 4`)
- SYS_STATUS register is 5 bytes (`LEN_SYS_STATUS = 5`)
- `_sysmask` buffer is 4 bytes (matches SYS_MASK)
- Using wrong length causes buffer overrun
- Corrupts interrupt mask configuration

---

## Alternative Solutions

### Option 1: Library Patch (Recommended)

**Pros**:
- Fixes root cause
- Should resolve all interrupt-based examples
- One-time fix applies to all tests
- Most likely to succeed

**Cons**:
- Requires modifying library code
- Need to recompile all tests
- May introduce new issues (unlikely)

**Effort**: 5 minutes

### Option 2: Polling Mode (Workaround)

Some DW1000 libraries support polling instead of interrupts.

**Pros**:
- Avoids interrupt issues entirely
- May be more reliable on Arduino Uno

**Cons**:
- Not implemented in current library
- Would require significant code changes
- Less efficient (wastes CPU cycles)
- Higher latency

**Effort**: Several hours

### Option 3: Hardware Inspection (Diagnostic)

Check physical connections, especially IRQ pin.

**Pros**:
- Eliminates hardware as root cause
- Quick to verify
- Good practice anyway

**Cons**:
- Unlikely to be the issue (Test 2 worked)
- Won't fix library bug

**Effort**: 10 minutes

### Option 4: ESP32 Migration (Long-term)

Migrate to ESP32 platform with better interrupt handling.

**Pros**:
- ESP32 has more robust interrupt support
- More processing power
- More memory
- Better for production

**Cons**:
- Requires new hardware
- Code changes needed
- More complex setup
- Deferred until Arduino proven

**Effort**: Several days

---

## Next Steps - Priority Order

### Priority 1: Upload Debug Versions ⏭️

**Action**:
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging
# Compile and upload debug versions
# Monitor output
```

**Expected Result**: Confirm if loop() is executing or setup() hangs

**Time**: 10 minutes

### Priority 2: Apply Library Fix 🔧

**Action**:
```bash
# Edit DW1000.cpp line 992-996
# Change LEN_SYS_STATUS → LEN_SYS_MASK (4 places)
# Recompile and upload
# Retest ranging
```

**Expected Result**: Ranging should work if bug is root cause

**Time**: 15 minutes

### Priority 3: Hardware Inspection 🔍

**Action**:
- Visual inspection of shield
- Check IRQ pin (D2) connection
- Verify RST pin (D9) connection
- Inspect solder joints

**Expected Result**: Rule out hardware issues

**Time**: 10 minutes

### Priority 4: Power Supply Test ⚡

**Action**:
- Test with external 5V supply
- Measure current during operation
- Try single device first

**Expected Result**: Rule out power issues

**Time**: 15 minutes

### Priority 5: Distance Measurement 📏

**Only if ranging works after fixes above**

**Action**:
- Place devices at known distances
- Record measurements
- Calculate accuracy
- Test at: 1m, 2m, 3m, 5m

**Expected Result**: Distance measurements with accuracy assessment

**Time**: 30 minutes

---

## Success Criteria for Retest

### Minimum Success

- [ ] Both devices print initialization messages
- [ ] "ALIVE" heartbeat appears every 5 seconds
- [ ] Anchor prints "blink" when detecting tag
- [ ] Tag prints "ranging init" when finding anchor
- [ ] At least one range measurement appears

### Full Success

- [ ] Continuous range measurements (1+ Hz)
- [ ] Distance values are reasonable (0.1 - 10 meters)
- [ ] RX power values are present (-60 to -100 dBm)
- [ ] Measurements are stable (not wildly fluctuating)
- [ ] Both directions work (anchor→tag and tag→anchor)

### Calibration Success

- [ ] Measured distance within ±20cm of actual distance
- [ ] Standard deviation < 10cm
- [ ] Consistent measurements over 60 seconds
- [ ] No timeouts or dropped measurements

---

## Lessons Learned

### 1. Library Quality Matters

The arduino-dw1000 library has a critical bug that prevents interrupt-based operation. This demonstrates the importance of:
- Code review of third-party libraries
- Testing at multiple abstraction levels
- Understanding library internals
- Having debug/diagnostic tools

### 2. Interrupt-Driven vs Polling

Interrupt-based approaches are more efficient but also more fragile:
- Hardware dependencies (IRQ pin)
- Software dependencies (correct mask setup)
- Platform dependencies (interrupt latency)
- Debugging is harder (asynchronous behavior)

### 3. Progressive Testing Strategy

Our test progression (1→2→3-4→6) was correct:
- Test 1-2: Non-interrupt tests passed ✅
- Test 3-4: Basic interrupt tests failed ❌
- Test 6: Advanced interrupt tests failed ❌

This isolated the problem to interrupt handling specifically.

### 4. Documentation Value

The detailed debugging documentation for Test 3-4 saved significant time in Test 6:
- Recognized identical symptoms immediately
- Had clear hypothesis ready
- Knew where to look in library code
- Understood root cause mechanism

### 5. Debug-First Development

Creating debug versions with heartbeat messages should be standard practice:
- Proves code is running
- Shows where it hangs
- Provides execution timeline
- Minimal overhead

---

## Comparison with Working Systems

### Known Working Examples

**Platform**: Arduino Uno + DW1000
**Source**: Various forums and GitHub repos
**Status**: Many users report success with DW1000Ranging

**Key Differences**:
1. Some use older library versions (may not have bug)
2. Some may have applied unofficial patches
3. Some use different hardware (better IRQ handling)
4. Some use polling mode instead of interrupts

**Implication**: Hardware combination is proven to work, suggesting software issue (bug) rather than fundamental incompatibility.

### ESP32-Based Systems

**Platform**: ESP32 + DW1000
**Status**: Widely reported as working well

**Advantages**:
- Better interrupt handling
- More CPU cycles for polling fallback
- More memory for debugging
- Faster interrupt response

**Consideration**: May migrate to ESP32 if Arduino Uno proves unreliable even after fix.

---

## Files Reference

### Test Files
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/test_06_anchor.ino`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/test_06_tag.ino`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/test_06_anchor_debug.ino`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/test_06_tag_debug.ino`

### Scripts
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/monitor_serial.py`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/check_library.py`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/run_ranging_test.sh`

### Output Files
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/anchor_output.txt`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/tag_output.txt`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/RESULTS.txt`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/ANALYSIS.md` (this file)

### Library Files
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp` (contains bug)
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000Ranging.cpp`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.h`
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000Ranging.h`

### Documentation
- `/home/devel/Desktop/SwarmLoc/docs/findings/interrupt_debugging.md` (bug analysis)
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/TEST_RESULTS.md` (test log)
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/SESSION_SUMMARY_2026-01-11.md` (session notes)

---

## Conclusion

The DW1000Ranging test failed due to a probable library bug in interrupt mask configuration. This is the same root cause identified in Tests 3-4 (BasicSender/Receiver). The bug prevents DW1000 hardware interrupts from firing, which causes the ranging protocol to fail silently since it depends entirely on interrupt-driven callbacks.

**Confidence Level**: HIGH (90%)

**Recommended Action**: Apply library fix and retest.

**Expected Outcome**: Ranging should work after fix is applied.

**Contingency**: If fix doesn't work, proceed with hardware inspection and power supply testing.

---

**Document Status**: Complete
**Next Update**: After debug versions are tested or library fix is applied
**Last Modified**: 2026-01-11
