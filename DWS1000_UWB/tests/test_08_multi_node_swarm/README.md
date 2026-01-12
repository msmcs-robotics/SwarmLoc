# Test 08: Multi-Node Swarm Test

**Project**: SwarmLoc - GPS-Denied Positioning System
**Hardware**: 3-5× Arduino Uno + DW1000 (PCL298336 v1.3 Shield)
**Date**: 2026-01-11
**Purpose**: Test and validate multi-node swarm functionality with 3-5 nodes

---

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Hardware Requirements](#hardware-requirements)
4. [Setup Instructions](#setup-instructions)
5. [Configuration Guide](#configuration-guide)
6. [Running the Test](#running-the-test)
7. [Interpreting Results](#interpreting-results)
8. [Troubleshooting](#troubleshooting)
9. [Advanced Usage](#advanced-usage)

---

## Overview

This test suite validates multi-node swarm functionality by deploying 3-5 nodes in a coordinated network. It proves that the system can:

- **Manage multiple nodes** with unique IDs and roles
- **Prevent collisions** using TDMA (Time Division Multiple Access)
- **Perform inter-node ranging** between all nodes
- **Calculate positions** using trilateration (when 3+ anchors available)
- **Detect and handle failures** gracefully

### Key Features

- **Unified firmware**: Single codebase for all nodes
- **Automatic role assignment**: Node 1 = Coordinator, Nodes 2-5 = Mobile
- **TDMA scheduling**: Prevents message collisions
- **CSV output**: Structured data for analysis
- **Real-time monitoring**: Multi-device serial monitor
- **Post-test analysis**: Comprehensive data analysis tools

---

## System Architecture

### Network Topology

```
Node 1 (Coordinator/Anchor)
    |
    | UWB Ranging
    |
    +---+---+---+---+
    |   |   |   |   |
  Node2 Node3 Node4 Node5
(Mobile Tags with TDMA)
```

### Role Assignment

| Node ID | Role        | Function                  | TDMA Slot |
|---------|-------------|---------------------------|-----------|
| 1       | Coordinator | Primary anchor (fixed)    | N/A       |
| 2       | Mobile      | Mobile tag                | 0         |
| 3       | Mobile      | Mobile tag                | 1         |
| 4       | Mobile      | Mobile tag                | 2         |
| 5       | Mobile      | Mobile tag                | 3         |

### TDMA Timing

```
Frame Duration: 750ms (for 5 nodes)
Slot Duration: 150ms per node

Time: 0     150   300   450   600   750
      |     |     |     |     |     |
Slot: | N2  | N3  | N4  | N5  |Idle | (repeat)
      |     |     |     |     |     |
```

**Benefits:**
- No message collisions
- Predictable update rate
- Fair bandwidth allocation
- Scalable to 5 nodes

---

## Hardware Requirements

### Minimum Setup (3 nodes)

- 3× Arduino Uno
- 3× DW1000 UWB modules (PCL298336 v1.3 Shield)
- 3× USB cables (USB-A to USB-B)
- 1× Computer with 3+ USB ports (or USB hub)
- Power supply for each board

### Recommended Setup (5 nodes)

- 5× Arduino Uno
- 5× DW1000 UWB modules
- 5× USB cables
- 1× Computer with 5+ USB ports
- Power supply or USB hub

### Optional

- Tripods or mounting hardware (for fixed anchor positions)
- Measuring tape (for ground truth measurements)
- Multi-port USB hub with individual power switches

---

## Setup Instructions

### Step 1: Hardware Preparation

1. **Assemble each node**:
   - Attach DW1000 shield to Arduino Uno
   - Verify all pins are properly connected
   - Connect antenna to DW1000 module

2. **Verify hardware**:
   - Run `test_01_chip_id` on each board to verify DW1000 is working
   - Expected output: `DW1000 Chip ID: DECA0130`

3. **Label each board**:
   - Use labels or tape to mark Node IDs (1-5)
   - This helps track which board has which firmware

### Step 2: Software Setup

1. **Install dependencies**:
   ```bash
   # PlatformIO (if not already installed)
   curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/get-platformio.py | python3

   # Python packages for monitoring and analysis
   pip3 install pyserial colorama matplotlib numpy
   ```

2. **Navigate to test directory**:
   ```bash
   cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_08_multi_node_swarm
   ```

3. **Review configuration**:
   - Open `config.h`
   - Check `MAX_NODES` setting (default: 5)
   - Adjust `SLOT_DURATION_MS` if needed (default: 150ms)

### Step 3: Upload Firmware

**Option A: Interactive Upload (Recommended)**

```bash
./upload_swarm.sh
```

The script will:
1. Detect all connected Arduino boards
2. Prompt you to assign Node ID (1-5) to each device
3. Update `config.h` with correct NODE_ID
4. Compile and upload firmware
5. Verify upload success

**Option B: Manual Upload**

For each node:

1. Edit `config.h`:
   ```cpp
   #define NODE_ID 1  // Change to 2, 3, 4, 5 for other nodes
   ```

2. Connect the Arduino for this node

3. Upload using PlatformIO:
   ```bash
   cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB
   pio run --target upload --environment uno --upload-port /dev/ttyACM0
   ```

4. Verify by opening serial monitor:
   ```bash
   pio device monitor --port /dev/ttyACM0 --baud 115200
   ```

   Expected output:
   ```
   ========================================
   SwarmLoc Multi-Node Swarm Test
   ========================================
   Node ID: 1
   Role: COORDINATOR
   ...
   ```

5. Repeat for each node

### Step 4: Physical Deployment

1. **Position Node 1 (Coordinator)**:
   - Place at a fixed location
   - Record position as (0, 0, 1.5m) for reference
   - Ensure clear line-of-sight to test area

2. **Position mobile nodes (2-5)**:
   - Place at test positions
   - Ensure they can "see" the coordinator
   - For best results, keep nodes 1-10 meters apart

3. **Connect all nodes to computer**:
   - Use USB hub if needed
   - Verify all nodes appear as `/dev/ttyACMx` or `/dev/ttyUSBx`

---

## Configuration Guide

### Basic Configuration (`config.h`)

**Required settings**:

```cpp
// Node ID - CHANGE FOR EACH NODE
#define NODE_ID 1  // 1=Coordinator, 2-5=Mobile

// Swarm size
#define MAX_NODES 5  // 3-5 nodes supported

// TDMA timing
#define SLOT_DURATION_MS 150  // 150ms works well for 5 nodes
```

**Optional settings**:

```cpp
// Position calculation
#define ENABLE_POSITION_CALC true
#define POSITION_UPDATE_MS 500

// Debug output
#define DEBUG_TDMA false
#define DEBUG_POSITION true

// Serial baud rate
#define SERIAL_BAUD 115200
```

### TDMA Tuning

The slot duration affects update rate and collision probability:

| Nodes | Slot Duration | Frame Duration | Update Rate |
|-------|---------------|----------------|-------------|
| 3     | 150ms         | 450ms          | 2.2 Hz      |
| 4     | 150ms         | 600ms          | 1.7 Hz      |
| 5     | 150ms         | 750ms          | 1.3 Hz      |
| 5     | 100ms         | 500ms          | 2.0 Hz      |
| 5     | 200ms         | 1000ms         | 1.0 Hz      |

**Recommendations**:
- Start with 150ms (default)
- Increase if you see collisions (lost messages)
- Decrease for faster updates (but risk collisions)

---

## Running the Test

### Quick Start

1. **Connect all nodes** to your computer via USB

2. **Start the monitor**:
   ```bash
   python3 monitor_swarm.py
   ```

   This will:
   - Detect all connected nodes
   - Display color-coded output from each node
   - Log data to separate files in `logs/` directory
   - Show real-time statistics

3. **Let it run** for 5-10 minutes to collect data

4. **Press Ctrl+C** to stop monitoring

5. **Analyze results**:
   ```bash
   python3 analyze_swarm_data.py logs/
   ```

### Monitoring Options

**Show ranging matrix only**:
```bash
python3 monitor_swarm.py --matrix
```

**Monitor specific devices**:
```bash
python3 monitor_swarm.py --devices /dev/ttyACM0 /dev/ttyACM1 /dev/ttyACM2
```

**No colored output**:
```bash
python3 monitor_swarm.py --no-color
```

### Expected Output

**During monitoring**:
```
[Node 1] timestamp,1,9B,2.34,63.2
[Node 2] timestamp,2,9A,2.35,64.1
[Node 1] timestamp,1,9C,5.67,58.4
[Node 3] timestamp,3,9A,5.68,59.2
...

========================================
RANGING MATRIX (distances in meters)
========================================
From\To   1         2         3         4         5
1         ---       2.34      5.67      ---       ---
2         2.35      ---       ---       ---       ---
3         5.68      ---       ---       ---       ---
...
```

**Status output** (press 'S' in any serial terminal):
```
========================================
STATUS
========================================
Node ID: 2
Role: MOBILE
Uptime: 120 s
Ranges: 45
Errors: 2
Free RAM: 1234 bytes

Valid Ranges:
  Node 1: 2.34 m (63.2 dBm)
  Node 3: 4.56 m (60.1 dBm)
========================================
```

---

## Interpreting Results

### Success Criteria

A successful test should show:

- ✓ **All nodes detected**: All nodes appear in ranging matrix
- ✓ **Regular ranging**: Each node reports ranges at expected rate
- ✓ **Low error rate**: <10% error rate
- ✓ **Stable distances**: Distance measurements stable over time
- ✓ **No collisions**: TDMA prevents message collisions

### Analyzing Results

Run the analysis tool:
```bash
python3 analyze_swarm_data.py logs/ --plot
```

**Key metrics**:

1. **Range Count**: Number of successful range measurements
   - Expected: >50 per node per minute
   - Low count indicates communication issues

2. **Error Rate**: Percentage of failed operations
   - Good: <5%
   - Fair: 5-10%
   - Poor: >10%

3. **Distance Statistics**:
   - Mean distance should match physical setup
   - Std deviation <0.3m indicates good accuracy

4. **Network Health**:
   - "GOOD": Error rate <5%, disconnect rate <10%
   - "FAIR": Error rate <10%, disconnect rate <20%
   - "POOR": Higher error/disconnect rates

### Common Issues

**Issue**: "Only X nodes detected"
- **Cause**: Not all nodes are connected or running
- **Fix**: Check USB connections, verify firmware upload

**Issue**: "High error rate on Node X"
- **Cause**: Interference, poor antenna, or timing issues
- **Fix**: Check antenna connection, adjust TDMA timing

**Issue**: "Asymmetric ranging"
- **Cause**: One-way communication failure
- **Fix**: Check antenna orientation, reduce distance

**Issue**: "Distance outliers"
- **Cause**: Multipath interference or clock drift
- **Fix**: Improve line-of-sight, calibrate DW1000

---

## Troubleshooting

### Hardware Issues

**Problem**: DW1000 not responding

```bash
# Test with chip ID check
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_01_chip_id
pio run --target upload --environment uno
pio device monitor --port /dev/ttyACM0 --baud 9600
```

Expected: `DW1000 Chip ID: DECA0130`

**Problem**: No serial output

- Check baud rate matches (115200)
- Verify USB connection
- Try different USB port
- Check power supply

**Problem**: Nodes not ranging

- Verify all nodes are running (check LEDs)
- Check antenna connections
- Reduce distance between nodes
- Check for physical obstructions

### Software Issues

**Problem**: Compilation fails

```bash
# Clean build
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB
pio run --target clean
pio run --environment uno
```

**Problem**: Upload fails

- Check device path (ls /dev/ttyACM*)
- Verify user permissions (sudo usermod -a -G dialout $USER)
- Try different USB cable
- Reset Arduino manually before upload

**Problem**: Wrong node ID showing

- Verify `config.h` was updated correctly
- Check NODE_ID define before upload
- Re-upload with correct configuration

### Performance Issues

**Problem**: Low update rate

- **Solution**: Reduce `SLOT_DURATION_MS` in config.h
- **Trade-off**: May increase collision risk

**Problem**: High collision rate

- **Solution**: Increase `SLOT_DURATION_MS`
- **Trade-off**: Reduces update rate

**Problem**: Memory errors

- **Solution**: Reduce `MAX_NODES` to match actual node count
- **Trade-off**: Cannot add more nodes without reflashing

---

## Advanced Usage

### Custom Anchor Positions

If you deploy Node 2 or 3 as fixed anchors (not mobile), update positions in `node_firmware.ino`:

```cpp
Position anchorPositions[MAX_NODES] = {
    {0.0, 0.0, 1.5, true, 0},     // Node 1 (coordinator)
    {10.0, 0.0, 1.5, true, 0},    // Node 2 (fixed anchor)
    {5.0, 10.0, 1.5, true, 0},    // Node 3 (fixed anchor)
    {0.0, 0.0, 0.0, false, 0},    // Node 4 (mobile)
    {0.0, 0.0, 0.0, false, 0}     // Node 5 (mobile)
};
```

### Exporting Data

**Export to CSV**:
```bash
python3 analyze_swarm_data.py logs/ --export results.csv
```

**Import into spreadsheet**:
- Open `results.csv` in Excel, LibreOffice, or Google Sheets
- Columns: timestamp, source_node, node_id, target_id, distance, rx_power

**Plot in Python**:
```python
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('results.csv')
df[df['node_id'] == 2]['distance'].plot()
plt.xlabel('Sample')
plt.ylabel('Distance (m)')
plt.title('Node 2 Distance over Time')
plt.show()
```

### Continuous Testing

**Run overnight test**:
```bash
# Start monitoring with nohup
nohup python3 monitor_swarm.py > monitor_output.txt 2>&1 &

# Check it's running
ps aux | grep monitor_swarm

# Stop after desired time
kill $(ps aux | grep monitor_swarm | awk '{print $2}')

# Analyze
python3 analyze_swarm_data.py logs/ --plot
```

### Comparing Configurations

Test different TDMA settings:

```bash
# Test 1: 150ms slots
# Edit config.h: SLOT_DURATION_MS = 150
./upload_swarm.sh --auto
python3 monitor_swarm.py > test_150ms.log &
sleep 300  # 5 minutes
killall python3
mv logs/ logs_150ms/

# Test 2: 100ms slots
# Edit config.h: SLOT_DURATION_MS = 100
./upload_swarm.sh --auto
python3 monitor_swarm.py > test_100ms.log &
sleep 300
killall python3
mv logs/ logs_100ms/

# Compare
python3 analyze_swarm_data.py logs_150ms/ > report_150ms.txt
python3 analyze_swarm_data.py logs_100ms/ > report_100ms.txt
diff report_150ms.txt report_100ms.txt
```

---

## File Structure

```
test_08_multi_node_swarm/
├── node_firmware.ino        # Main firmware (unified for all nodes)
├── config.h                 # Configuration header (edit before upload)
├── upload_swarm.sh          # Upload script (auto-configure and upload)
├── monitor_swarm.py         # Multi-device monitor (real-time display)
├── analyze_swarm_data.py    # Post-test analysis tool
├── README.md                # This file
├── logs/                    # Created by monitor_swarm.py
│   ├── node_1_timestamp.log
│   ├── node_2_timestamp.log
│   └── ...
└── upload_log.txt           # Created by upload_swarm.sh
```

---

## Next Steps

After successful swarm test:

1. **Validate accuracy**: Compare measured distances to ground truth
2. **Test movement**: Move mobile nodes and verify position tracking
3. **Test failures**: Disconnect one node and verify graceful degradation
4. **Scale up**: Add more nodes (if hardware permits)
5. **Integrate**: Connect to flight controller or navigation system

---

## References

- **Dual-Role Architecture**: `../docs/findings/DUAL_ROLE_ARCHITECTURE.md`
- **DW1000 Library**: `../../lib/DW1000/`
- **Ranging Best Practices**: `../docs/findings/DW1000_RANGING_BEST_PRACTICES.md`

---

## Support

For issues or questions:

1. Check [Troubleshooting](#troubleshooting) section
2. Review log files in `logs/` directory
3. Run analysis tool for diagnostic information
4. Check hardware connections and antenna placement

---

**Status**: Ready for Testing
**Last Updated**: 2026-01-11
**Tested With**: Arduino Uno + DW1000 (PCL298336 v1.3)
