# Multi-Node Swarm Test - Quick Start Guide

**5-Minute Setup for Experienced Users**

---

## Prerequisites

- 3-5× Arduino Uno + DW1000 shields
- PlatformIO installed
- Python 3 with pyserial, colorama

---

## Setup Steps

### 1. Configure and Upload (5 minutes)

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_08_multi_node_swarm

# Connect all Arduino boards via USB

# Interactive upload (recommended)
./upload_swarm.sh

# Follow prompts to assign Node IDs (1-5)
# Node 1 = Coordinator, Nodes 2-5 = Mobile
```

### 2. Start Monitoring (1 minute)

```bash
# Monitor all nodes with color-coded output
python3 monitor_swarm.py

# Or just show ranging matrix
python3 monitor_swarm.py --matrix
```

### 3. Collect Data (5-10 minutes)

Let the test run for 5-10 minutes. You should see:
- Color-coded output from each node
- CSV data: `timestamp,node_id,target_id,distance,rx_power`
- Ranging matrix updates
- Node statistics

Press **Ctrl+C** to stop.

### 4. Analyze Results (1 minute)

```bash
# Generate comprehensive report
python3 analyze_swarm_data.py logs/

# Generate plots
python3 analyze_swarm_data.py logs/ --plot

# Export to CSV
python3 analyze_swarm_data.py logs/ --export results.csv
```

---

## Expected Results

**Good Test:**
- ✓ All nodes detected and ranging
- ✓ Update rate: 1-2 Hz per node
- ✓ Error rate: <5%
- ✓ Stable distances (std dev <0.3m)
- ✓ Network health: GOOD

**Issues to Check:**
- Missing nodes: Check USB connections
- High error rate: Adjust TDMA timing or check antennas
- No ranging: Verify firmware upload and antenna connections

---

## Configuration Quick Reference

**Edit `config.h` before uploading:**

```cpp
#define NODE_ID 1              // Change for each node (1-5)
#define MAX_NODES 5            // Total nodes in swarm (3-5)
#define SLOT_DURATION_MS 150   // TDMA slot duration
#define ENABLE_TDMA true       // Enable collision prevention
```

**Tuning TDMA:**

| Nodes | Slot Duration | Update Rate |
|-------|---------------|-------------|
| 3     | 150ms         | 2.2 Hz      |
| 5     | 150ms         | 1.3 Hz      |
| 5     | 100ms         | 2.0 Hz      |

---

## Troubleshooting

| Problem | Quick Fix |
|---------|-----------|
| No serial output | Check baud rate (115200) and USB connection |
| Upload fails | Run `pio run --target clean` and retry |
| Nodes not ranging | Check antennas and reduce distance |
| High collision rate | Increase `SLOT_DURATION_MS` in config.h |
| Low update rate | Decrease `SLOT_DURATION_MS` |

---

## Command Cheat Sheet

```bash
# Upload firmware to all nodes
./upload_swarm.sh

# Monitor all nodes
python3 monitor_swarm.py

# Show ranging matrix only
python3 monitor_swarm.py --matrix

# Monitor specific devices
python3 monitor_swarm.py --devices /dev/ttyACM0 /dev/ttyACM1

# Analyze collected data
python3 analyze_swarm_data.py logs/

# Generate plots
python3 analyze_swarm_data.py logs/ --plot

# Export to CSV
python3 analyze_swarm_data.py logs/ --export results.csv

# Check individual node
pio device monitor --port /dev/ttyACM0 --baud 115200
```

---

## File Locations

- **Firmware**: `node_firmware.ino`
- **Config**: `config.h`
- **Logs**: `logs/node_X_timestamp.log`
- **Analysis**: `swarm_analysis.png`, `results.csv`
- **Full docs**: `README.md`

---

**Ready to test? Run `./upload_swarm.sh` to begin!**
