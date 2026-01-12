# Arduino Upload Issue Resolution - 2026-01-11

## Problem Summary

**Issue**: Unable to upload TAG firmware to second Arduino on `/dev/ttyACM1`
**Error**: `avrdude: stk500_getsync() not in sync: resp=0x00/0x20`
**Root Cause**: Hardware issue with Arduino or USB connection on ACM1

---

## Diagnostic Process

### 1. Hardware Detection

**Devices Found**:
```
/dev/ttyACM0 - Arduino Uno (VID:PID=2341:0043) - ✅ WORKING
/dev/ttyACM1 - Arduino Uno (VID:PID=2341:0043) - ❌ UPLOAD FAILS
/dev/ttyUSB0 - CP2102N USB-UART Bridge         - ❌ NOT ARDUINO UNO
```

### 2. Upload Testing Results

| Port | Firmware | Result | Notes |
|------|----------|--------|-------|
| /dev/ttyACM0 | ANCHOR | ✅ SUCCESS | Consistently works |
| /dev/ttyACM0 | TAG | ✅ SUCCESS | Consistently works |
| /dev/ttyACM1 | ANCHOR | ❌ FAILED | stk500_getsync errors |
| /dev/ttyACM1 | TAG | ❌ FAILED | stk500_getsync errors |
| /dev/ttyUSB0 | TAG | ❌ FAILED | Wrong device type |

**Conclusion**: `/dev/ttyACM1` has a hardware or connection issue preventing uploads.

### 3. Error Analysis

**Typical Error Messages**:
```
avrdude: stk500_getsync() attempt 1 of 10: not in sync: resp=0x00
avrdude: stk500_recv(): programmer is not responding
avrdude: stk500_getsync() attempt 2 of 10: not in sync: resp=0x20
...
avrdude: stk500_getsync() attempt 10 of 10: not in sync: resp=0x00
*** [upload] Error 1
```

**What this means**:
- Bootloader not responding to upload requests
- DTR reset not triggering bootloader mode
- Communication timing issues
- Possible hardware failure

---

## Solution Implemented

### Wait-for-Start Feature

**Problem**: Even if both devices could be uploaded, continuous serial output from one device interferes with uploading to the other.

**Solution**: Modified firmware to wait for user input before starting ranging operations.

**Implementation** ([src/main.cpp](../../src/main.cpp)):

```cpp
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("========================================");
  Serial.println("DW1000 Ranging Test (Bug Fixed)");
  Serial.println(IS_ANCHOR ? "Mode: ANCHOR" : "Mode: TAG");
  Serial.println("Expected distance: 45.72 cm (18 inches)");
  Serial.println("========================================");

  // WAIT FOR USER INPUT BEFORE STARTING
  Serial.println(">>> Send any character to start ranging <<<");
  Serial.println("(This allows all devices to be uploaded first)");

  while (!Serial.available()) {
    delay(100);  // Wait for user input
  }

  // Clear input buffer
  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("[USER] Start command received!");

  // Continue with DW1000 initialization...
}
```

**Benefits**:
1. Devices can be uploaded in any order
2. No serial interference between uploads
3. User controls when ranging begins
4. Easy to reset and restart tests

---

## Workarounds for ACM1 Issue

### Option 1: Cable Swap Method (Recommended for Now)

**Procedure**:
1. Upload ANCHOR firmware
2. Unplug Arduino from ACM0
3. Plug in second Arduino to ACM0
4. Upload TAG firmware
5. Connect both Arduinos (can use any USB ports for power/serial)
6. Open two serial monitors
7. Send 'start' to both devices

**Advantages**:
- Uses known working port
- Both devices get programmed successfully
- Simple and reliable

**Disadvantages**:
- Manual cable swapping required
- Tedious for iterative development

### Option 2: Fix ACM1 Hardware

**Troubleshooting Steps**:

1. **Replug USB Cable**
   - Unplug ACM1 Arduino
   - Wait 5 seconds
   - Plug into different USB port
   - Check if now appears as working port

2. **Try Different USB Cable**
   - Some cables are charge-only (no data)
   - Use known working cable

3. **Check Arduino**
   - Look for physical damage
   - Check USB connector for bent pins
   - Test Arduino with LED blink (to verify bootloader)

