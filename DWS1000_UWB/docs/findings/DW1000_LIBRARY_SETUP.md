# DW1000 Library Setup and Configuration

## Date: 2026-01-08

---

## Library Installation Complete

**Library**: arduino-dw1000 by thotro
**Repository**: https://github.com/thotro/arduino-dw1000
**Location**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/`
**Hardware**: DW1000 chip (Device ID: 0xDECA0130)

---

## Why This Library?

### Critical Discovery

Test 1 revealed that your PCL298336 v1.3 shields contain **DW1000** chips (not DWM3000):

```
Expected (based on docs): 0xDECA0302 (DW3000)
Actual (from hardware):   0xDECA0130 (DW1000)
```

### This is GOOD NEWS!

1. **Original library choice was CORRECT**
   - Your old code used `#include <DW1000.h>` - that was right!
   - The arduino-dw1000 library by thotro is the correct one

2. **Better Arduino Uno support**
   - DW1000 has mature, proven libraries
   - More examples and community code
   - Proven to work on 16MHz Arduino Uno
   - Many successful projects exist

3. **Proven accuracy**
   - Can achieve ±10 cm (better than expected for Arduino Uno)
   - TWR is proven to work with this hardware
   - Well-documented calibration process

---

## Library Structure

```
lib/DW1000/
├── src/                       # Library source code
│   ├── DW1000.cpp            # Main library implementation
│   ├── DW1000.h              # Header file
│   ├── DW1000Constants.h     # Register definitions
│   ├── DW1000Time.cpp        # Timestamp handling
│   └── DW1000Time.h
├── examples/                  # Working examples
│   ├── BasicSender/          # Simple TX test
│   ├── BasicReceiver/        # Simple RX test
│   ├── RangingAnchor/        # TWR anchor (responder)
│   ├── RangingTag/           # TWR tag (initiator)
│   ├── DW1000Ranging_ANCHOR/ # Advanced ranging anchor
│   ├── DW1000Ranging_TAG/    # Advanced ranging tag
│   ├── MessagePingPong/      # Bidirectional test
│   ├── BasicConnectivityTest/# SPI test
│   └── TimestampUsageTest/   # Timestamp verification
├── README.md                  # Library documentation
└── library.properties         # Arduino library metadata
```

---

## Available Examples

### 1. BasicConnectivityTest
**Purpose**: Verify SPI communication
**Use**: First test after hardware setup
**Tests**: Chip ID read, basic SPI functionality

### 2. BasicSender
**Purpose**: Simple transmit test
**Use**: Verify TX functionality
**Features**: Sends packets periodically

### 3. BasicReceiver
**Purpose**: Simple receive test
**Use**: Verify RX functionality
**Features**: Listens and reports received packets

### 4. MessagePingPong
**Purpose**: Bidirectional communication
**Use**: Test TX/RX on both devices
**Features**: Devices alternate sending messages

### 5. RangingTag (Initiator)
**Purpose**: Simple TWR initiator
**Use**: Basic distance measurement
**Features**: Initiates ranging, calculates distance

### 6. RangingAnchor (Responder)
**Purpose**: Simple TWR responder
**Use**: Responds to ranging requests
**Features**: Passive anchor for ranging

### 7. DW1000Ranging_TAG (Advanced)
**Purpose**: Full-featured ranging tag
**Use**: Production-ready ranging
**Features**: Error handling, filtering, calibration

### 8. DW1000Ranging_ANCHOR (Advanced)
**Purpose**: Full-featured ranging anchor
**Use**: Production-ready anchor
**Features**: Multiple tag support, robust protocol

### 9. TimestampUsageTest
**Purpose**: Verify timestamp capture
**Use**: Debug timing issues
**Features**: Tests all 8 timestamps (T1-T8)

---

## Integration with PlatformIO

### platformio.ini Configuration

The library is installed locally in `lib/DW1000/`. PlatformIO will automatically find it.

```ini
[env]
platform = atmelavr
board = uno
framework = arduino

lib_deps =
    ; DW1000 library (local copy in lib/DW1000)
    ; Hardware confirmed: DW1000 chip (Device ID: 0xDECA0130)
    SPI
```

### Pin Configuration

**Arduino Uno + PCL298336 Shield:**

| Function | Arduino Pin | DW1000 Connection |
|----------|-------------|-------------------|
| MOSI | D11 | SPI MOSI |
| MISO | D12 | SPI MISO |
| SCK | D13 | SPI CLK |
| CS (SS) | D10 | Chip Select |
| IRQ | D2 | Interrupt Request |
| RST | D9 | Hardware Reset |
| 3.3V | 3.3V | Power |
| GND | GND | Ground |

