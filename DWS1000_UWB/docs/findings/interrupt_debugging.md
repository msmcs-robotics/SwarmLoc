# DW1000 Interrupt Handling Issue on Arduino Uno - Debug Report

**Date:** 2026-01-11
**Platform:** Arduino Uno + DW1000 (PCL298336 shield)
**Library:** arduino-dw1000 v0.9
**Problem:** Interrupt callbacks not firing in BasicSender/BasicReceiver examples

---

## Problem Summary

### Observed Symptoms
1. **BasicSender**: Transmits packet #0 then stops (enters infinite loop)
2. **BasicReceiver**: Initializes successfully but receives no packets
3. **Root Cause**: Interrupt callbacks (`handleSent`, `handleReceived`) never execute

### Hardware Configuration
- **IRQ Pin**: D2 (INT0 on Arduino Uno)
- **RST Pin**: D9
- **SS Pin**: D10 (default SPI SS)
- **Interrupt Mode**: RISING edge (configured in library)

---

## Critical Bug Found in Library Code

### Bug Location
**File:** `/DWS1000_UWB/lib/DW1000/src/DW1000.cpp`
**Function:** `interruptOnReceiveFailed(boolean val)`
**Lines:** 992-996

### The Bug
```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);  // BUG: Should be LEN_SYS_MASK
    setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);   // BUG: Should be LEN_SYS_MASK
    setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);   // BUG: Should be LEN_SYS_MASK
    setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);  // BUG: Should be LEN_SYS_MASK
}
```

**Problem:** Uses `LEN_SYS_STATUS` (5 bytes) instead of `LEN_SYS_MASK` (4 bytes), causing buffer overrun and corruption of the interrupt mask register.

### Correct Implementation
```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);   // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);    // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);    // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);   // FIXED
}
```

---

## Why This Breaks BasicSender/BasicReceiver

### Call Chain
1. **BasicSender/BasicReceiver setup()**
   ```cpp
   DW1000.newConfiguration();
   DW1000.setDefaults();      // <- Calls interruptOnReceiveFailed(true)
   DW1000.commitConfiguration();
   ```

2. **setDefaults() implementation** (lines 1251-1277)
   ```cpp
   void DW1000Class::setDefaults() {
       if(_deviceMode == IDLE_MODE) {
           // ... other settings ...
           interruptOnSent(true);           // OK
           interruptOnReceived(true);       // OK
           interruptOnReceiveFailed(true);  // CORRUPTS _sysmask!
           interruptOnReceiveTimestampAvailable(false);
           interruptOnAutomaticAcknowledgeTrigger(true);
           // ...
       }
   }
   ```

3. **commitConfiguration()** writes corrupted mask
   ```cpp
   void DW1000Class::commitConfiguration() {
       writeSystemEventMaskRegister();  // Writes corrupted _sysmask to DW1000
       // ...
   }
   ```

### Impact
- The `_sysmask` buffer gets corrupted during `interruptOnReceiveFailed(true)`
- When `commitConfiguration()` writes this to the DW1000's SYS_MASK register, interrupts are incorrectly configured
- Hardware interrupts never fire because the mask register has wrong values
- Callbacks never execute, causing the infinite loop behavior

---

## Library Interrupt Setup Analysis

### Interrupt Attachment (Arduino Side)
**File:** `DW1000.cpp`, lines 165-183

```cpp
void DW1000Class::begin(uint8_t irq, uint8_t rst) {
    delay(5);
    pinMode(irq, INPUT);  // Configure IRQ pin as input
    SPI.begin();

    #ifndef ESP8266
    SPI.usingInterrupt(digitalPinToInterrupt(irq));  // Prevents SPI conflicts
    #endif

    _rst = rst;
    _irq = irq;
    _deviceMode = IDLE_MODE;

    // Attach interrupt handler
    attachInterrupt(digitalPinToInterrupt(_irq), DW1000Class::handleInterrupt, RISING);
}
```

**Configuration:**
- Interrupt on RISING edge (active-high polarity)
- Handler: `DW1000Class::handleInterrupt`
- Arduino Uno: D2 = INT0 (fully supported)

### DW1000 Interrupt Mask Register
**File:** `DW1000Constants.h`, lines 101-104

```cpp
// system event mask register
// NOTE: uses the bit definitions of SYS_STATUS (below 32)
#define SYS_MASK 0x0E
#define LEN_SYS_MASK 4
```

