# Multi-Node Swarm Architecture

This document provides visual diagrams and detailed architecture information for the multi-node swarm test system.

---

## System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    MULTI-NODE SWARM SYSTEM                  │
│                                                             │
│  Hardware Layer:                                            │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐│
│  │Node 1  │  │Node 2  │  │Node 3  │  │Node 4  │  │Node 5  ││
│  │Arduino │  │Arduino │  │Arduino │  │Arduino │  │Arduino ││
│  │+ DW1000│  │+ DW1000│  │+ DW1000│  │+ DW1000│  │+ DW1000││
│  └───┬────┘  └───┬────┘  └───┬────┘  └───┬────┘  └───┬────┘│
│      │           │           │           │           │     │
│      └───────────┴───────────┴───────────┴───────────┘     │
│                          │                                  │
│                     UWB Ranging                             │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │            USB Serial Connection                      │  │
│  └──────────────────────────────────────────────────────┘  │
│                          │                                  │
│  Software Layer:                                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  monitor_swarm.py (Real-time monitoring)              │  │
│  │  - Multi-thread serial readers                        │  │
│  │  - Color-coded output                                 │  │
│  │  - CSV logging                                        │  │
│  │  - Ranging matrix display                             │  │
│  └──────────────────────────────────────────────────────┘  │
│                          │                                  │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  analyze_swarm_data.py (Post-test analysis)           │  │
│  │  - Parse logs                                         │  │
│  │  - Generate statistics                                │  │
│  │  - Detect issues                                      │  │
│  │  - Create visualizations                              │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Network Topology

### Physical Layout (Example 5-Node Setup)

```
                    Node 1 (Coordinator)
                   Position: (0, 0, 1.5m)
                          ┌─┐
                          │C│  C = Coordinator/Anchor
                          └┬┘
                           │
          ┌────────────────┼────────────────┐
          │                │                │
          │                │                │
     ┌────▼───┐       ┌────▼───┐       ┌───▼────┐
     │   M2   │       │   M3   │       │   M4   │
     └────────┘       └────────┘       └────────┘
     Node 2           Node 3           Node 4
   TDMA Slot 0      TDMA Slot 1      TDMA Slot 2

                    ┌────────┐
                    │   M5   │
                    └────────┘
                      Node 5
                   TDMA Slot 3

Legend:
  C  = Coordinator (fixed anchor, always active)
  M  = Mobile node (tags with TDMA scheduling)
  │  = UWB ranging capability
```

### Communication Pattern

```
Time-Division Multiple Access (TDMA) Scheduling:

Coordinator (Node 1): Always listening
Mobile Nodes (2-5):   Take turns ranging

Frame Timeline (150ms slots × 5 nodes = 750ms frame):

0ms      150ms    300ms    450ms    600ms    750ms
│        │        │        │        │        │
├────────┼────────┼────────┼────────┼────────┤
│ Node 2 │ Node 3 │ Node 4 │ Node 5 │  Idle  │ → Repeat
│  TX    │  TX    │  TX    │  TX    │  ---   │
└────────┴────────┴────────┴────────┴────────┘

During each slot, the active node:
1. Sends POLL to coordinator
2. Receives POLL_ACK from coordinator
3. Sends RANGE
4. Receives RANGE_REPORT with distance
5. Logs: timestamp,node_id,target_id,distance,rx_power
```

---

## Firmware Architecture

### State Machine (Mobile Node)

```
┌─────────────┐
│   STARTUP   │
│  Load config│
│  Init DW1000│
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ SYNC FRAME  │
│ Calculate   │
│ current slot│
└──────┬──────┘
       │
       ├────────────────┐
       │                │
       ▼                ▼
┌─────────────┐  ┌─────────────┐
│  MY SLOT    │  │ OTHER SLOT  │
│  Active     │  │  Idle       │
│  Ranging    │  │  delay(10)  │
└──────┬──────┘  └──────┬──────┘
       │                │
       └────────┬───────┘
                │
                ▼
         ┌─────────────┐
         │UPDATE STATS │
         │Print status │
         │LED blink    │
         └──────┬──────┘
                │
                └──────→ Loop
```