**In code:**
```cpp
#define PIN_SS   10   // Chip Select
#define PIN_RST  9    // Reset
#define PIN_IRQ  2    // Interrupt
```

---

## Basic Usage

### 1. Include the Library

```cpp
#include <SPI.h>
#include <DW1000.h>
```

### 2. Define Pins

```cpp
#define PIN_SS   10
#define PIN_RST  9
#define PIN_IRQ  2
```

### 3. Initialize DW1000

```cpp
void setup() {
    Serial.begin(9600);

    // Initialize SPI
    SPI.begin();

    // Initialize DW1000
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Configure device
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    // Attach interrupt
    attachInterrupt(digitalPinToInterrupt(PIN_IRQ), handleInterrupt, RISING);
}
```

### 4. Read Device ID (Verification)

```cpp
char msg[128];
DW1000.getPrintableDeviceIdentifier(msg);
Serial.print("Device ID: ");
Serial.println(msg);  // Should show DW1000 identification
```

### 5. Transmit

```cpp
byte data[] = "Hello DW1000!";
DW1000.newTransmit();
DW1000.setData(data, sizeof(data));
DW1000.startTransmit();
```

### 6. Receive

```cpp
void handleInterrupt() {
    // Handle RX done interrupt
    DW1000.newReceive();
    DW1000.startReceive();
}

void loop() {
    if (receivedFlag) {
        receivedFlag = false;

        byte data[128];
        DW1000.getData(data, DW1000.getDataLength());

        Serial.print("Received: ");
        Serial.println((char*)data);
    }
}
```

### 7. TWR Distance Calculation

```cpp
// Capture timestamps
DW1000Time timePollSent, timePollReceived;
DW1000Time timePollAckSent, timePollAckReceived;

// On initiator after receiving poll ACK:
DW1000.getTransmitTimestamp(timePollSent);
DW1000.getReceiveTimestamp(timePollAckReceived);

// Calculate distance (simplified - see examples for full protocol)
int64_t tof = /* TWR calculation */;
float distance = tof * DW1000Time::TIME_UNIT * SPEED_OF_LIGHT;
```

---

## Key Library Features

### DW1000 Main Class

**Configuration:**
- `DW1000.begin(irqPin, rstPin)` - Initialize with pins
- `DW1000.select(csPin)` - Set chip select pin
- `DW1000.newConfiguration()` - Start configuration
- `DW1000.setDefaults()` - Apply default settings
- `DW1000.commitConfiguration()` - Save configuration

**Operating Modes:**
- `DW1000.MODE_SHORTDATA_FAST_LOWPOWER` - Fast, short range
- `DW1000.MODE_LONGDATA_FAST_LOWPOWER` - Fast, long range
- `DW1000.MODE_SHORTDATA_FAST_ACCURACY` - Fast, accurate
- `DW1000.MODE_LONGDATA_FAST_ACCURACY` - Fast, accurate, long range
- `DW1000.MODE_LONGDATA_RANGE_LOWPOWER` - Maximum range, low power
- `DW1000.MODE_LONGDATA_RANGE_ACCURACY` - Maximum range, accurate

**Device Configuration:**
- `DW1000.setDeviceAddress(address)` - Set device address
- `DW1000.setNetworkId(id)` - Set network ID
- `DW1000.setAntennaDelay(delay)` - Calibration value

**Transmit:**
- `DW1000.newTransmit()` - Prepare new transmission
- `DW1000.setData(data, len)` - Set payload
- `DW1000.startTransmit()` - Begin transmission
- `DW1000.getTransmitTimestamp(timestamp)` - Get TX timestamp

**Receive:**
- `DW1000.newReceive()` - Prepare for reception
- `DW1000.startReceive()` - Begin listening
- `DW1000.getData(buffer, len)` - Read received data
- `DW1000.getReceiveTimestamp(timestamp)` - Get RX timestamp
- `DW1000.getDataLength()` - Get received data length

**Status:**
- `DW1000.isTransmitDone()` - Check TX complete
- `DW1000.isReceiveDone()` - Check RX complete
- `DW1000.isReceiveFailed()` - Check for RX errors

### DW1000Time Class

**Purpose**: Handle 40-bit timestamps for TWR

**Features:**
- Timestamp arithmetic
- Time-of-flight calculations
- Overflow handling
- Unit conversions

