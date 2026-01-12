# Test 7: Dual-Role UWB Firmware Prototype

**Project**: SwarmLoc - GPS-Denied Positioning System
**Hardware**: Arduino Uno + DW1000 (PCL298336 v1.3 Shield)
**Date**: 2026-01-11
**Status**: Prototype - Not Tested

---

## Overview

This is a **proof-of-concept** dual-role firmware that allows a single Arduino+DW1000 node to switch between ANCHOR and TAG roles via serial commands. The firmware is designed for drone swarm applications where flexible role assignment is desired.

### Key Features

- **Role Switching**: Switch between ANCHOR and TAG roles via serial commands
- **Persistent Configuration**: Role stored in EEPROM, survives power cycles
- **TDMA Support**: Built-in time-slot management for multi-node operation
- **Status Monitoring**: Real-time status display and heartbeat messages
- **Simple Interface**: Single-character serial commands

### Design Philosophy

This firmware is designed as a **simple, understandable prototype** based on the 48KB research document `docs/findings/DUAL_ROLE_ARCHITECTURE.md`. It demonstrates the core concepts while acknowledging the technical limitations of the Arduino Uno platform.

---

## Architecture

### The Challenge: Library Limitations

The DW1000Ranging library **does not support dynamic role switching**. The role is fixed at initialization when calling `startAsAnchor()` or `startAsTag()`. There is no mechanism to change roles at runtime without reinitializing the entire library.

### The Solution: Reset-Based Role Switching

This firmware implements role switching through a **reset-and-reconfigure** approach:

1. User sends serial command to change role
2. Firmware saves new role to EEPROM
3. Arduino performs software reset
4. On reboot, firmware reads role from EEPROM and initializes accordingly

**Trade-offs:**
- **Pros**: Works with unmodified library, simple implementation, reliable
- **Cons**: Not true runtime switching, brief downtime during reset (~2 seconds)

### Memory Architecture

```
Arduino Uno SRAM: 2048 bytes
├─ Arduino core:           ~200 bytes
├─ DW1000 library state:   ~150 bytes
├─ Device array (4×74):     296 bytes
├─ Firmware variables:      ~150 bytes
├─ Stack:                   ~800 bytes
└─ Available margin:        ~452 bytes
```

**Memory Safety**: Only one role is active at a time, avoiding the memory overhead of dual-role operation.

---

## Usage Guide

### Hardware Setup

1. **Connect DW1000 shield to Arduino Uno**
   - Ensure proper seating on headers
   - Check power LED on shield

2. **Connect USB cable**
   - Port will appear as `/dev/ttyACM0` or `/dev/ttyACM1`

3. **Verify connections**
   - IRQ: Pin D2
   - RST: Pin D9
   - SS: Pin D10

### Uploading Firmware

```bash
# Navigate to test directory
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_dual_role

# Option 1: Using Arduino IDE
# - Open test_07_dual_role.ino
# - Select Board: Arduino Uno
# - Select Port: /dev/ttyACM0
# - Click Upload

# Option 2: Using PlatformIO (if configured)
pio run --target upload --upload-port /dev/ttyACM0
```

### Configuration

Before uploading, customize these settings in the firmware:

```cpp
// Node Configuration (make unique for each node)
const uint8_t NODE_ID = 1;  // Change for each node: 1, 2, 3, 4
const char* NODE_NAME = "Node01";

// Address Configuration (make unique per node)
const char* MY_ANCHOR_ADDRESS = "82:17:5B:D5:A9:9A:E2:9A";  // Change last byte
const char* MY_TAG_ADDRESS = "7D:00:22:EA:82:60:3B:9A";     // Change last byte

// TDMA Configuration (for multi-node operation)
#define NUM_NODES 4          // Total number of nodes in swarm
#define SLOT_DURATION_MS 150 // Time slot per node in milliseconds
```

**Important**: Each node MUST have unique addresses. Change the last byte (9A, 9B, 9C, 9D) for each node.

### Serial Commands

Connect to the serial port at **115200 baud**:

```bash
# Using screen
screen /dev/ttyACM0 115200

# Using minicom
minicom -D /dev/ttyACM0 -b 115200

# Using Python serial monitor
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

**Available Commands:**

| Command | Function | Notes |
|---------|----------|-------|
| `A` | Switch to ANCHOR role | Arduino will reset |
| `T` | Switch to TAG role | Arduino will reset |
| `S` | Show status | Display current configuration |
| `R` | Reset and restart | Manual software reset |
| `H` | Show help | Display command list |

**Example Session:**

```
SwarmLoc Dual-Role Firmware v1.0
Node: Node01
Node ID: 1
Current role: TAG

