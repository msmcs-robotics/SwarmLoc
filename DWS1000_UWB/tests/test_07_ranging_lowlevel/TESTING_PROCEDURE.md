# Test 07 - Manual Testing Procedure

## Prerequisites
- 2 Arduino boards with DW1000 modules
- USB cables connected to both Arduinos
- PlatformIO CLI installed (`pio --version` to verify)

## Step-by-Step Testing Procedure

### Step 1: Verify Test Setup
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_ranging_lowlevel

# Verify files are present
ls -la
# Should see: src_tag/, src_anchor/, platformio.ini, compile_and_upload.sh

# Check library link
ls -la ../../lib/DW1000
# Should show DW1000 library directory
```

### Step 2: Compile TAG
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_ranging_lowlevel
pio run -e tag
```

**Expected Output:**
```
Processing tag (platform: atmelavr; board: uno; framework: arduino)
...
Checking size .pio/build/tag/firmware.elf
RAM:   [====      ]  XX.X% (used XXXX bytes from 2048 bytes)
Flash: [=====     ]  XX.X% (used XXXXX bytes from 32256 bytes)
========================= [SUCCESS] Took X.XX seconds =========================
```

**If compilation fails:**
- Check that DW1000 library exists at `../../lib/DW1000/`
- Verify platformio.ini has correct `lib_extra_dirs` setting
- Check that src_tag/main.cpp exists and is readable

### Step 3: Compile ANCHOR
```bash
pio run -e anchor
```

**Expected Output:** Similar to TAG compilation above

### Step 4: Identify Arduino Ports
```bash
# List available serial devices
ls -la /dev/ttyACM* /dev/ttyUSB* 2>/dev/null

# Or use PlatformIO device list
pio device list
```

**Typical Output:**
```
/dev/ttyACM0 - Arduino Uno
/dev/ttyACM1 - Arduino Uno
```

**Note the ports** - you'll need them for upload and monitoring.

### Step 5: Upload TAG Firmware
```bash
# Replace /dev/ttyACM0 with your actual TAG port
pio run -e tag --target upload --upload-port /dev/ttyACM0
```

**Expected Output:**
```
Uploading .pio/build/tag/firmware.hex
...
avrdude: XXXX bytes of flash written
...
========================= [SUCCESS] Took X.XX seconds =========================
```

### Step 6: Upload ANCHOR Firmware
```bash
# Replace /dev/ttyACM1 with your actual ANCHOR port
pio run -e anchor --target upload --upload-port /dev/ttyACM1
```

**Expected Output:** Similar to TAG upload above

### Step 7: Monitor Serial Output

**Option A: Use PlatformIO Monitor (Recommended)**

Open two terminals:

**Terminal 1 - TAG Monitor:**
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_ranging_lowlevel
pio device monitor --port /dev/ttyACM0 --baud 115200
```

**Terminal 2 - ANCHOR Monitor:**
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_ranging_lowlevel
pio device monitor --port /dev/ttyACM1 --baud 115200
```

**Option B: Use Screen**
```bash
# Terminal 1 - TAG
screen /dev/ttyACM0 115200

# Terminal 2 - ANCHOR
screen /dev/ttyACM1 115200

# To exit screen: Ctrl+A, then K, then Y
```

### Step 8: Observe and Record Results

**Monitor for 2-3 minutes minimum**

#### TAG Expected Output:
```
### DW1000-arduino-ranging-tag ###
DW1000 initialized ...
Committed configuration ...
Device ID: DECA0130
Unique ID: [8-byte hex string]
Network ID & Device Address: 10:2
Device mode: [Mode configuration]
```
- TAG will be quiet after initialization
- It continuously sends POLL messages but doesn't print them