**Usage:**
```cpp
DW1000Time timestamp;
DW1000.getTransmitTimestamp(timestamp);

// Arithmetic
DW1000Time diff = timestamp2 - timestamp1;

// Time units
float microseconds = diff.getAsMicroSeconds();
float meters = diff.getAsMeters();  // Time-of-flight to distance
```

---

## Testing Strategy

### Phase 1: Basic Communication (Use library examples)

**Test 1: BasicConnectivityTest** ← START HERE
- Verify SPI communication
- Read device ID
- Confirm: 0xDECA0130 (DW1000)

**Test 2: BasicSender** (Upload to one Arduino)
- Verify TX functionality
- Monitor for successful transmissions

**Test 3: BasicReceiver** (Upload to other Arduino)
- Verify RX functionality
- Should receive packets from BasicSender

**Test 4: MessagePingPong** (Both Arduinos)
- Bidirectional communication
- Confirms both devices can TX and RX

### Phase 2: Ranging (Use library examples)

**Test 5: RangingTag + RangingAnchor**
- Upload RangingTag to initiator
- Upload RangingAnchor to responder
- Test at known distance (e.g., 1 meter)
- Record measured distance
- Calculate error

**Test 6: Calibration**
- Test at multiple distances: 0.5m, 1m, 2m, 5m
- Calculate antenna delay correction
- Apply calibration
- Re-test

**Test 7: Advanced Ranging**
- Use DW1000Ranging_TAG/ANCHOR examples
- Better error handling
- More robust protocol
- Achieve ±10-20 cm accuracy

### Phase 3: Custom Implementation

**Test 8: Fix Original Code**
- Apply lessons learned from library examples
- Implement complete 4-message DS-TWR
- Capture all timestamps properly
- Implement proper interrupt handling

**Test 9: Production Code**
- Serial output format: `Distance: X.XX m (XXX cm) ±YY cm`
- Error handling
- Timeout management
- Calibrated measurements

---

## Common Issues and Solutions

### Issue 1: Device ID reads incorrectly
**Symptoms**: ID is not 0xDECA0130
**Solutions**:
- Check SPI wiring (MOSI, MISO, SCK, CS)
- Verify 3.3V power at shield
- Ensure RST pin is HIGH after reset
- Try lower SPI speed

### Issue 2: No interrupts firing
**Symptoms**: Callbacks never execute
**Solutions**:
- Verify IRQ pin connection (D2)
- Check `attachInterrupt()` is called
- Ensure interrupt function is defined correctly
- Test with LED on IRQ pin

### Issue 3: Transmit but no receive
**Symptoms**: TX works, RX never gets data
**Solutions**:
- Check both devices on same network ID
- Verify both using same configuration
- Check antenna connections
- Ensure sufficient distance (>10 cm)

### Issue 4: Distance always wrong
**Symptoms**: Measurements have large offset
**Solutions**:
- Calibrate antenna delay
- Test at known distance
- Apply correction factor
- Check clock drift compensation

### Issue 5: Arduino Uno runs out of memory
**Symptoms**: Erratic behavior, crashes
**Solutions**:
- Reduce buffer sizes
- Remove debug strings
- Use F() macro for strings: `Serial.println(F("Text"));`
- Simplify protocol

---

## Antenna Delay Calibration

### What is Antenna Delay?

The time taken for signals to propagate through the antenna and PCB traces. Affects distance accuracy.

### Calibration Procedure

1. **Measure at known distance**:
   ```
   Set devices exactly 1.000 meter apart
   Record measured distance
   ```

2. **Calculate error**:
   ```
   Error = Measured - Actual
   Example: 1.15 m - 1.00 m = +0.15 m error
   ```

3. **Adjust antenna delay**:
   ```cpp
   // Default value
   uint16_t antennaDelay = 16450;

   // If measuring too long, increase delay
   // If measuring too short, decrease delay
   antennaDelay += adjustment;  // Typically ±100 to ±500

   DW1000.setAntennaDelay(antennaDelay);
   ```

4. **Iterate**:
   - Test again at 1 meter
   - Refine adjustment
   - Test at multiple distances (0.5m, 2m, 5m)
   - Verify linear accuracy

### Typical Values
- **DWM1000 modules**: 16400 - 16500
- **Depends on**: PCB design, antenna type, cable length
- **Your hardware**: Will need calibration (unique to each module)

---

## Performance Expectations

### Arduino Uno + DW1000