### Coordinator State Machine

```
┌─────────────┐
│   STARTUP   │
│  Load config│
│  Init DW1000│
│  As anchor  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   LISTEN    │
│  Wait POLL  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  RX POLL    │
│  From tag   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ TX POLL_ACK │
│  To tag     │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  RX RANGE   │
│  From tag   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  CALCULATE  │
│  Distance   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│TX RANGE_RPT │
│  To tag     │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   LOG DATA  │
│Update stats │
└──────┬──────┘
       │
       └──────→ Loop
```

---

## Data Flow

### Ranging Sequence Diagram

```
Mobile Node 2    Coordinator (Node 1)    Monitor PC
     │                  │                    │
     │                  │                    │
     ├─POLL─────────────>│                   │
     │                  │                   │
     │<────POLL_ACK──────┤                   │
     │                  │                   │
     ├─RANGE────────────>│                   │
     │                  │                   │
     │<──RANGE_REPORT────┤                   │
     │ (distance=2.34m) │                   │
     │                  │                   │
     ├─────────────CSV output───────────────>│
     │  "timestamp,2,1,2.34,63.2"           │
     │                  │                   │
     │                  ├─────CSV output────>│
     │                  │ "timestamp,1,2,..." │
     │                  │                   │
     │                  │                   │
     │                  │     ┌─────────────▼────────┐
     │                  │     │  monitor_swarm.py    │
     │                  │     │  - Parse CSV         │
     │                  │     │  - Update matrix     │
     │                  │     │  - Display colored   │
     │                  │     │  - Log to file       │
     │                  │     └──────────────────────┘
```

### CSV Data Format

```
Field Structure:
┌───────────┬─────────┬───────────┬──────────┬──────────┐
│ timestamp │ node_id │ target_id │ distance │ rx_power │
│  (ms)     │  (1-5)  │   (hex)   │   (m)    │  (dBm)   │
└───────────┴─────────┴───────────┴──────────┴──────────┘

Example:
12345,2,9A,2.34,63.2
12346,1,9B,2.35,64.1
12347,3,9A,5.67,58.4

timestamp: Milliseconds since boot
node_id:   Node generating this log entry
target_id: Address of ranging partner (hex)
distance:  Measured range in meters
rx_power:  Received signal strength in dBm
```

---

## Memory Layout (Arduino Uno)

```
┌────────────────────────── 2048 bytes SRAM ──────────────────────────┐
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Arduino Core & Libraries                    ~350 bytes      │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ DW1000 Library State                        ~200 bytes      │   │
│  │  - Device structures                                        │   │
│  │  - Protocol state machine                                   │   │
│  │  - Timestamp buffers                                        │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Range Data Array (MAX_NODES)                ~100 bytes      │   │
│  │  - 5 nodes × 20 bytes each                                  │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Position Data                                ~80 bytes      │   │
│  │  - Anchor positions (5 × 16 bytes)                          │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Application Variables                        ~100 bytes     │   │
│  │  - Statistics counters                                      │   │
│  │  - TDMA timing                                              │   │
│  │  - Status flags                                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Stack                                        ~800 bytes     │   │
│  │  (grows downward)                                           │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Free RAM                                     ~418 bytes     │   │
│  │  (safety margin)                                            │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘

Total Used: ~1630 bytes
Free:       ~418 bytes (20% margin)
```

---

## Tool Architecture

### monitor_swarm.py

