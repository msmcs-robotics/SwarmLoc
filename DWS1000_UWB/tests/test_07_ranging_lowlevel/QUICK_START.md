# Test 07 Quick Start Guide

## TL;DR - Run the Test
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_ranging_lowlevel
./compile_and_upload.sh
```

## Manual Commands (if script doesn't work)

### 1. Compile
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_ranging_lowlevel
pio run -e tag
pio run -e anchor
```

### 2. Upload (adjust ports as needed)
```bash
pio run -e tag --target upload --upload-port /dev/ttyACM0
pio run -e anchor --target upload --upload-port /dev/ttyACM1
```

### 3. Monitor

**Terminal 1 - TAG:**
```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

**Terminal 2 - ANCHOR:**
```bash
pio device monitor --port /dev/ttyACM1 --baud 115200
```

## Expected ANCHOR Output
```
### DW1000-arduino-ranging-anchor ###
DW1000 initialized ...
Committed configuration ...
Device ID: DECA0130
Unique ID: XXXXXXXXXXXX
Network ID & Device Address: 10:1
Device mode: ...

Range: 1.23 m    RX power: -78.50 dBm    Sampling: 4.12 Hz
Range: 1.24 m    RX power: -78.45 dBm    Sampling: 4.15 Hz
Range: 1.22 m    RX power: -78.48 dBm    Sampling: 4.13 Hz
...
```

## Success Criteria
- ✓ ANCHOR shows continuous "Range:" messages
- ✓ Sampling rate 3-5 Hz
- ✓ RX power -60 to -85 dBm
- ✓ Range values relatively stable (±10cm)

## If It Doesn't Work
1. Check both devices show "DW1000 initialized"
2. Verify TAG is Device Address 2, ANCHOR is Address 1
3. Check antenna connections
4. Try swapping which Arduino runs which code
5. See TESTING_PROCEDURE.md for detailed troubleshooting

## Documentation
After testing, fill out RESULTS_TEMPLATE.md and update:
`/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/TEST_RESULTS.md`