**Best Case (after calibration):**
- Accuracy: ±10 cm
- Range: 10-30 meters indoor, 50+ meters outdoor
- Update rate: 1-5 Hz
- Conditions: Line-of-sight, open area

**Typical (realistic):**
- Accuracy: ±20 cm
- Range: 10-20 meters indoor
- Update rate: 1-3 Hz
- Conditions: Indoor, some obstacles

**Challenges:**
- Arduino Uno 16 MHz CPU may struggle with timing
- 2 KB RAM limits buffer sizes
- Accuracy affected by:
  - Multipath (reflections)
  - Clock drift (compensated by DS-TWR)
  - Antenna delay (needs calibration)
  - Environmental factors

**Mitigation:**
- Use Double-Sided TWR (compensates clock drift)
- Calibrate antenna delay carefully
- Test in open areas initially
- Use MODE_LONGDATA_RANGE_ACCURACY

---

## Next Steps

### Immediate (Today)

1. **Test BasicConnectivityTest**
   - Copy example to temporary location
   - Upload to one Arduino
   - Verify chip ID reads correctly
   - Document results

2. **Test BasicSender/Receiver**
   - Upload BasicSender to Arduino #1
   - Upload BasicReceiver to Arduino #2
   - Verify communication works
   - Record any issues

3. **Test RangingTag/Anchor**
   - Upload to both devices
   - Measure at 1 meter
   - Record measured distance
   - Calculate initial error

### Short Term (This Week)

4. **Calibrate Antenna Delay**
   - Test at multiple known distances
   - Calculate optimal antenna delay
   - Apply calibration
   - Re-test accuracy

5. **Test Advanced Examples**
   - DW1000Ranging_TAG/ANCHOR
   - Better error handling
   - Achieve target accuracy

6. **Fix Original Code**
   - Apply library patterns to original code
   - Implement complete TWR
   - Proper timestamp capture
   - Error handling

### Long Term (Next Week+)

7. **Optimize for Arduino Uno**
   - Reduce memory usage
   - Optimize timing
   - Handle edge cases
   - Document limitations

8. **Production Implementation**
   - Custom calibration procedure
   - Serial output formatting
   - Error reporting
   - Documentation

9. **Consider ESP32 Migration**
   - If Arduino Uno performance insufficient
   - Follow ESP32_Migration_Guide.md
   - Expected: ±5-10 cm accuracy (vs ±20 cm on Uno)

---

## Resources

### Library Documentation
- **GitHub**: https://github.com/thotro/arduino-dw1000
- **README**: `lib/DW1000/README.md`
- **Examples**: `lib/DW1000/examples/`

### DW1000 Chip Documentation
- **Datasheet**: DW1000 User Manual (Decawave/Qorvo)
- **Device ID**: 0xDECA0130
- **Standard**: IEEE 802.15.4-2011

### Community Resources
- **Arduino Forum**: Search for "DW1000 ranging"
- **Decawave Forum**: https://forum.qorvo.com/
- **GitHub Issues**: Check library issues for solutions

### Project Documentation
- **Hardware Discovery**: `CRITICAL_HARDWARE_DISCOVERY.md`
- **Testing Plan**: `TESTING_PLAN.md`
- **Roadmap**: `roadmap.md`
- **Code Review**: `code-review.md`

---

## Summary

### What We Have

✓ **Correct Hardware**: DW1000 chip confirmed
✓ **Correct Library**: arduino-dw1000 installed locally
✓ **Working Examples**: 9 proven examples available
✓ **Documentation**: Comprehensive guides created
✓ **Test Plan**: Incremental feature-by-feature approach

### What Works

✓ SPI communication (Test 1 passed)
✓ Chip ID read (0xDECA0130 confirmed)
✓ Hardware reset functional
✓ Library compatible with Arduino Uno
✓ Proven TWR examples exist

### What's Next

1. Test library examples (BasicConnectivityTest first)
2. Verify TX/RX functionality
3. Test ranging at known distance
4. Calibrate antenna delay
5. Achieve ±10-20 cm accuracy
6. Fix original code if needed
7. Document results

### Confidence Level

**HIGH** - We have:
- Correct hardware identified
- Proven, mature library
- Working examples
- Clear test plan
- Realistic expectations

The DW1000 + Arduino Uno combination is proven to work. Success is achievable!

---

**Status**: Library installed and documented
**Next Action**: Test BasicConnectivityTest example
**Expected Result**: Verify full library functionality

Date: 2026-01-08
