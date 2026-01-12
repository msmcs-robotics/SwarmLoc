# Quick Ranging Test - Manual Instructions

## Current Status

✅ **Bug Fix Applied**: The critical interrupt bug in `lib/DW1000/src/DW1000.cpp` (lines 993-996) is fixed
✅ **Firmware Uploaded**: Both devices have been programmed:
   - `/dev/ttyACM0` = **TAG** (will measure distance)
   - `/dev/ttyACM1` = **ANCHOR** (reference point)

## Physical Setup

**IMPORTANT**: How far apart are your radios right now?

The code in `src/main.cpp` is configured for:
- **Expected distance: 45.72 cm (18 inches)**

For best results:
1. Place the two Arduino+DW1000 modules about **18 inches (45 cm) apart**
2. Ensure clear line-of-sight between them
3. Avoid placing near metal objects or walls

## How to Test (Manual Method)

Since automated serial monitoring has permission issues, here's the manual method:

### Option 1: Using Arduino IDE Serial Monitor

1. Open Arduino IDE
2. Tools → Serial Monitor (or Ctrl+Shift+M)
3. Set baud rate to **115200**
4. Select port `/dev/ttyACM0` (TAG)
5. You should see:
   ```
   ========================================
   DW1000 Ranging Test (Bug Fixed)
   Mode: TAG
   Expected distance: 45.72 cm (18 inches)
   ========================================

   >>> Send any character to start ranging <<<
   (This allows all devices to be uploaded first)
   ```
6. Press Enter or type any character
7. You should see:
   ```
   [USER] Start command received!
   [INIT] Initializing DW1000...
   [INIT] Device initialized
   [TAG] Address: 7D:00:22:EA:82:60:3B:9C
   [TAG] Searching for anchors...
   [DEVICE] Found: 8217 at ...
   [RANGE] Range: 0.45 m (45 cm) from 8217
   [RANGE] Range: 0.46 m (46 cm) from 8217
   ...
   ```

8. Repeat for `/dev/ttyACM1` (ANCHOR) in a second Serial Monitor window

### Option 2: Using screen

Open two terminal windows:

**Terminal 1 (TAG)**:
```bash
screen /dev/ttyACM0 115200
# Press Enter when you see "Send any character to start"
```

**Terminal 2 (ANCHOR)**:
```bash
screen /dev/ttyACM1 115200
# Press Enter when you see "Send any character to start"
```

To exit screen: `Ctrl+A` then `K` then `Y`

### Option 3: Using minicom

```bash
# Install if needed
sudo apt-get install minicom

# Terminal 1
minicom -D /dev/ttyACM0 -b 115200

# Terminal 2
minicom -D /dev/ttyACM1 -b 115200
```

To exit minicom: `Ctrl+A` then `X`

### Option 4: Using pio device monitor

```bash
# Terminal 1 (TAG)
pio device monitor --port /dev/ttyACM0 --baud 115200

# Terminal 2 (ANCHOR)
pio device monitor --port /dev/ttyACM1 --baud 115200
```

## What to Look For

### ✅ SUCCESS Indicators:

1. **Device Discovery**:
   ```
   [DEVICE] Found: 8217 at ...
   ```

2. **Range Measurements**:
   ```
   [RANGE] Range: 0.45 m (45 cm) from 8217
   [RANGE] Range: 0.46 m (46 cm) from 8217
   ```

3. **Update Rate**: 1-5 measurements per second

4. **Reasonable Distance**:
   - If devices are 18 inches apart, expect: 0.40-0.50 m (40-50 cm)
   - Accuracy should be within ±10-20 cm before calibration

### ❌ PROBLEM Indicators:

1. **No Device Discovery**:
   ```
   [TAG] Searching for anchors...
   [No further output]
   ```
   - **Solution**: Reset both Arduinos (press reset button)
   - Check that both are powered on
   - Try moving them closer (< 1 meter apart)

2. **Garbled/Corrupted Output**:
   ```
   ====4ѶLQ%M׹>
   ```
   - **This is actually GOOD NEWS!** It means interrupts are firing
   - Devices are communicating but output is corrupted
   - Solution: Reset both devices and restart

3. **Stuck at Initialization**:
   ```
   [INIT] Initializing DW1000...
   [No further output]
   ```
   - Hardware connection issue
   - Check shield is properly seated
   - Try re-uploading firmware

## After Testing - Report Back

Please provide:

1. **Distance between radios**: _____ cm or inches
2. **Did you see range measurements?** YES / NO
3. **If YES**:
   - What distance was measured? _____ m
   - How stable were measurements? (changing a lot? fairly steady?)
   - Update rate? (how many per second?)
4. **If NO**:
   - What output did you see?
   - Any error messages?
   - Did devices find each other?

## Next Steps Based on Results

### If Ranging Works:
1. ✅ Mark bug fix as **VERIFIED**
2. Test at multiple distances (0.5m, 1m, 2m, 3m)
3. Calibrate antenna delay for ±10cm accuracy
4. Clean up codebase (remove unused files)
5. Proceed to multi-node testing

### If Ranging Doesn't Work:
1. Debug based on symptoms above
2. Check hardware connections
3. Verify bug fix is in place (`grep LEN_SYS_MASK lib/DW1000/src/DW1000.cpp`)
4. Try simpler BasicSender/BasicReceiver test first

---

**Current Code Location**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/src/main.cpp`
**Bug Fix Location**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000.cpp:993-996`