4. **Re-burn Bootloader** (if needed)
   - Use Arduino-as-ISP method
   - See: [ARDUINO_UPLOAD_TROUBLESHOOTING.md](ARDUINO_UPLOAD_TROUBLESHOOTING.md)

### Option 3: Use Different Computer USB Port

**Issue**: Some USB hubs or ports have power/signal issues

**Solution**:
- Try direct motherboard USB port (not hub)
- Avoid USB 3.0 ports (try USB 2.0)
- Use rear panel ports (more stable power)

---

## Current Status

**Working Configuration**:
- ✅ ANCHOR firmware: Successfully uploaded and verified on /dev/ttyACM0
- ✅ TAG firmware: Successfully uploads to /dev/ttyACM0 (tested, works)
- ✅ Wait-for-start feature: Implemented and tested
- ❌ Second Arduino: Upload fails on /dev/ttyACM1

**Recommendation**: User should physically troubleshoot the ACM1 Arduino:
1. Unplug and replug USB cable
2. Try different USB port on computer
3. Try different USB cable
4. Check for physical damage

If ACM1 continues to fail, use **Cable Swap Method** to complete ranging tests.

---

## Scripts Created

### Upload Helper Script

Created `upload_both.sh` to assist with cable swap method:

```bash
#!/bin/bash
# Helper script for uploading both firmwares using cable swap

echo "==================================="
echo "DUAL ARDUINO UPLOAD (Cable Swap)"
echo "==================================="

echo ""
echo "Step 1: Upload ANCHOR"
echo "  - Ensure ONE Arduino is plugged into /dev/ttyACM0"
read -p "Press Enter when ready..."

# Set ANCHOR mode
sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR true/' src/main.cpp
pio run --target upload --upload-port /dev/ttyACM0

echo ""
echo "Step 2: Upload TAG"
echo "  - UNPLUG the ANCHOR Arduino"
echo "  - PLUG IN the second Arduino to /dev/ttyACM0"
read -p "Press Enter when ready..."

# Set TAG mode
sed -i 's/#define IS_ANCHOR true/#define IS_ANCHOR false/' src/main.cpp
pio run --target upload --upload-port /dev/ttyACM0

echo ""
echo "==================================="
echo "✅ Both devices programmed!"
echo "==================================="
echo ""
echo "Next steps:"
echo "1. Plug in both Arduinos (any USB ports for power)"
echo "2. Open two serial monitors (115200 baud)"
echo "3. Send any character to BOTH to start ranging"
echo ""
echo "Expected distance: 45.72 cm (18 inches)"
```

---

## Lessons Learned

### Multi-Arduino Development Best Practices

1. **Wait-for-Start Pattern**
   - Always implement user-triggered start for multi-device systems
   - Prevents serial interference during uploads
   - Makes testing more controllable

2. **Port Reliability**
   - Test each port individually before assuming both work
   - Some Arduino clones have flaky USB implementations
   - Genuine Arduino Uno (VID:PID=2341:0043) usually reliable

3. **Cable Quality Matters**
   - Many micro-USB cables are charge-only
   - Use cables known to work for data transfer
   - Keep spare verified cables

4. **Diagnostic Approach**
   - Systematically test one variable at a time
   - Test firmware on known working port first
   - Isolate hardware vs software issues early

---

## References

- [ARDUINO_UPLOAD_TROUBLESHOOTING.md](ARDUINO_UPLOAD_TROUBLESHOOTING.md) - Comprehensive upload troubleshooting guide
- [Arduino Forum - stk500_getsync Errors](https://forum.arduino.cc/t/avrdude-stk500-getsync-not-in-sync/394373)
- [PlatformIO Upload Issues](https://docs.platformio.org/en/latest/faq.html#platformio-udev-rules)

---

## Next Steps

**Immediate**:
1. User troubleshoots ACM1 Arduino (unplug/replug, different port)
2. If ACM1 still fails, use cable swap method
3. Complete dual ranging test with both devices

**Future**:
1. Document cable swap workflow in README
2. Create automated test scripts that accommodate single working port
3. Consider marking Arduino devices with labels (ANCHOR #1, TAG #1, etc.)
4. Test with ESP32 as backup (no bootloader sync issues)

---

**Document Created**: 2026-01-11 18:05
**Issue Status**: ⚠️ WORKAROUND AVAILABLE - Hardware issue with ACM1
**Impact**: Medium - Can still test with cable swap method
**Priority**: Low - Workaround is sufficient for current development phase