Starting as TAG (initiator/mobile)
Address: 7D:00:22:EA:82:60:3B:9A
TAG mode active - searching for anchors...
TDMA enabled - Slot 0 of 4

[User types 'S']

========================================
SYSTEM STATUS
========================================
Node Name: Node01
Node ID: 1
Current Role: TAG
Uptime: 45 seconds
Range Count: 12
Error Count: 0
Free RAM: 452 bytes

TDMA Configuration:
  Enabled: YES
  My Slot: 0 of 4
  Slot Duration: 150 ms
  Frame Duration: 600 ms
========================================

[User types 'A']

========================================
ROLE SWITCH REQUEST: ANCHOR
========================================
Saving ANCHOR role to EEPROM...
Role saved. Resetting Arduino...
Resetting in 3...
2...
1...

[Arduino resets]

SwarmLoc Dual-Role Firmware v1.0
Node: Node01
Node ID: 1
Current role: ANCHOR

Starting as ANCHOR (responder/fixed position)
Address: 82:17:5B:D5:A9:9A:E2:9A
ANCHOR mode active - waiting for tags...
Ready!
```

---

## TDMA Time Slot Management

### What is TDMA?

**Time Division Multiple Access (TDMA)** prevents message collisions when multiple TAG nodes try to range with the same ANCHOR nodes simultaneously.

### How It Works

```
Timeline (4 nodes, 150ms slots):
┌─────────┬─────────┬─────────┬─────────┐
│ 0-150ms │ 150-300 │ 300-450 │ 450-600 │
├─────────┼─────────┼─────────┼─────────┤
│ Node 1  │ Node 2  │ Node 3  │ Node 4  │
│ ACTIVE  │  IDLE   │  IDLE   │  IDLE   │
└─────────┴─────────┴─────────┴─────────┘
Cycle repeats every 600ms → 1.67 Hz per node
```

**Each TAG node:**
1. Waits for its assigned time slot
2. Performs ranging during its slot
3. Stays idle during other nodes' slots

**ANCHOR nodes:**
- Always active, respond to any TAG
- No TDMA needed (they only respond, never initiate)

### Configuration

```cpp
#define NUM_NODES 4          // Total number of TAG nodes
#define SLOT_DURATION_MS 150 // Time per slot
#define MY_SLOT_ID (NODE_ID - 1)  // Node 1 → Slot 0, Node 2 → Slot 1, etc.
```

**Performance Calculation:**

```
Frame Duration = NUM_NODES × SLOT_DURATION_MS
Update Rate = 1000 / Frame Duration (per node)