```
┌─────────────────────────────────────────────────┐
│          monitor_swarm.py (Main Thread)         │
│                                                 │
│  ┌───────────────────────────────────────┐     │
│  │ Device Detection                      │     │
│  │ - Scan /dev/ttyACM*, /dev/ttyUSB*    │     │
│  │ - Filter Arduino devices              │     │
│  └───────────────┬───────────────────────┘     │
│                  │                             │
│                  ▼                             │
│  ┌───────────────────────────────────────┐     │
│  │ Thread Manager                        │     │
│  │ - Spawn reader thread per device      │     │
│  │ - Create output queue                 │     │
│  │ - Coordinate threads                  │     │
│  └───────────────┬───────────────────────┘     │
│                  │                             │
└──────────────────┼─────────────────────────────┘
                   │
     ┌─────────────┼─────────────┐
     │             │             │
     ▼             ▼             ▼
┌─────────┐   ┌─────────┐   ┌─────────┐
│Thread 1 │   │Thread 2 │   │Thread N │
│Device 1 │   │Device 2 │   │Device N │
│         │   │         │   │         │
│ ┌─────┐ │   │ ┌─────┐ │   │ ┌─────┐ │
│ │Read │ │   │ │Read │ │   │ │Read │ │
│ │Parse│ │   │ │Parse│ │   │ │Parse│ │
│ │Queue│ │   │ │Queue│ │   │ │Queue│ │
│ │Log  │ │   │ │Log  │ │   │ │Log  │ │
│ └─────┘ │   │ └─────┘ │   │ └─────┘ │
└────┬────┘   └────┬────┘   └────┬────┘
     │             │             │
     └─────────────┼─────────────┘
                   │
                   ▼
         ┌──────────────────┐
         │  Output Queue    │
         │ (Thread-safe)    │
         └─────────┬────────┘
                   │
                   ▼
         ┌──────────────────┐
         │ Display Thread   │
         │ - Color-code     │
         │ - Print to term  │
         └──────────────────┘
                   │
                   ▼
         ┌──────────────────┐
         │ Matrix Updater   │
         │ - Parse ranges   │
         │ - Update matrix  │
         │ - Display table  │
         └──────────────────┘
```

### analyze_swarm_data.py

```
┌─────────────────────────────────────────────────┐
│       analyze_swarm_data.py (Main)              │
│                                                 │
│  ┌───────────────────────────────────────┐     │
│  │ Load Logs                             │     │
│  │ - Scan log directory                  │     │
│  │ - Extract node IDs from filenames     │     │
│  │ - Read all .log files                 │     │
│  └───────────────┬───────────────────────┘     │
│                  │                             │
│                  ▼                             │
│  ┌───────────────────────────────────────┐     │
│  │ Parse Data                            │     │
│  │ - Extract CSV range lines             │     │
│  │ - Parse events (discovery, errors)    │     │
│  │ - Build data structures               │     │
│  └───────────────┬───────────────────────┘     │
│                  │                             │
│                  ▼                             │
│  ┌───────────────────────────────────────┐     │
│  │ Analysis Engine                       │     │
│  │                                       │     │
│  │  ┌─────────────────────────────┐     │     │
│  │  │ Ranging Matrix Generator    │     │     │
│  │  │ - Build N×N matrix          │     │     │
│  │  │ - Calculate mean/std        │     │     │
│  │  └─────────────────────────────┘     │     │
│  │                                       │     │
│  │  ┌─────────────────────────────┐     │     │
│  │  │ Statistics Calculator       │     │     │
│  │  │ - Per-node metrics          │     │     │
│  │  │ - Network-wide metrics      │     │     │
│  │  └─────────────────────────────┘     │     │
│  │                                       │     │
│  │  ┌─────────────────────────────┐     │     │
│  │  │ Issue Detector              │     │     │
│  │  │ - High error rates          │     │     │
│  │  │ - Asymmetric ranging        │     │     │
│  │  │ - Distance outliers         │     │     │
│  │  └─────────────────────────────┘     │     │
│  └───────────────┬───────────────────────┘     │
│                  │                             │
│                  ▼                             │
│  ┌───────────────────────────────────────┐     │
│  │ Output Generator                      │     │
│  │                                       │     │
│  │  ┌─────────────────────────────┐     │     │
│  │  │ Console Report              │     │     │
│  │  │ - Text tables               │     │     │
│  │  │ - Statistics summary        │     │     │
│  │  └─────────────────────────────┘     │     │
│  │                                       │     │
│  │  ┌─────────────────────────────┐     │     │
│  │  │ CSV Exporter (optional)     │     │     │
│  │  │ - Structured data export    │     │     │
│  │  └─────────────────────────────┘     │     │
│  │                                       │     │
│  │  ┌─────────────────────────────┐     │     │
│  │  │ Plot Generator (optional)   │     │     │
│  │  │ - Matplotlib charts         │     │     │
│  │  │ - 4 subplots                │     │     │
│  │  └─────────────────────────────┘     │     │
│  └───────────────────────────────────────┘     │
└─────────────────────────────────────────────────┘
```

