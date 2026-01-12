# Dual-Role Firmware Design Document

**Project**: SwarmLoc - GPS-Denied Positioning System
**Test**: test_07_dual_role
**Date**: 2026-01-11
**Version**: 1.0
**Status**: Design Complete - Implementation Ready

---

## Executive Summary

This document describes the design and implementation of a dual-role UWB firmware prototype for Arduino Uno + DW1000. The firmware allows nodes to switch between ANCHOR (responder/fixed) and TAG (initiator/mobile) roles via serial commands, enabling flexible swarm configurations.

**Key Innovation**: Reset-based role switching that works within Arduino Uno memory constraints and DW1000Ranging library limitations.

**Target Use Case**: Drone swarms requiring dynamic role assignment without hardware changes or firmware reflashing.

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Research Findings](#research-findings)
3. [Design Constraints](#design-constraints)
4. [Architecture Overview](#architecture-overview)
5. [Implementation Details](#implementation-details)
6. [Design Decisions](#design-decisions)
7. [Performance Analysis](#performance-analysis)
8. [Testing Strategy](#testing-strategy)
9. [Future Work](#future-work)

---

## 1. Problem Statement

### 1.1 The Challenge

**Scenario**: In a drone swarm using UWB ranging for GPS-denied positioning, some drones need to act as fixed reference points (ANCHORS) while others navigate freely (TAGS). The optimal configuration depends on mission requirements and may change over time.

**Current Limitation**: Existing firmware requires:
1. Separate anchor.ino and tag.ino files
2. Reflashing firmware to change roles
3. Physical access to each drone

**Desired Capability**:
- Single firmware that supports both roles
- Role switching via software command (no reflashing)
- Minimal memory overhead (Arduino Uno has only 2KB SRAM)
- Compatible with existing DW1000Ranging library

### 1.2 Research Context

This design is based on **48KB of research** documented in:
`/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/DUAL_ROLE_ARCHITECTURE.md`

**Key Finding**: The DW1000Ranging library **does not support dynamic role switching**. The role is fixed at initialization and cannot be changed at runtime.

### 1.3 Success Criteria

**Must Have:**
- [ ] Single firmware supports both ANCHOR and TAG roles
- [ ] Role switching via serial commands (no reflashing)
- [ ] Role persists across power cycles
- [ ] Memory usage < 1800 bytes SRAM (leave 250 byte margin)
- [ ] Works with unmodified DW1000Ranging library

**Should Have:**
- [ ] TDMA support for multi-tag operation
- [ ] Status monitoring and diagnostics
- [ ] Simple, understandable code structure

**Nice to Have:**
- [ ] Configuration storage for node ID, addresses
- [ ] Automatic role assignment based on criteria
- [ ] WiFi/Bluetooth remote configuration (ESP32 only)

---

## 2. Research Findings

### 2.1 Library Analysis

**File**: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000Ranging.cpp`

**Fixed-Role Architecture:**

```cpp
// Lines 162-223: Separate initialization functions
void DW1000RangingClass::startAsAnchor(...) {
    _type = ANCHOR;  // Set once, never changed
    // ... anchor-specific initialization
}

void DW1000RangingClass::startAsTag(...) {
    _type = TAG;     // Set once, never changed
    // ... tag-specific initialization
}

// Lines 515-678: Protocol state machine branches on _type
if(_type == ANCHOR) {
    // Anchor-specific message handling
} else if(_type == TAG) {
    // Tag-specific message handling
}
```

**Key Observation**: The `_type` variable is set once during initialization and never changed. There is no `switchRole()` or `setRole()` function.

### 2.2 Memory Constraints

**Arduino Uno SRAM Budget:**

```
Total SRAM: 2048 bytes

Fixed Overhead:
├─ Arduino core (Serial, etc):    ~200 bytes
├─ DW1000 library state:          ~150 bytes
├─ Device array (4 devices×74B):   296 bytes
└─ Stack:                          ~800 bytes
                                  ─────────
Total Fixed:                      ~1446 bytes

Available for Application:         ~600 bytes
```

**Device Array Memory**: Each device entry (anchor or tag) consumes 74 bytes:
- 8 bytes: address
- 6 timestamps × 8 bytes = 48 bytes
- 18 bytes: range, RX power, timeouts, etc.

**Conclusion**: Cannot maintain separate device arrays for both roles simultaneously. Must use single-role architecture.

### 2.3 Alternative Approaches Evaluated

| Approach | Memory (SRAM) | Complexity | Library Mods | Feasible? |
|----------|---------------|------------|--------------|-----------|
| **Runtime role switch** | ~2400 bytes | High | Required | ❌ NO (exceeds SRAM) |
| **Dual-role simultaneous** | ~2400 bytes | Very High | Required | ❌ NO (exceeds SRAM) |
| **Reset-based switch** | ~1600 bytes | Medium | None | ✅ YES |
| **Compile-time selection** | ~1600 bytes | Low | None | ⚠️ Limited (requires reflash) |

**Selected Approach**: Reset-based role switching (best trade-off)

---

## 3. Design Constraints

### 3.1 Hardware Constraints

**Arduino Uno Specifications:**
- CPU: ATmega328P @ 16 MHz
- Flash: 32 KB (30KB usable after bootloader)
- SRAM: 2 KB
- EEPROM: 1 KB
- GPIO: 14 digital pins, 6 analog inputs

**DW1000 Interface:**
- SPI: MOSI (D11), MISO (D12), SCK (D13), SS (D10)
- IRQ: D2 (hardware interrupt INT0)
- RST: D9 (reset pin)

### 3.2 Software Constraints

**DW1000Ranging Library Limitations:**
1. Role fixed at initialization (no dynamic switching)
2. Single-role protocol state machine
3. Device array sized for one role (anchors OR tags, not both)
4. Interrupt-driven architecture (requires proper ISR setup)

**Known Issues:**
- Interrupt bug in DW1000.cpp lines 992-996 (see test_06_ranging/README.md)
- Must use `LEN_SYS_MASK` not `LEN_SYS_STATUS`

### 3.3 Performance Constraints

**Target Performance:**
- Update rate: ≥2 Hz per node (500ms per ranging cycle)
- Ranging accuracy: ±20-50cm (Arduino Uno limitation)
- Role switch time: <5 seconds (acceptable for infrequent changes)
- Uptime: >1 hour continuous operation

**TDMA Scaling:**
```
Update Rate = 1000 ms / (NUM_NODES × SLOT_DURATION_MS)

Examples:
- 2 nodes × 150ms = 300ms → 3.3 Hz per node ✅
- 3 nodes × 150ms = 450ms → 2.2 Hz per node ✅
- 4 nodes × 150ms = 600ms → 1.7 Hz per node ✅
- 5 nodes × 150ms = 750ms → 1.3 Hz per node ⚠️
```

**Conclusion**: Support up to 4 TAG nodes with 150ms slots to maintain ≥1.7 Hz update rate.

---

## 4. Architecture Overview

### 4.1 System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Dual-Role Firmware                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌────────────┐     ┌─────────────┐     ┌──────────────┐  │
│  │  Serial    │────▶│ Role        │────▶│ EEPROM       │  │
│  │  Commands  │     │ Manager     │     │ Storage      │  │
│  │  (A/T/S/R) │     │             │     │              │  │
│  └────────────┘     └─────────────┘     └──────────────┘  │
│                            │                               │
│                            ▼                               │
│                     ┌─────────────┐                        │
│                     │  Software   │                        │
│                     │  Reset      │                        │
│                     └─────────────┘                        │
│                            │                               │
│                            ▼                               │
│  ┌──────────────────────────────────────────────────────┐ │
│  │              Boot Sequence                           │ │
│  │  1. Read role from EEPROM                           │ │
│  │  2. Initialize DW1000                               │ │
│  │  3. Call startAsAnchor() OR startAsTag()            │ │
│  │  4. Enter main loop                                 │ │
│  └──────────────────────────────────────────────────────┘ │
│                            │                               │
│         ┌──────────────────┴──────────────────┐           │
│         ▼                                      ▼           │
│  ┌────────────┐                         ┌────────────┐    │
│  │  ANCHOR    │                         │    TAG     │    │
│  │  Mode      │                         │   Mode     │    │
│  │            │                         │            │    │
│  │ - Respond  │                         │ - Initiate │    │
│  │ - Fixed    │                         │ - Mobile   │    │
│  │ - No TDMA  │                         │ - TDMA     │    │
│  └────────────┘                         └────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 State Machine

```
┌──────────────┐
│   BOOT       │
└──────┬───────┘
       │
       ▼
┌──────────────────┐
│ Read EEPROM Role │
└──────┬───────────┘
       │
       ├─── Role == ANCHOR ──────▶ ┌─────────────────┐
       │                           │  ANCHOR MODE    │
       │                           │  - Wait for TAGs│
       │                           │  - Respond      │◀─────┐
       │                           │  - Report Range │      │
       │                           └─────────┬───────┘      │
       │                                     │              │
       │                                     └──── loop ────┘
       │
       └─── Role == TAG ─────────▶ ┌─────────────────┐
                                    │  TAG MODE       │
                                    │  - Find ANCHORs │
                                    │  - TDMA Slot    │◀─────┐
                                    │  - Range        │      │
                                    └─────────┬───────┘      │
                                              │              │
                                              └──── loop ────┘

       [Serial Command 'A' or 'T']
                   │
                   ▼
       ┌───────────────────────┐
       │ Save Role to EEPROM   │
       └───────────┬───────────┘
                   │
                   ▼
       ┌───────────────────────┐
       │  Software Reset       │
       │  (asm jmp 0)          │
       └───────────┬───────────┘
                   │
                   └────────▶ (Back to BOOT)
```

### 4.3 TDMA Timing Diagram

```
Timeline for 4 TAG nodes (150ms slots):

Node 1 (Slot 0):
┌────────┬─────────┬─────────┬─────────┬────────┬
│ RANGE  │  IDLE   │  IDLE   │  IDLE   │ RANGE  │ ...
└────────┴─────────┴─────────┴─────────┴────────┴
0       150       300       450       600       750 (ms)

Node 2 (Slot 1):
┌─────────┬────────┬─────────┬─────────┬─────────┬
│  IDLE   │ RANGE  │  IDLE   │  IDLE   │  IDLE   │ ...
└─────────┴────────┴─────────┴─────────┴─────────┴
0       150       300       450       600       750 (ms)

Node 3 (Slot 2):
┌─────────┬─────────┬────────┬─────────┬─────────┬
│  IDLE   │  IDLE   │ RANGE  │  IDLE   │  IDLE   │ ...
└─────────┴─────────┴────────┴─────────┴─────────┴
0       150       300       450       600       750 (ms)

Node 4 (Slot 3):
┌─────────┬─────────┬─────────┬────────┬─────────┬
│  IDLE   │  IDLE   │  IDLE   │ RANGE  │  IDLE   │ ...
└─────────┴─────────┴─────────┴────────┴─────────┴
0       150       300       450       600       750 (ms)

Frame Duration: 600ms
Update Rate: 1.67 Hz per node
```

---

## 5. Implementation Details

### 5.1 Role Storage (EEPROM)

**Memory Layout:**
```
EEPROM Address Map:
┌──────┬────────────────────┬─────────────┐
│ Addr │ Content            │ Size        │
├──────┼────────────────────┼─────────────┤
│ 0x00 │ Role ('A' or 'T')  │ 1 byte      │
│ 0x01 │ Magic (0x42)       │ 1 byte      │
│ 0x02 │ Reserved           │ 1022 bytes  │
└──────┴────────────────────┴─────────────┘
```

**Code:**
```cpp
#define EEPROM_ROLE_ADDR 0
#define EEPROM_MAGIC_ADDR 1
#define EEPROM_MAGIC_VALUE 0x42

void saveRoleToEEPROM(NodeRole role) {
    EEPROM.write(EEPROM_ROLE_ADDR, (uint8_t)role);
}

NodeRole loadRoleFromEEPROM() {
    uint8_t roleValue = EEPROM.read(EEPROM_ROLE_ADDR);
    return (NodeRole)roleValue;
}
```

**First Boot Detection:**
```cpp
if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    // First boot - initialize
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    EEPROM.write(EEPROM_ROLE_ADDR, ROLE_TAG);  // Default
}
```

**Why Magic Byte?** Prevents using uninitialized EEPROM (all 0xFF) as valid role.

### 5.2 Software Reset Implementation

**Method**: Jump to address 0 (bootloader entry point)

```cpp
void resetAndRestart() {
    Serial.println("Resetting in 3...");
    delay(1000);
    Serial.println("2...");
    delay(1000);
    Serial.println("1...");
    delay(1000);

    // Software reset: Jump to bootloader
    asm volatile ("jmp 0");
}
```

**Alternative Methods Considered:**

| Method | Pros | Cons | Selected |
|--------|------|------|----------|
| **jmp 0** | Simple, reliable | Not "clean" reset | ✅ YES |
| Watchdog timer | Proper reset | Requires WDT config | ❌ NO |
| External reset pin | Cleanest | Needs extra hardware | ❌ NO |

**Rationale**: `jmp 0` is simplest and works reliably for role switching.

### 5.3 TDMA Slot Management

**Algorithm:**

```cpp
void loop() {
    if (tdmaEnabled && currentRole == ROLE_TAG) {
        uint32_t currentTime = millis();
        uint32_t timeInFrame = (currentTime - frameStartTime) % FRAME_DURATION_MS;
        uint8_t currentSlot = timeInFrame / SLOT_DURATION_MS;

        if (currentSlot == MY_SLOT_ID) {
            // My slot - perform ranging
            DW1000Ranging.loop();
        } else {
            // Not my slot - stay idle
            delay(10);
        }

        // Re-sync every 10 seconds
        if (currentTime - frameStartTime > 10000) {
            frameStartTime = currentTime;
        }
    } else {
        // ANCHOR or single TAG - always active
        DW1000Ranging.loop();
    }
}
```

**Key Features:**
1. Frame-based timing: All nodes share 600ms frame (4 nodes × 150ms)
2. Slot assignment: `MY_SLOT_ID = NODE_ID - 1` (0-indexed)
3. Periodic resync: Prevents drift from `millis()` wraparound
4. Idle behavior: `delay(10)` during other nodes' slots

**Synchronization Issue**: Nodes boot at different times, so frames are not aligned.

**Mitigation**: Resync every 10 seconds. For production, implement beacon-based sync.

### 5.4 Serial Command Processing

**Command Parser:**

```cpp
void processSerialCommand() {
    char cmd = Serial.read();

    // Convert to uppercase
    if (cmd >= 'a' && cmd <= 'z') {
        cmd = cmd - 32;
    }

    switch (cmd) {
        case 'A': // Anchor
            if (currentRole != ROLE_ANCHOR) {
                saveRoleToEEPROM(ROLE_ANCHOR);
                resetAndRestart();
            }
            break;

        case 'T': // Tag
            if (currentRole != ROLE_TAG) {
                saveRoleToEEPROM(ROLE_TAG);
                resetAndRestart();
            }
            break;

        case 'S': // Status
            printStatus();
            break;

        case 'R': // Reset
            resetAndRestart();
            break;

        case 'H': // Help
            printHelp();
            break;
    }
}
```

**Design Choices:**
- **Single character commands**: Fast to type, no parsing needed
- **Case insensitive**: User convenience
- **Immediate reset**: No confirmation dialog (keep it simple)

---

## 6. Design Decisions

### 6.1 Decision Matrix

| Decision Point | Options | Selected | Rationale |
|----------------|---------|----------|-----------|
| **Role switching method** | Runtime / Reset-based | **Reset-based** | Library limitation, memory |
| **Configuration storage** | Compile-time / EEPROM | **EEPROM** | Flexibility, no reflash |
| **TDMA synchronization** | Beacon / Free-running | **Free-running** | Simplicity, good enough |
| **Command interface** | Serial / Button / WiFi | **Serial** | Simplest for prototype |
| **Memory architecture** | Dual-role / Single-role | **Single-role** | SRAM constraint |

### 6.2 Decision Log

#### Decision 1: Reset-Based Role Switching

**Date**: 2026-01-11
**Context**: DW1000Ranging library does not support runtime role changes.
**Options**:
1. Rewrite library for runtime switching
2. Reset-based approach with EEPROM storage
3. Compile-time role selection

**Selected**: Option 2 (reset-based)

**Rationale**:
- Option 1 too complex (2-3 weeks development)
- Option 3 still requires reflashing (defeats purpose)
- Option 2 provides good trade-off: simple, works with library, ~2s downtime acceptable

**Trade-off**: 2-second downtime during role switch vs weeks of development.

**See**: Research doc Section 1.3

---

#### Decision 2: EEPROM for Persistent Storage

**Date**: 2026-01-11
**Context**: Need to store role across resets and power cycles.
**Options**:
1. EEPROM (Arduino built-in)
2. External SPI flash
3. SD card

**Selected**: Option 1 (EEPROM)

**Rationale**:
- Already available, no extra hardware
- Simple API (read/write single byte)
- 1KB capacity (only need 2 bytes)
- Wear leveling not a concern (infrequent writes)

**EEPROM Wear**: Rated 100,000 write cycles. Even with 100 role changes per day = 2.7 years lifetime.

---

#### Decision 3: Single-Role Memory Architecture

**Date**: 2026-01-11
**Context**: Arduino Uno has only 2KB SRAM.
**Options**:
1. Dual-role simultaneous (both protocols in memory)
2. Single-role (only one protocol active)

**Selected**: Option 2 (single-role)

**Rationale**:
- Dual-role would require ~2400 bytes SRAM (exceeds 2048 limit)
- Single-role uses ~1600 bytes (400 byte margin)
- No benefit to dual-role for reset-based switching

**Memory Budget**:
```
Single-role:   1600 bytes (✅ fits)
Dual-role:     2400 bytes (❌ exceeds limit)
```

**See**: Research doc Section 1.3 (Memory Usage)

---

#### Decision 4: Free-Running TDMA

**Date**: 2026-01-11
**Context**: Multiple TAG nodes need collision avoidance.
**Options**:
1. Beacon-based synchronization (ANCHOR broadcasts sync)
2. Free-running with periodic resync
3. No TDMA (collisions accepted)

**Selected**: Option 2 (free-running)

**Rationale**:
- Option 1 requires beacon protocol (adds complexity)
- Option 3 unacceptable (high collision rate)
- Option 2 simple, works for 4 nodes

**Limitation**: Slot drift if nodes boot at different times.

**Mitigation**: Resync every 10 seconds, acceptable for proof-of-concept.

**Future**: Implement beacon sync for production.

---

#### Decision 5: Serial Command Interface

**Date**: 2026-01-11
**Context**: How to trigger role switches.
**Options**:
1. Serial commands via USB
2. Button press (hardware)
3. WiFi/Bluetooth remote (ESP32)

**Selected**: Option 1 (serial commands)

**Rationale**:
- No extra hardware needed
- Works from any terminal/script
- Simple to implement
- Good for prototype testing

**Limitation**: Requires USB connection.

**Production**: Integrate with flight controller (MAVLink commands).

---

### 6.3 Open Questions

| Question | Current Answer | Future Work |
|----------|----------------|-------------|
| How to synchronize TDMA slots? | Free-running, resync every 10s | Implement beacon protocol |
| What if role switch fails? | Manual reset required | Add watchdog, error recovery |
| Can multiple nodes have same address? | No (undefined behavior) | Add address conflict detection |
| How to configure from flight controller? | Not implemented | Add MAVLink/UART interface |

---

## 7. Performance Analysis

### 7.1 Memory Usage

**Static Memory (Compile-Time):**

| Component | SRAM | Flash | Notes |
|-----------|------|-------|-------|
| Arduino core | 200 B | 4 KB | Serial, timers, etc |
| DW1000 library | 150 B | 10 KB | SPI, ranging protocol |
| Device array (4×74B) | 296 B | - | Max 4 anchors or tags |
| Firmware variables | 150 B | 2 KB | Globals, strings |
| Stack | 800 B | - | Function calls |
| **Total** | **1596 B** | **16 KB** | |
| **Available** | **452 B** | **14 KB** | Safety margin |

**Dynamic Memory (Runtime):**

```cpp
// Free RAM measurement
int freeRAM() {
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
```

**Expected**: ~450 bytes free during normal operation.

**Monitored**: Heartbeat message prints free RAM every 10 seconds.

### 7.2 Timing Analysis

**Role Switch Sequence:**

```
User types 'A'
  ↓ <10ms (Serial read)
Command parsed
  ↓ <1ms (EEPROM write)
Role saved
  ↓ 1000ms (delay for user message)
Reset countdown (3, 2, 1...)
  ↓ 3000ms (delays)
Jump to address 0
  ↓ <100ms (bootloader)
Firmware boots
  ↓ ~1500ms (DW1000 init)
New role active
──────────────────────
Total: ~5.6 seconds
```

**Breakdown:**
- Command processing: <10ms
- User notification: 4s (countdown)
- Boot and init: ~1.5s
- **Total downtime**: 5-6 seconds

**Acceptable**: Role switches are infrequent in swarm operation.

### 7.3 TDMA Performance

**Update Rate Calculation:**

```
Frame Duration = NUM_NODES × SLOT_DURATION_MS
Update Rate = 1000 / Frame Duration (per node)

Examples:
- 1 node (no TDMA):    ~5 Hz
- 2 nodes × 150ms:    3.3 Hz
- 3 nodes × 150ms:    2.2 Hz
- 4 nodes × 150ms:    1.7 Hz
```

**Latency:**

Worst-case latency (Tag waiting for slot):
```
Max Wait = (NUM_NODES - 1) × SLOT_DURATION_MS

Examples:
- 2 nodes: 150ms
- 3 nodes: 300ms
- 4 nodes: 450ms
```

**Throughput:**

```
Messages per second = NUM_NODES × (1000 / FRAME_DURATION_MS)

Examples:
- 2 nodes: 6.6 messages/s
- 3 nodes: 6.6 messages/s
- 4 nodes: 6.6 messages/s

(Throughput constant because all nodes transmit in parallel frame)
```

---

## 8. Testing Strategy

### 8.1 Unit Tests

| Test | Objective | Pass Criteria |
|------|-----------|---------------|
| **EEPROM Read/Write** | Verify role storage | Role persists across resets |
| **Serial Command Parser** | Verify command recognition | All commands (A/T/S/R/H) work |
| **TDMA Slot Calculation** | Verify timing math | Correct slot for each node ID |
| **Free RAM Measurement** | Verify memory tracking | Reasonable value (400-500B) |

### 8.2 Integration Tests

| Test | Setup | Expected Result |
|------|-------|-----------------|
| **Single Node Boot** | 1 node, default role | Boots as TAG (default) |
| **Role Switch A→T** | Start ANCHOR, send 'T' | Resets, becomes TAG |
| **Role Switch T→A** | Start TAG, send 'A' | Resets, becomes ANCHOR |
| **Power Cycle** | Switch role, unplug, replug | Role persists |
| **TDMA Single Tag** | TAG with NUM_NODES=1 | TDMA disabled, always active |
| **TDMA Multi-Tag** | 2 TAGs, NUM_NODES=2 | Tags range in turns |

### 8.3 System Tests

| Test | Setup | Duration | Success Criteria |
|------|-------|----------|------------------|
| **Anchor-Tag Pair** | 1 ANCHOR + 1 TAG, 1m apart | 5 min | Range ~1.0m ± 0.3m |
| **Multi-Anchor** | 3 ANCHORs + 1 TAG | 10 min | Tag ranges with all 3 |
| **TDMA 4-Node** | 3 ANCHORs + 3 TAGs | 30 min | No collisions, all tags range |
| **Long-Term Stability** | 1 ANCHOR + 1 TAG | 60 min | No crashes, <10% packet loss |

---

## 9. Future Work

### 9.1 Short-Term Enhancements

**Priority 1: Beacon-Based TDMA Sync**

Currently: Free-running slots, nodes not synchronized

**Enhancement**: ANCHOR broadcasts sync beacon every frame

```cpp
// Anchor: Broadcast sync beacon
void broadcastSyncBeacon() {
    // Send timestamp + frame start marker
}

// Tag: Receive sync beacon
void onSyncBeacon(uint32_t timestamp) {
    frameStartTime = millis() - (timestamp % FRAME_DURATION_MS);
}
```

**Benefit**: Nodes synchronize slots, eliminate collision from boot timing.

**Priority 2: Address Conflict Detection**

Currently: Undefined behavior if two nodes have same address

**Enhancement**: Check for address conflicts at boot

```cpp
void detectAddressConflict() {
    // Listen for own address on air
    // If detected → collision, change address or alert user
}
```

**Priority 3: Watchdog Timer**

Currently: Manual reset if firmware hangs

**Enhancement**: Watchdog timer for automatic recovery

```cpp
#include <avr/wdt.h>

void setup() {
    wdt_enable(WDTO_8S);  // 8 second timeout
}

void loop() {
    wdt_reset();  // Pet the dog
    // ... normal operation
}
```

### 9.2 Medium-Term: ESP32 Port

**Target**: True runtime role switching without reset

**Hardware**: ESP32 + DWM1000 module

**Architecture**:
```cpp
// ESP32 FreeRTOS implementation
xTaskCreate(rangingTask, ...);      // Dedicated ranging task
xTaskCreate(roleManagerTask, ...);  // Dedicated role manager

void roleManagerTask() {
    while (true) {
        if (shouldSwitchRole()) {
            suspendTask(rangingTask);
            reinitializeWithNewRole();
            resumeTask(rangingTask);
        }
    }
}
```

**Benefits**:
- No reset downtime
- WiFi telemetry
- More memory (520KB SRAM)
- Faster CPU (240 MHz)

**See**: Research doc Section 7 (Migration to ESP32)

### 9.3 Long-Term: Production Firmware

**Features for Production Swarm:**

1. **Flight Controller Integration**
   - MAVLink commands for role switching
   - Position data output to autopilot
   - Collision warnings to flight controller

2. **Advanced TDMA**
   - Dynamic slot allocation
   - Beacon synchronization
   - Automatic slot reassignment on failure

3. **Position Computation**
   - Trilateration from multiple anchors
   - Kalman filtering for smooth tracking
   - 3D position with 4+ anchors

4. **Telemetry**
   - WiFi/LoRa ground station link
   - Real-time position visualization
   - Swarm topology monitoring

5. **Fault Tolerance**
   - Anchor failure detection
   - Automatic role reassignment
   - Mesh routing for out-of-range nodes

---

## 10. References

### Research Documents

1. **DUAL_ROLE_ARCHITECTURE.md** (48KB)
   - Path: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/DUAL_ROLE_ARCHITECTURE.md`
   - Primary reference for this design

2. **INTERRUPT_ISSUE_SUMMARY.md**
   - Path: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/INTERRUPT_ISSUE_SUMMARY.md`
   - Known bug fix

3. **UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md**
   - TDMA analysis and scaling

### Related Tests

- **test_06_ranging**: Anchor/tag examples
- **test_05_pingpong**: Two-way communication

### External References

- IEEE 802.15.4a-2007: UWB PHY standard
- Decawave DW1000 User Manual
- Arduino EEPROM Library Documentation

---

## Conclusion

This dual-role firmware demonstrates that **flexible role switching is possible** on Arduino Uno despite library limitations, through a **reset-based approach** with EEPROM storage.

**Key Achievements:**
1. Single firmware for both roles
2. Simple serial command interface
3. Persistent configuration
4. TDMA support for multi-node operation
5. Works within Arduino Uno memory constraints

**Key Limitations:**
1. Role switch requires 5-second reset
2. TDMA slots not synchronized (free-running)
3. Single-role architecture (not simultaneous)

**Next Steps:**
1. Test prototype on hardware
2. Measure performance (accuracy, update rate)
3. Collect field data
4. Plan ESP32 migration for production

---

**Document Status**: Complete
**Review Date**: 2026-01-11
**Next Review**: After initial hardware testing

---

**End of Design Document**