Examples:
- 2 nodes, 150ms slots → 300ms frame → 3.3 Hz per node
- 3 nodes, 150ms slots → 450ms frame → 2.2 Hz per node
- 4 nodes, 150ms slots → 600ms frame → 1.7 Hz per node
```

### When to Use TDMA

**Enable TDMA if:**
- Multiple TAG nodes (NUM_NODES > 1)
- All tags ranging with same anchors
- Collision avoidance needed

**Disable TDMA if:**
- Single TAG node
- Tags range with different anchors
- Using ANCHOR role (always responds)

---

## Testing Strategy

### Test 1: Single Node Role Switching (5 minutes)

**Objective**: Verify role switching works correctly

**Procedure:**
1. Upload firmware to one Arduino
2. Open serial monitor at 115200 baud
3. Verify TAG mode starts (default)
4. Send command 'A' to switch to ANCHOR
5. Wait for reset and verify ANCHOR mode
6. Send command 'T' to switch back to TAG
7. Verify TAG mode after reset

**Success Criteria:**
- [ ] Firmware boots in TAG mode by default
- [ ] Command 'A' saves ANCHOR role and resets
- [ ] Firmware boots in ANCHOR mode after reset
- [ ] Command 'T' switches back to TAG mode
- [ ] EEPROM persists role across power cycles

### Test 2: Anchor-Tag Pair Ranging (10 minutes)

**Objective**: Verify ranging works in both roles

**Setup:**
- Node 1: ANCHOR mode
- Node 2: TAG mode
- Distance: 1 meter apart

**Procedure:**
1. Configure Node 1 as ANCHOR (command 'A')
2. Configure Node 2 as TAG (command 'T')
3. Place nodes 1 meter apart
4. Monitor serial output from both nodes
5. Verify ranging messages appear
6. Measure update rate

**Success Criteria:**
- [ ] TAG discovers ANCHOR
- [ ] ANCHOR detects TAG
- [ ] Range measurements appear (~1.0m ± 0.3m)
- [ ] RX power values present (-60 to -100 dBm)
- [ ] Update rate ≥ 1 Hz

### Test 3: TDMA Multi-Tag Operation (15 minutes)

**Objective**: Verify TDMA prevents collisions

**Setup:**
- Node 1: ANCHOR mode
- Node 2: TAG mode, NODE_ID=1 (Slot 0)
- Node 3: TAG mode, NODE_ID=2 (Slot 1)
- All nodes 2 meters from ANCHOR

**Procedure:**
1. Configure firmware with NUM_NODES=2
2. Set Node 2: NODE_ID=1
3. Set Node 3: NODE_ID=2
4. Upload and start all nodes
5. Monitor serial outputs
6. Verify time-slotted ranging

**Success Criteria:**
- [ ] Both tags discover anchor
- [ ] No message collisions
- [ ] Tags range in turn (not simultaneously)
- [ ] Update rate: ~3.3 Hz per tag (2 nodes, 150ms slots)
- [ ] Stable operation for 5+ minutes

### Test 4: Long-Term Stability (60 minutes)

**Objective**: Verify firmware stability

**Setup:** 1 ANCHOR + 1 TAG, 2m distance

**Procedure:**
1. Start ranging
2. Monitor for 60 minutes
3. Record range count, error count, free RAM

**Success Criteria:**
- [ ] No firmware crashes or resets
- [ ] Free RAM stable (no memory leaks)
- [ ] Packet loss < 10%
- [ ] Range accuracy stable (std dev < 20cm)

---

## Design Decisions and Rationale

### Decision 1: Reset-Based Role Switching

**Problem**: DW1000Ranging library does not support dynamic role switching.

**Options Considered:**

| Option | Pros | Cons | Selected |
|--------|------|------|----------|
| **A: Reset-based** | Simple, reliable, works with library | Brief downtime | ✅ YES |
| B: Runtime switching | No downtime | Requires library rewrite | ❌ NO |
| C: Dual-role simultaneous | Flexible | Exceeds Arduino Uno memory | ❌ NO |

**Rationale**: Reset-based approach is the only feasible option on Arduino Uno without major library modifications. The 2-second reset downtime is acceptable for most drone swarm use cases where role changes are infrequent.

**See**: Research document Section 1.3 (Options for Implementation)

### Decision 2: EEPROM for Role Storage

**Problem**: Need to persist role selection across resets.

**Why EEPROM?**
- Non-volatile: Survives power cycles
- Simple API: `EEPROM.read()` and `EEPROM.write()`
- Already available: No additional libraries
- Low overhead: Only 2 bytes used

**Alternative Considered**: Hardcoded role (compile-time selection)
- **Rejected**: Requires recompiling firmware for each role change

### Decision 3: Single-Role Architecture

**Problem**: Arduino Uno has only 2KB SRAM.

**Memory Analysis:**
```
Dual-role firmware estimate:
├─ Tag protocol code:       ~200 bytes SRAM
├─ Anchor protocol code:    ~200 bytes SRAM
├─ Role switching logic:    ~100 bytes SRAM
├─ Duplicate device arrays: +296 bytes SRAM
────────────────────────────────────────
Total additional:           ~796 bytes SRAM

Single-role firmware (current): ~1596 bytes SRAM
Dual-role firmware (estimated): ~2392 bytes SRAM