#### ANCHOR Expected Output:
```
### DW1000-arduino-ranging-anchor ###
DW1000 initialized ...
Committed configuration ...
Device ID: DECA0130
Unique ID: [8-byte hex string]
Network ID & Device Address: 10:1
Device mode: [Mode configuration]

Range: 1.23 m    RX power: -78.50 dBm    Sampling: 4.12 Hz
Range: 1.24 m    RX power: -78.45 dBm    Sampling: 4.15 Hz
Range: 1.22 m    RX power: -78.48 dBm    Sampling: 4.13 Hz
Range: 1.25 m    RX power: -78.52 dBm    Sampling: 4.10 Hz
...
```

### Step 9: Collect Test Data

Record the following information:

1. **Initialization Status**
   - [ ] TAG initialized successfully
   - [ ] ANCHOR initialized successfully
   - [ ] Device IDs displayed correctly
   - [ ] Network addresses correct (TAG=10:2, ANCHOR=10:1)

2. **Ranging Performance** (observe for 2-3 minutes)
   - [ ] ANCHOR displays continuous "Range:" messages
   - [ ] Range values are relatively stable
   - [ ] RX power values are reasonable (-60 to -85 dBm typical)
   - [ ] Sampling rate is 3-5 Hz

3. **Measurements** (record actual values)
   - Average distance: ________ m
   - Range variation: ±________ m
   - Average RX power: ________ dBm
   - Average sampling rate: ________ Hz
   - Actual distance between devices: ________ m

4. **Issues Observed**
   - [ ] No issues - ranging works perfectly
   - [ ] Intermittent ranging (gaps in output)
   - [ ] RANGE_FAILED messages
   - [ ] Wildly varying distances
   - [ ] Low sampling rate (<2 Hz)
   - [ ] No ranging output at all
   - Other: _________________________________

### Step 10: Test Variations (Optional)

If basic ranging works, try these variations:

1. **Distance Test**
   - Move devices to different distances (0.5m, 1m, 2m, 5m, 10m)
   - Record range accuracy at each distance

2. **Stability Test**
   - Keep devices stationary for 5 minutes
   - Record variation in measurements
   - Calculate standard deviation

3. **Obstruction Test**
   - Place obstacles between devices (metal, wood, water)
   - Observe effect on ranging and signal strength

4. **Interference Test**
   - Test near WiFi router, microwave, or other 2.4GHz sources
   - Observe effect on ranging stability

## Automated Script Alternative

Instead of manual steps 2-6, you can run:
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_ranging_lowlevel
./compile_and_upload.sh
```

This script will:
- Compile both TAG and ANCHOR
- Prompt for device ports
- Upload firmware to both devices
- Start monitoring ANCHOR output

## Troubleshooting

### Compilation Fails
```bash
# Check library path
ls ../../lib/DW1000/

# If missing, check main project lib
ls /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/

# Verify platformio.ini settings
cat platformio.ini | grep lib_extra_dirs
```

### Upload Fails
```bash
# Check device permissions
ls -la /dev/ttyACM*

# Add user to dialout group if needed
sudo usermod -a -G dialout $USER
# Then logout and login again

# Try resetting Arduino with reset button before upload
```

### No Serial Output
```bash
# Verify baud rate is 115200
pio device monitor --port /dev/ttyACM0 --baud 115200

# Try pressing reset button on Arduino while monitoring

# Check if device is actually connected
pio device list
```

### Devices Don't Range
1. **Check initialization on both devices**
   - Both should show "DW1000 initialized"
   - Both should show device info

2. **Verify configuration match**
   - TAG: Device Address 2, Network ID 10
   - ANCHOR: Device Address 1, Network ID 10

3. **Check hardware**
   - Verify antenna connections
   - Check power supply (should be stable 5V)
   - Verify SPI connections (MOSI, MISO, SCK, SS, IRQ, RST)

4. **Try swapping roles**
   - Upload ANCHOR code to the other Arduino
   - Upload TAG code to the first Arduino
   - This helps isolate hardware issues

## Next Steps

After completing this test:
1. Document results in TEST_RESULTS.md (see RESULTS_TEMPLATE.md)
2. Compare with test_06_ranging results
3. If this test works but test_06 doesn't, investigate DW1000Ranging library issues
4. If neither works, focus on hardware/wiring issues