---

## Deployment Scenarios

### Scenario 1: Indoor Lab Test (3 Nodes)

```
Lab Layout (10m × 10m room):

    0m          5m          10m
0m  ┌───────────┬───────────┐
    │           │           │
    │     M2    │    M3     │
    │   (2,8)   │  (8,8)    │
5m  ├───────────┼───────────┤
    │           │           │
    │           C1          │
    │         (5,2)         │
10m └───────────┴───────────┘

Nodes: 3 (minimum)
Update Rate: 2.2 Hz per node
Range: 2-10m
Purpose: Basic functionality validation
```

### Scenario 2: Outdoor Field Test (5 Nodes)

```
Field Layout (20m × 20m area):

    0m          10m         20m
0m  ┌───────────┬───────────┐
    │           │           │
    │    M2     │    M3     │
    │  (5,15)   │  (15,15)  │
10m ├───────────┼───────────┤
    │           │           │
    │     C1    │    M5     │
    │  (0,0)    │  (15,5)   │
    │           │           │
    │           M4          │
    │         (10,5)        │
20m └───────────┴───────────┘

Nodes: 5 (full swarm)
Update Rate: 1.3 Hz per node
Range: 5-20m
Purpose: Full swarm validation
```

---

## Performance Characteristics

### Timing Budget (Per Node)

```
TDMA Slot: 150ms
├─ Ranging Transaction: ~40ms
│  ├─ POLL: ~5ms
│  ├─ POLL_ACK: ~5ms
│  ├─ RANGE: ~5ms
│  └─ RANGE_REPORT: ~5ms
│  └─ Processing: ~20ms
├─ Position Update: ~10ms
├─ Serial Output: ~5ms
└─ Margin: ~95ms (63%)

Total: 150ms per slot
Frame (5 nodes): 750ms
Update Rate: 1.33 Hz
```

### Throughput Analysis

```
Data Per Range Measurement:
- CSV line: ~30 bytes
- 1.33 Hz × 5 nodes = 6.65 ranges/sec
- Throughput: ~200 bytes/sec per node
- Network: ~1 KB/sec total

Log File Size (1 hour test):
- 1 hour = 3600 seconds
- 6.65 ranges/sec × 3600 sec = 23,940 ranges
- 30 bytes × 23,940 = 718 KB per node
- 5 nodes × 718 KB = 3.6 MB total
```

---

## Testing Checklist

### Pre-Test

- [ ] Hardware assembled and labeled
- [ ] All antennas connected
- [ ] `validate_setup.sh` passes
- [ ] Firmware configured and uploaded
- [ ] Physical positions planned
- [ ] Line-of-sight verified

### During Test

- [ ] All nodes boot successfully
- [ ] Coordinator shows "waiting for mobile nodes"
- [ ] Mobile nodes discover coordinator
- [ ] Ranging data appearing in monitor
- [ ] No error messages or warnings
- [ ] TDMA slots cycling correctly
- [ ] LED indicators blinking

### Post-Test

- [ ] Log files created for all nodes
- [ ] Analysis report generated
- [ ] Ranging matrix complete
- [ ] Error rate <10%
- [ ] Network health GOOD or FAIR
- [ ] Results documented

---

**Last Updated**: 2026-01-11
**Version**: 1.0