Arduino Uno SRAM limit: 2048 bytes → DUAL-ROLE FAILS
```

**Rationale**: Single-role architecture keeps memory usage under control. For true dual-role operation, migrate to ESP32 (520KB SRAM).

**See**: Research document Section 1.3 (Memory Usage)

### Decision 4: TDMA Implementation

**Problem**: Multiple TAG nodes cause message collisions.

**Why TDMA?**
- **Simple**: Time-slot allocation prevents collisions
- **Deterministic**: Predictable update rates
- **Scalable**: Supports 2-6 nodes on Arduino Uno
- **Standard**: Industry-proven approach

**Alternative Considered**: CSMA/CA (Carrier Sense Multiple Access)
- **Rejected**: Requires carrier sensing, more complex, unpredictable delays

**Trade-off**: Update rate decreases as nodes increase (1.67 Hz for 4 nodes vs 5+ Hz for single node).

**See**: Research document Section 2.2 (Multi-Anchor Star)

### Decision 5: Serial Command Interface

**Problem**: How should users trigger role switches?

**Options Considered:**
1. **Serial commands** ✅ SELECTED
   - Pros: Simple, works from any terminal, no extra hardware
   - Cons: Requires USB connection

2. Hardware jumper (DIP switch)
   - Pros: No software needed
   - Cons: Requires extra hardware, not dynamic

3. Button press
   - Pros: User-friendly
   - Cons: Ambiguous (short press vs long press?)

**Rationale**: Serial commands provide the best balance of simplicity and functionality for a prototype. For production drones, integrate with flight controller commands.

---

## Known Limitations

### 1. Role Switching Requires Reset

**Limitation**: Cannot switch roles at runtime, must reset Arduino.

**Impact**: ~2 second downtime during role change.

**Workaround**: For runtime switching, migrate to ESP32 with custom protocol.

**See**: Research document Section 3.2 (Time-Division Role Switching)

### 2. TDMA Synchronization

**Limitation**: TDMA uses `millis()` for timing, not synchronized across nodes.

**Impact**: Potential slot drift over time, especially if nodes boot at different times.

**Workaround**:
- Resynchronize every 10 seconds
- For production, implement beacon-based synchronization

**Future Enhancement**: Add synchronization beacon from ANCHOR nodes.

### 3. Arduino Uno Memory Constraints

**Limitation**: Only 2KB SRAM limits device array to 4 devices max.

**Impact**: Cannot support large swarms (>4 anchors or tags).

**Workaround**: Migrate to ESP32 for swarms >6 nodes.

**See**: Research document Section 1.1 (Current Library Architecture)

### 4. No Dual-Role Simultaneous Operation

**Limitation**: Cannot act as ANCHOR for some peers and TAG for others.

**Impact**: Fixed topology (nodes are either anchors or tags, not both).

**Workaround**: Deploy dedicated anchor nodes and tag nodes.

**See**: Research document Section 3.4 (Can a Node Be Anchor for Some Peers?)

### 5. Library Interrupt Bug

**Limitation**: Known bug in DW1000.cpp line 992-996 may prevent interrupts.

**Impact**: Ranging may fail if bug is present.

**Fix**: Apply patch from `test_06_ranging/README.md`:
```cpp
// Change LEN_SYS_STATUS to LEN_SYS_MASK in lines 992-996
void DW1000Class::interruptOnReceiveFailed(boolean val) {
    setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);   // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);    // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);    // FIXED
    setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);   // FIXED
}
```

**Status**: Documented in `docs/findings/INTERRUPT_ISSUE_SUMMARY.md`

---

## Migration Path to ESP32

### When to Migrate

Migrate to ESP32 when:
- [ ] Swarm size exceeds 6 nodes
- [ ] Need >10 Hz update rate per node
- [ ] Require true runtime role switching
- [ ] Need WiFi/Bluetooth telemetry
- [ ] Want to implement advanced algorithms (Kalman filter, path planning)

### ESP32 Advantages

| Feature | Arduino Uno | ESP32 | Improvement |
|---------|-------------|-------|-------------|
| CPU | 16 MHz | 240 MHz | 15× faster |
| SRAM | 2 KB | 520 KB | 260× more |
| Flash | 32 KB | 4 MB | 125× more |
| WiFi/BT | No | Yes | Telemetry |
| FreeRTOS | No | Yes | Multi-tasking |
| Dual-Role | No | Yes | Runtime switching |

### ESP32 Implementation Sketch

```cpp
// Conceptual ESP32 dual-role firmware
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "DW1000Ranging.h"

enum NodeRole { ANCHOR, TAG, SWITCHING };
NodeRole currentRole = ANCHOR;