**Important:** SYS_MASK uses same bit definitions as SYS_STATUS but is only 4 bytes, not 5!

### Interrupt Handler Flow
**File:** `DW1000.cpp`, lines 711-749

```cpp
void DW1000Class::handleInterrupt() {
    readSystemEventStatusRegister();  // Read SYS_STATUS

    if(isClockProblem() && _handleError != 0) {
        (*_handleError)();
    }
    if(isTransmitDone() && _handleSent != 0) {
        (*_handleSent)();
        clearTransmitStatus();
    }
    if(isReceiveDone() && _handleReceived != 0) {
        (*_handleReceived)();
        clearReceiveStatus();
        if(_permanentReceive) {
            newReceive();
            startReceive();
        }
    }
    // ... handle other interrupts ...
    clearAllStatus();
}
```

---

## DW1000Ranging Examples (Why They Might Work Better)

### Key Differences

1. **Uses DW1000Ranging wrapper**
   ```cpp
   // DW1000Ranging_TAG.ino
   DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
   DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
   DW1000Ranging.loop();  // Continuous polling in main loop
   ```

2. **Additional state management**
   - More robust error handling
   - Automatic retry mechanisms
   - State machine for ranging protocol
   - Continuous loop() processing

3. **Higher-level protocol**
   - Handles timing between devices
   - Manages device discovery/pairing
   - Better recovery from errors

**However:** DW1000Ranging also calls `setDefaults()`, so it suffers from the same bug!

---

## setDelay() Function Analysis

### Implementation
**File:** `DW1000.cpp`, lines 1108-1129

```cpp
DW1000Time DW1000Class::setDelay(const DW1000Time& delay) {
    if(_deviceMode == TX_MODE) {
        setBit(_sysctrl, LEN_SYS_CTRL, TXDLYS_BIT, true);
    } else if(_deviceMode == RX_MODE) {
        setBit(_sysctrl, LEN_SYS_CTRL, RXDLYS_BIT, true);
    } else {
        return DW1000Time();  // Idle mode: ignore delay
    }

    byte delayBytes[5];
    DW1000Time futureTime;
    getSystemTimestamp(futureTime);
    futureTime += delay;
    futureTime.getTimestamp(delayBytes);
    delayBytes[0] = 0;
    delayBytes[1] &= 0xFE;
    writeBytes(DX_TIME, NO_SUB, delayBytes, LEN_DX_TIME);

    futureTime.setTimestamp(delayBytes);
    futureTime += _antennaDelay;
    return futureTime;
}
```

### Potential Issues with setDelay()

1. **Timing Precision**
   - BasicSender uses 10ms delay: `DW1000Time deltaTime = DW1000Time(10, DW1000Time::MILLISECONDS);`
   - Very short for UWB timing - might cause transmission before radio ready
   - Recommended: 1-5ms minimum, or remove delay entirely for simple tests

2. **Mode Dependency**
   - Only works in TX_MODE or RX_MODE
   - Must call `newTransmit()` BEFORE `setDelay()`
   - BasicSender does this correctly

3. **Not the Primary Issue**
   - setDelay() implementation is correct
   - The interrupt bug prevents ANY callbacks regardless of timing

---

## Solutions and Workarounds

### Solution 1: Fix the Library Bug (RECOMMENDED)

**Edit:** `/DWS1000_UWB/lib/DW1000/src/DW1000.cpp`, lines 992-996

```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);   // Change LEN_SYS_STATUS -> LEN_SYS_MASK
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);    // Change LEN_SYS_STATUS -> LEN_SYS_MASK
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);    // Change LEN_SYS_STATUS -> LEN_SYS_MASK
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);   // Change LEN_SYS_STATUS -> LEN_SYS_MASK
}
```

**Testing:**
1. Apply fix
2. Re-upload BasicSender/BasicReceiver
3. Verify callbacks fire
4. Check serial output for continuous packet transmission/reception

---

### Solution 2: Polling Mode (WORKAROUND)

Instead of relying on interrupts, manually check for events in loop():

```cpp
// Modified BasicReceiver example
void loop() {
    // Manually poll for receive status
    DW1000.readSystemEventStatusRegister();

    if(DW1000.isReceiveDone()) {
        numReceived++;
        DW1000.getData(message);
        Serial.print("Received message ... #"); Serial.println(numReceived);
        Serial.print("Data is ... "); Serial.println(message);

        DW1000.clearReceiveStatus();

        // Restart receiver manually
        DW1000.newReceive();
        DW1000.setDefaults();
        DW1000.receivePermanently(true);
        DW1000.startReceive();
    }

    if(DW1000.isReceiveFailed()) {
        Serial.println("Error receiving");
        DW1000.clearReceiveStatus();

        // Restart receiver
        DW1000.newReceive();
        DW1000.setDefaults();
        DW1000.receivePermanently(true);
        DW1000.startReceive();
    }
}
```

