# Test 07: Low-Level Ranging Examples (RangingTag & RangingAnchor)

## Overview
This test evaluates the basic two-way ranging functionality using the original DW1000 library examples. These are lower-level examples compared to the DW1000Ranging library, giving us direct control over the ranging protocol.

## Test Files
- `src_tag/main.cpp` - RangingTag example (initiates ranging requests)
- `src_anchor/main.cpp` - RangingAnchor example (responds and computes range)
- `platformio.ini` - PlatformIO configuration
- `compile_and_upload.sh` - Automated compilation and upload script

## Configuration

### TAG Configuration
- **Device Address**: 2
- **Network ID**: 10
- **Pin Configuration**:
  - RST: Pin 9
  - IRQ: Pin 2
  - SS: Pin 10 (default SPI SS)
- **Mode**: MODE_LONGDATA_RANGE_LOWPOWER
- **Reply Delay**: 3000 microseconds

### ANCHOR Configuration
- **Device Address**: 1
- **Network ID**: 10
- **Pin Configuration**:
  - RST: Pin 9
  - IRQ: Pin 2
  - SS: Pin 10 (default SPI SS)
- **Mode**: MODE_LONGDATA_RANGE_LOWPOWER
- **Reply Delay**: 3000 microseconds
- **Algorithm**: Asymmetric two-way ranging

## Ranging Protocol

The two-way ranging protocol follows this sequence:

```
TAG                                    ANCHOR
 |                                        |
 |-------- POLL -----------------------> |  (Tag sends POLL)
 |                                        |  (Anchor receives POLL)
 | <------- POLL_ACK -------------------|  (Anchor sends POLL_ACK after 3ms delay)
 |  (Tag receives POLL_ACK)              |
 |-------- RANGE ----------------------> |  (Tag sends RANGE with timestamps)
 |                                        |  (Anchor receives RANGE)
 |                                        |  (Anchor computes distance)
 | <------- RANGE_REPORT ---------------|  (Anchor sends result)
 |  (Tag receives result)                |
 |                                        |
 [Repeat...]
```

## How to Run

### Method 1: Automated Script
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_ranging_lowlevel
./compile_and_upload.sh
```

### Method 2: Manual PlatformIO Commands

1. **Compile both sketches**:
   ```bash
   pio run -e tag
   pio run -e anchor
   ```

2. **Upload to devices**:
   ```bash
   # Upload TAG
   pio run -e tag --target upload --upload-port /dev/ttyACM0

   # Upload ANCHOR
   pio run -e anchor --target upload --upload-port /dev/ttyACM1
   ```

3. **Monitor serial output**:
   ```bash
   # In terminal 1 (TAG)
   pio device monitor -e tag

   # In terminal 2 (ANCHOR)
   pio device monitor -e anchor
   ```

## Expected Output

### TAG Serial Output
```
### DW1000-arduino-ranging-tag ###
DW1000 initialized ...
Committed configuration ...
Device ID: DECA0130
Unique ID: [Device-specific EUI]
Network ID & Device Address: 10:2
Device mode: [Mode details]
```

The TAG will continuously send POLL messages and process responses, but doesn't print ranging results.

### ANCHOR Serial Output
```
### DW1000-arduino-ranging-anchor ###
DW1000 initialized ...
Committed configuration ...
Device ID: DECA0130
Unique ID: [Device-specific EUI]
Network ID & Device Address: 10:1
Device mode: [Mode details]

Range: 1.23 m    RX power: -78.50 dBm    Sampling: 4.12 Hz
Range: 1.24 m    RX power: -78.45 dBm    Sampling: 4.15 Hz
Range: 1.22 m    RX power: -78.48 dBm    Sampling: 4.13 Hz
...
```

The ANCHOR displays:
- **Range**: Measured distance in meters
- **RX power**: Received signal power in dBm
- **Sampling**: Ranging frequency in Hz (typically 3-5 Hz)

## What to Observe

### Success Criteria
1. **Initialization**: Both devices show "DW1000 initialized" and device info
2. **Continuous Ranging**: ANCHOR displays range measurements continuously
3. **Stable Measurements**: Range values should be relatively consistent (±5-10cm variation)
4. **Good Signal**: RX power should be reasonable (-60 to -85 dBm for close range)
5. **Sampling Rate**: Should achieve 3-5 Hz with the 250ms timeout

### Failure Indicators
1. **No initialization**: "DW1000 initialized" not shown (hardware issue)
2. **No ranging output**: ANCHOR shows no "Range:" messages (communication failure)
3. **Only "RANGE_FAILED" messages**: Protocol errors or timing issues
4. **Wildly varying distances**: >50cm variation (noise, interference, or timing problems)
5. **Very low sampling rate**: <1 Hz (protocol timeouts, missed messages)

## Troubleshooting

### No Serial Output
- Check USB connections
- Verify correct serial port in platformio.ini
- Try pressing reset button on Arduino

### Devices Don't Communicate
- Verify both devices are on same Network ID (10)
- Check antenna connections
- Ensure proper power supply
- Verify pin connections (RST=9, IRQ=2, SS=10)

### Inconsistent Ranging
- Check for interference (WiFi, other UWB devices)
- Ensure stable power supply
- Verify antennas are not blocked
- Try adjusting device separation

### Compilation Errors
- Ensure DW1000 library is in `../../lib/DW1000/`
- Check PlatformIO installation: `pio --version`
- Verify board type in platformio.ini matches your Arduino

## Key Differences from DW1000Ranging Library

1. **Manual Protocol**: This implements the ranging protocol manually vs. library automation
2. **No Addressing Logic**: Simple device address comparison, no complex network management
3. **Direct Timestamp Access**: Manual handling of timestamps for TOF computation
4. **State Machine**: Explicit state management with expectedMsgId variable
5. **Simpler**: Fewer features but easier to understand and debug

## Testing Duration
Monitor for at least 2-3 minutes to:
- Verify consistent operation
- Measure average sampling rate
- Observe range stability
- Check for protocol failures or timeouts

## Documentation
Results should be documented in:
- `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/TEST_RESULTS.md`

Include:
- Success/failure status
- Measured ranges and sampling rates
- Any error messages or anomalies
- Comparison with previous tests (test_06_ranging)