// Separate task for ranging
void rangingTask(void* parameter) {
    while (true) {
        if (currentRole != SWITCHING) {
            DW1000Ranging.loop();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// Separate task for role management
void roleManagerTask(void* parameter) {
    while (true) {
        if (shouldSwitchRole()) {
            currentRole = SWITCHING;
            // Stop current protocol
            // Reinitialize with new role
            // Resume operation
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    startAnchorRole();

    xTaskCreate(rangingTask, "Ranging", 4096, NULL, 1, NULL);
    xTaskCreate(roleManagerTask, "RoleManager", 4096, NULL, 1, NULL);
}

void loop() {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
```

**See**: Research document Section 7 (Migration to ESP32)

---

## Troubleshooting

### Problem: Firmware won't compile

**Symptom**: Build errors about missing files or libraries

**Solution**:
1. Check library path: `lib/DW1000/src/` exists
2. Verify `#include "DW1000Ranging.h"` path
3. Ensure SPI library is available

### Problem: No serial output

**Symptom**: Serial monitor shows no messages

**Solution**:
1. Check baud rate: must be 115200
2. Verify USB cable is data-capable (not charge-only)
3. Check port: `/dev/ttyACM0` or `/dev/ttyACM1`
4. Press Arduino reset button

### Problem: Role switch doesn't work

**Symptom**: Command 'A' or 'T' doesn't change role

**Solution**:
1. Check EEPROM initialization: Look for "First boot" message
2. Verify serial command received: Should print "ROLE SWITCH REQUEST"
3. Wait for full reset sequence (3 seconds)
4. Check serial monitor reconnects after reset

### Problem: No ranging messages

**Symptom**: Nodes initialize but don't range

**Solution**:
1. Apply interrupt bug fix (see Known Limitations #5)
2. Check distance: Nodes should be 0.5-10m apart
3. Verify addresses are unique (change last byte)
4. Confirm one node is ANCHOR, other is TAG
5. Check IRQ pin connection (D2)

### Problem: TDMA collisions

**Symptom**: Multiple tags seem to interfere

**Solution**:
1. Verify NODE_ID is unique per node
2. Check NUM_NODES matches actual number of tags
3. Ensure nodes boot at similar times (< 5 second difference)
4. Increase SLOT_DURATION_MS (try 200ms)

### Problem: Memory errors or crashes

**Symptom**: Firmware resets randomly, "Free RAM" shows low values

**Solution**:
1. Reduce NUM_NODES if possible
2. Disable TDMA if single tag
3. Remove debug Serial.print statements
4. Check for memory leaks in modified code

---

## File Structure

```
tests/test_07_dual_role/
├── test_07_dual_role.ino    # Main firmware (this file)
├── README.md                # This documentation
├── DESIGN.md                # Design decisions (you are here)
└── test_script.sh           # Automated test script (future)
```

---

## References

### Research Documentation

1. **DUAL_ROLE_ARCHITECTURE.md** (48KB research document)
   - Path: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/DUAL_ROLE_ARCHITECTURE.md`
   - Comprehensive analysis of dual-role implementation
   - Topology recommendations
   - Memory constraints
   - Migration path to ESP32

2. **INTERRUPT_ISSUE_SUMMARY.md**
   - Path: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/INTERRUPT_ISSUE_SUMMARY.md`
   - Known library bug documentation
   - Fix instructions

### Related Tests

- **Test 6**: DW1000Ranging (anchor/tag examples)
  - Path: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_06_ranging/`
  - Baseline anchor and tag firmware

### Library Documentation

- **DW1000Ranging Library**
  - Path: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/`
  - Source: arduino-dw1000 v0.9
  - Limitations documented in research

---

## Next Steps

### Immediate (This Session)

1. **Upload firmware** to test board
2. **Verify compilation** and basic operation
3. **Test role switching** (command 'A' and 'T')
4. **Document any issues** encountered

### Short-Term (Next Session)

1. **Test ranging** with anchor-tag pair
2. **Measure accuracy** at 1m, 2m, 3m distances
3. **Test TDMA** with 2-3 tag nodes
4. **Collect performance data** (update rate, packet loss)

### Long-Term (Future Work)

1. **Migrate to ESP32** for true dual-role
2. **Implement beacon synchronization** for TDMA
3. **Add WiFi telemetry** for ground station
4. **Integrate with drone flight controller**
5. **Field test** on actual drones

---

## Contributing

This is a research prototype. Improvements welcome:

1. **Report bugs**: Document symptoms and conditions
2. **Suggest enhancements**: Explain use case and benefits
3. **Share results**: Test data, accuracy measurements, field tests
4. **Port to ESP32**: Implement true dual-role firmware

---

## License

This firmware is part of the SwarmLoc project for GPS-denied drone positioning.

---

## Contact

**Project**: SwarmLoc
**Hardware**: Arduino Uno + DW1000 (PCL298336 v1.3)
**Date**: 2026-01-11
**Status**: Prototype - Ready for Testing

---

**End of Documentation**