**Pros:**
- Works around the interrupt bug
- No library modification needed
- Easier to debug

**Cons:**
- Higher CPU usage
- Potential timing issues
- May miss fast events
- Not recommended for production

---

### Solution 3: Minimal Interrupt Setup

Manually configure interrupts instead of using setDefaults():

```cpp
void setup() {
    Serial.begin(9600);

    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Manual configuration (avoid setDefaults)
    DW1000.newConfiguration();

    // Set only what we need
    DW1000.setDeviceAddress(6);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);

    // Manually enable only needed interrupts (skipping interruptOnReceiveFailed)
    DW1000.interruptOnReceived(true);   // OK - no bug
    DW1000.interruptOnSent(true);       // OK - no bug
    // DON'T call: DW1000.interruptOnReceiveFailed(true);  // Has the bug!

    DW1000.commitConfiguration();

    // Attach handlers
    DW1000.attachReceivedHandler(handleReceived);

    // Start receiver
    receiver();
}
```

**Pros:**
- Works around the bug without library changes
- More control over configuration

**Cons:**
- Need to manually configure everything
- Less robust (no receive-failed handling)
- More code to maintain

---

## Recommended Action Plan

### Immediate Fix (Development)
1. **Fix the bug in DW1000.cpp** (change 4 lines)
2. Test with BasicSender/BasicReceiver
3. Verify interrupt callbacks work correctly
4. Document the fix

### Long-term (Production)
1. Submit bug fix to library maintainers
2. Consider switching to maintained fork (if original is abandoned)
3. Implement comprehensive error handling
4. Add watchdog timer for hang detection

### Testing Checklist
- [ ] BasicSender continuously sends packets
- [ ] BasicReceiver continuously receives packets
- [ ] Serial output shows incrementing packet numbers
- [ ] handleSent() callback executes on sender
- [ ] handleReceived() callback executes on receiver
- [ ] Error callbacks work (test with disconnected antenna)
- [ ] Works reliably for >100 packets
- [ ] No memory leaks or crashes

---

## Additional Findings

### Arduino Uno Compatibility
- **IRQ Pin D2**: Fully compatible (INT0)
- **SPI**: Standard SPI pins work correctly
- **Interrupt Mode**: RISING edge is correct for DW1000
- **No known Arduino Uno-specific issues**

### Library Version Notes
- arduino-dw1000 v0.9 is relatively old (2015-2016)
- Consider checking for maintained forks:
  - thotro/arduino-dw1000 (original, may be unmaintained)
  - F-Army/arduino-dw1000-ng (newer fork)
  - leosayous21/DW1000 (ranging-focused fork)

### Alternative Approaches
If you continue to have issues after fixing the bug:

1. **Use DW1000Ranging** instead of raw DW1000 class
   - Higher-level API
   - Built-in ranging protocol
   - Better state management

2. **Implement hybrid approach**
   - Use interrupts for receive
   - Use polling for transmit status
   - Reduces CPU load vs pure polling

3. **Add debug output to interrupt handler**
   ```cpp
   void DW1000Class::handleInterrupt() {
       digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // Visual feedback
       // ... rest of handler
   }
   ```

---

## Conclusion

**Root Cause:** Buffer overrun in `interruptOnReceiveFailed()` function corrupts interrupt mask register.

**Fix:** Change `LEN_SYS_STATUS` to `LEN_SYS_MASK` in 4 places (lines 993-996 of DW1000.cpp).

**Confidence:** Very High - This is a clear buffer overrun bug that directly affects interrupt configuration.

**Impact:** Affects all examples that call `setDefaults()`, including BasicSender, BasicReceiver, and DW1000Ranging examples.

**Testing Required:** After applying fix, test all examples to ensure interrupts work correctly and no new issues introduced.

---

## References

- DW1000 User Manual v2.18 - Section 7.2.6 (System Event Mask Register)
- arduino-dw1000 library source code analysis
- Arduino Uno interrupt specifications (INT0/INT1)
- DW1000Constants.h - Register definitions and lengths
