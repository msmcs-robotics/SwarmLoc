# CRITICAL HARDWARE DISCOVERY

## Date: 2026-01-08
## Test: Test 1 - Chip ID Read

---

## FINDING: PCL298336 Contains DWM1000, NOT DWM3000!

### Test Results

**Expected Device ID**: `0xDECA0302` (DW3110 chip in DWM3000)
**Actual Device ID**: `0xDECA0130` (DW1000 chip in DWM1000)

```
=== Test 1: Chip ID ===
Device ID: 0xDECA0130
```

### What This Means

**THE HARDWARE IS DWM1000, NOT DWM3000!**

1. Your PCL298336 v1.3 shields contain **DWM1000 modules**
2. The chip inside is **DW1000** (Device ID: 0xDECA0130)
3. **NOT** DWM3000/DW3110 as documentation suggested

### Impact on Project

#### GOOD NEWS ✓
1. **Original library choice was CORRECT!**
   - `#include <DW1000.h>` is the RIGHT library
   - No need to use DWM3000 library
   - Arduino-dw1000 library by thotro is mature and well-tested

2. **Better Arduino Uno Support**
   - DW1000 has better Arduino library support
   - More examples and community code available
   - More likely to work on 16MHz Arduino Uno

3. **Proven Working**
   - Many successful Arduino Uno + DW1000 projects exist
   - TWR is proven to work with this hardware
   - Accuracy can reach 10cm (better than we thought for Uno)

#### Changes Needed
1. **Ignore DWM3000 research**
   - ESP32 migration guide still valid (works with DW1000 too)
   - But focus on DW1000 library now

2. **Use Correct Library**
   - Install: https://github.com/thotro/arduino-dw1000
   - NOT the DWM3000 library we cloned

3. **Update Original Code**
   - The original .ino files were using correct library
   - But had implementation bugs (incomplete TWR, no timestamps)
   - Need to fix those bugs, not change library

### Why the Confusion?

#### From Documentation Review:
The `overview_DW1000.md` file mentioned:
> "The Qorvo PCL298336 module shield (which appears to be the DWM3000EVB Arduino shield)"

**This was INCORRECT speculation** - likely based on:
- Similar product codes
- Both being Qorvo UWB shields
- Assumption that newer = DWM3000

#### Actual Hardware:
- **PCL298336 v1.3** = Arduino shield for **DWM1000**
- **DWM1000** = Module containing **DW1000** chip
- **DW1000** = First-generation UWB transceiver from Decawave

### Specifications (DW1000 - Actual Hardware)

**Chip**: DW1000 (IEEE 802.15.4-2011)
**Device ID**: 0xDECA0130
**Frequency**: 3.5-6.5 GHz (6 channels)
**Accuracy**: ±10 cm (with calibration)
**Library**: [arduino-dw1000](https://github.com/thotro/arduino-dw1000)
**Arduino Uno**: **PROVEN TO WORK** ✓

### Comparison: DW1000 vs DW3000

| Feature | DW1000 (Your Hardware) | DW3000 |
|---------|----------------------|---------|
| Chip ID | 0xDECA0130 | 0xDECA0302 |
| Standard | IEEE 802.15.4-2011 | IEEE 802.15.4z |
| Channels | 6 (3.5-6.5 GHz) | 8 (inc. 8 GHz) |
| Arduino Support | Excellent | Limited |
| Library Maturity | Mature | New/Experimental |
| Uno Compatibility | Proven | Questionable |
| Accuracy | ±10 cm | ±10 cm |

**Result**: DW1000 is BETTER for Arduino Uno!

### Corrected Project Plan

#### Phase 1: Use Original Library ✓
- Library: `arduino-dw1000` by thotro
- Install: Arduino Library Manager or GitHub
- Proven to work with Arduino Uno

#### Phase 2: Fix Original Code
The original `initiator.ino` and `responder.ino` files were using the **correct library** but had these bugs:

1. **Incomplete TWR Protocol**
   - Only POLL and POLL_ACK implemented
   - Missing RANGE and RANGE_REPORT messages
   - → Add missing messages

2. **No Timestamp Capture**
   - Variables declared but never populated
   - → Add `DW1000.getTransmitTimestamp()` calls
   - → Add `DW1000.getReceiveTimestamp()` calls

3. **Broken Interrupt Handlers**
   - `receivedAck` flag never set
   - → Properly handle RX done interrupt
   - → Set flags when messages received

4. **No Error Handling**
   - → Add timeouts
   - → Add status checking
   - → Add debug output

#### Phase 3: Test Incrementally
- Test 1: ✓ Chip ID confirmed (0xDECA0130)
- Test 2: GPIO and reset
- Test 3-5: Basic communication
- Test 6-7: Timestamp capture
- Test 8-9: Full TWR and distance calculation

### Library Installation

```bash
# Option 1: Arduino Library Manager
# Search for "DW1000" and install

# Option 2: Git Clone
cd ~/Arduino/libraries/
git clone https://github.com/thotro/arduino-dw1000.git DW1000

# Option 3: PlatformIO
# Add to platformio.ini:
lib_deps =
    https://github.com/thotro/arduino-dw1000.git
```

### Working Examples

The `arduino-dw1000` library includes:

- **BasicSender / BasicReceiver** - Simple TX/RX
- **RangingTag / RangingAnchor** - Two-Way Ranging
- **examples/DW1000Ranging** - Complete ranging system

These are **proven to work** on Arduino Uno!

### Updated Success Criteria

**Test 1**: ✓ **PASSED**
- SPI communication works
- Chip ID read successfully
- Device ID: 0xDECA0130 (DW1000 confirmed)

**Next Steps**:
1. Install arduino-dw1000 library
2. Test library examples (BasicSender/Receiver)
3. Fix original code using library properly
4. Implement complete TWR protocol
5. Achieve ±10-20 cm accuracy (realistic for Uno)

### Resources for DW1000

**Library**:
- https://github.com/thotro/arduino-dw1000

**Datasheet**:
- DW1000 User Manual (Decawave/Qorvo)

**Examples**:
- Library includes working TWR examples
- Many Arduino Uno projects online

**Forums**:
- Arduino Forum DW1000 discussions
- Decawave/Qorvo support forum

### Conclusion

This is **GOOD NEWS**!

Your hardware (DWM1000/DW1000) is actually:
- ✓ Better supported on Arduino Uno
- ✓ Has mature, working libraries
- ✓ Proven to achieve good accuracy
- ✓ More examples and documentation
- ✓ Your original library choice was correct!

The problem wasn't the hardware or library choice - it was the **implementation bugs** in the original code. Those are fixable!

We can now proceed with confidence that:
1. Arduino Uno CAN work with this hardware
2. The library exists and is proven
3. TWR will work if implemented correctly
4. Target accuracy (10-20cm) is achievable

---

**Status**: Critical hardware identification complete
**Next**: Install correct library and continue testing
**Confidence**: HIGH - We have the right hardware and library!
