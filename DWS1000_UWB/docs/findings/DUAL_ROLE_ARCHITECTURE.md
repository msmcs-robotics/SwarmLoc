# Dual-Role UWB Firmware Architecture for Drone Swarms

**Project**: SwarmLoc - GPS-Denied Positioning System
**Hardware**: Arduino Uno + DW1000 (PCL298336 v1.3 Shield)
**Date**: 2026-01-11
**Purpose**: Research and document dual-role UWB firmware architecture for drone swarms

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Dual-Role Implementation Analysis](#dual-role-implementation-analysis)
3. [Drone Swarm Topologies](#drone-swarm-topologies)
4. [Role-Switching Protocols](#role-switching-protocols)
5. [Practical Recommendations](#practical-recommendations)
6. [Code Examples](#code-examples)
7. [Testing Strategy](#testing-strategy)
8. [References](#references)

---

## Executive Summary

### Key Findings

1. **DW1000Ranging Library Does NOT Support Dynamic Role Switching**
   - Library uses separate `startAsTag()` and `startAsAnchor()` functions
   - Role is fixed at initialization and cannot be changed at runtime
   - No built-in mechanism for role switching or dual-role operation

2. **Current Architecture is Fixed-Role**
   - Tags initiate ranging (send POLL messages)
   - Anchors respond to ranging requests (send POLL_ACK)
   - Protocol state machine is role-specific
   - Memory footprint optimized for single role

3. **Arduino Uno Constraints**
   - 2KB SRAM severely limits dual-role implementation
   - MAX_DEVICES = 4 (each device requires 74 bytes)
   - Recommended swarm size: 3-6 nodes
   - Single-role firmware is strongly recommended

4. **Best Topology for Drone Swarms**
   - **Star topology** for 3-6 drones (recommended for Arduino Uno)
   - **Hybrid topology** for 6-15 drones (requires ESP32)
   - Fixed anchors + mobile tags is industry standard
   - Mesh topology requires ESP32 and custom protocol implementation

### Quick Answer Matrix

| Question | Answer | Rationale |
|----------|--------|-----------|
| **Should each drone support BOTH roles?** | **NO** for Arduino Uno | Memory constraints, library limitations |
| **Use dedicated anchor/tag firmware?** | **YES** | Simpler, more reliable, proven approach |
| **Can DW1000Ranging switch roles dynamically?** | **NO** | Not implemented in library, requires full rewrite |
| **Best topology for 5-10 drones?** | **Star** (3-6) or **Hybrid** (6-10) | Balance simplicity vs scalability |

---

## 1. Dual-Role Implementation Analysis

### 1.1 Current Library Architecture

The DW1000Ranging library uses a **fixed-role architecture**:

```cpp
// From DW1000Ranging.h (lines 84-85)
static void startAsAnchor(char address[], const byte mode[], const bool randomShortAddress = true);
static void startAsTag(char address[], const byte mode[], const bool randomShortAddress = true);

// From DW1000Ranging.cpp (lines 162-223)
void DW1000RangingClass::startAsAnchor(...) {
    _type = ANCHOR;
    Serial.println("### ANCHOR ###");
    // Initialize as responder
}

void DW1000RangingClass::startAsTag(...) {
    _type = TAG;
    Serial.println("### TAG ###");
    // Initialize as initiator
}
```

**Key Observations:**
1. `_type` variable is set once during initialization (line 189 for anchor, 220 for tag)
2. No function to change role after `setup()`
3. Protocol state machine branches on `_type` in `loop()` (DW1000Ranging.cpp lines 392-678)
4. Different callbacks for anchors (blink handler) vs tags (new device handler)

### 1.2 Why Dynamic Role Switching is NOT Implemented

**Technical Reasons:**

1. **Protocol State Machine Complexity**
   - Tags maintain state: POLL_ACK expected, RANGE_REPORT expected
   - Anchors maintain state: POLL expected, RANGE expected
   - Switching requires flushing state and resynchronizing

2. **Memory Management**
   - Tags store device array of anchors (up to 4)
   - Anchors store device array of tags (currently hardcoded to 1 in line 269-271)
   - Both roles simultaneously would require 2× device arrays

3. **Timestamp Management**
   - Each DW1000Device stores 6 timestamps (82-87 in DW1000Device.h)
   - Tag timestamps vs anchor timestamps have different meanings
   - Role switch would invalidate in-flight ranging operations

**Code Evidence:**
```cpp
// From DW1000Ranging.cpp lines 515-625
if(_type == ANCHOR) {
    if(messageType == POLL) {
        // Anchor-specific handling
        DW1000.getReceiveTimestamp(myDistantDevice->timePollReceived);
        transmitPollAck(myDistantDevice);
    }
    else if(messageType == RANGE) {
        // Compute range and send report
        computeRangeAsymmetric(myDistantDevice, &myTOF);
        transmitRangeReport(myDistantDevice);
    }
}
else if(_type == TAG) {
    if(messageType == POLL_ACK) {
        // Tag-specific handling
        DW1000.getReceiveTimestamp(myDistantDevice->timePollAckReceived);
        transmitRange(nullptr);  // Broadcast to all anchors
    }
}
```

### 1.3 Options for Implementation

#### Option A: Dedicated Firmware (RECOMMENDED)

**Architecture:**
```
anchor.ino          tag.ino
    |                  |
    v                  v
startAsAnchor()    startAsTag()
    |                  |
Loop: Respond      Loop: Initiate
to POLL msgs       POLL msgs
```

**Advantages:**
- ✓ Works with existing DW1000Ranging library (no modifications)
- ✓ Minimal memory footprint (single role only)
- ✓ Simple state management
- ✓ Proven, tested approach (library examples)
- ✓ Easy debugging (clear separation)

**Disadvantages:**
- ✗ Cannot change role without reflashing firmware
- ✗ Fixed topology (X anchors, Y tags)
- ✗ No dynamic adaptation to failures

**Memory Usage:**
```
Arduino Uno SRAM: 2048 bytes
├─ Arduino core:           ~200 bytes
├─ DW1000 library state:   ~150 bytes
├─ Device array (4×74):     296 bytes
├─ Application variables:   ~300 bytes
└─ Stack:                   ~800 bytes
────────────────────────────────────
Total used:                ~1746 bytes
Available margin:           ~302 bytes
```

#### Option B: Dual-Role Firmware (NOT RECOMMENDED for Arduino Uno)

**Architecture:**
```cpp
enum NodeRole { TAG, ANCHOR, IDLE };
volatile NodeRole currentRole = IDLE;

void setup() {
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    // Start in IDLE, wait for role assignment
}

void loop() {
    switch(currentRole) {
        case TAG:
            // Run tag protocol manually
            handleTagProtocol();
            break;
        case ANCHOR:
            // Run anchor protocol manually
            handleAnchorProtocol();
            break;
        case IDLE:
            // Listen for role assignment beacon
            break;
    }
}

void switchRole(NodeRole newRole) {
    // Clear all state
    clearDeviceArray();
    resetProtocolState();

    // Reinitialize with new role
    if (newRole == TAG) {
        // Manually implement tag behavior
    } else if (newRole == ANCHOR) {
        // Manually implement anchor behavior
    }

    currentRole = newRole;
}
```

**Challenges:**
1. **Cannot use DW1000Ranging library** - must reimplement protocol
2. **Memory consumption** - need space for both roles' code paths
3. **Complexity** - estimated 2-3× more complex than single-role
4. **Timing issues** - role switch introduces delays and synchronization problems

**Memory Impact:**
```
Dual-role firmware estimate:
├─ Tag protocol code:       ~500 bytes flash, ~200 bytes SRAM
├─ Anchor protocol code:    ~500 bytes flash, ~200 bytes SRAM
├─ Role switching logic:    ~200 bytes flash, ~100 bytes SRAM
├─ Duplicate device arrays: +296 bytes SRAM (tag list + anchor list)
────────────────────────────────────────────────────────
Total additional:           ~1200 bytes flash, ~796 bytes SRAM

Arduino Uno limits: 32KB flash (OK), 2048 bytes SRAM (FAILS)
```

**Verdict:** Dual-role firmware would **exceed Arduino Uno SRAM** capacity.

#### Option C: Role-Switching with ESP32 (FUTURE WORK)

**Feasibility:** High - ESP32 has 520KB SRAM (260× more than Arduino Uno)

**Implementation Approach:**
```cpp
// ESP32 dual-role firmware structure
class DualRoleNode {
private:
    NodeRole currentRole;
    TagProtocol tagHandler;
    AnchorProtocol anchorHandler;

public:
    void switchToTag() {
        currentRole = TAG;
        tagHandler.initialize();
    }

    void switchToAnchor() {
        currentRole = ANCHOR;
        anchorHandler.initialize();
    }

    void loop() {
        if (currentRole == TAG) {
            tagHandler.loop();
        } else {
            anchorHandler.loop();
        }
    }
};
```

**When to Use:**
- Swarm >6 nodes
- Dynamic topology required
- Fault tolerance (anchor failure recovery)
- Research platform

---

## 2. Drone Swarm Topologies

### 2.1 Star Topology (Recommended for 3-6 Drones)

```
            Anchor (Fixed Position)
                    |
                    | UWB Ranging
      +-------------+-------------+
      |             |             |
    Tag 1         Tag 2         Tag 3
  (Drone 1)     (Drone 2)     (Drone 3)
```

**Implementation:**
- 1 stationary anchor (known position)
- N mobile tags (compute position via ranging)
- All ranging initiated by tags

**Advantages:**
- ✓ Simple protocol (single anchor)
- ✓ Easy synchronization
- ✓ Low collision probability
- ✓ Works well with Arduino Uno

**Disadvantages:**
- ✗ Single point of failure (anchor)
- ✗ No 3D positioning (need ≥3 anchors for trilateration)
- ✗ Limited range (all drones must be within anchor range)

**Best For:**
- 2D positioning only
- Small area coverage (<50m radius)
- Proof-of-concept development
- Testing and validation

**Code Example:**
```cpp
// Anchor (stationary)
void setup() {
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachBlinkDevice(newBlink);
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",
                                 DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}

// Tag (drone)
void setup() {
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                              DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}
```

### 2.2 Multi-Anchor Star (3D Positioning for 1-4 Drones)

```
    Anchor 1        Anchor 2        Anchor 3
   (0, 0, 5)      (10, 0, 5)      (5, 10, 5)
        \              |              /
         \             |             /
          \            |            /
           \           |           /
            \          |          /
             \         |         /
              \        |        /
                  TAG (Drone)
              Position (x,y,z)
            Computed via Trilateration
```

**Implementation:**
- 3+ stationary anchors (known positions)
- 1-4 mobile tags (compute 3D position)
- Tags range sequentially with each anchor

**Communication Flow:**
```
Tag → Anchor 1: POLL → POLL_ACK → RANGE → RANGE_REPORT (d1)
Tag → Anchor 2: POLL → POLL_ACK → RANGE → RANGE_REPORT (d2)
Tag → Anchor 3: POLL → POLL_ACK → RANGE → RANGE_REPORT (d3)
Tag computes position: (x,y,z) from (d1, d2, d3)
```

**Timing (TDMA):**
```
Time slot allocation (100ms per slot):
┌─────────┬─────────┬─────────┬─────────┐
│  0-100  │ 100-200 │ 200-300 │ 300-400 │
├─────────┼─────────┼─────────┼─────────┤
│ Tag→A1  │ Tag→A2  │ Tag→A3  │ Compute │
│ Ranging │ Ranging │ Ranging │ Position│
└─────────┴─────────┴─────────┴─────────┘
Cycle: 400ms → 2.5 Hz update rate
```

**Advantages:**
- ✓ 3D positioning capability
- ✓ Redundancy (4th anchor for error correction)
- ✓ Scalable to multiple tags (TDMA scheduling)
- ✓ Industry-standard approach

**Disadvantages:**
- ✗ Requires 3+ anchor nodes (hardware cost)
- ✗ Sequential ranging (slower update rate)
- ✗ All anchors must be in range of tag

**Trilateration Code:**
```cpp
struct Position { float x, y, z; };

Position trilaterate(Position a1, float d1,
                     Position a2, float d2,
                     Position a3, float d3) {
    // Simplified 2D trilateration (extend to 3D with 4th anchor)
    float A = 2 * (a2.x - a1.x);
    float B = 2 * (a2.y - a1.y);
    float C = d1*d1 - d2*d2 - a1.x*a1.x + a2.x*a2.x
              - a1.y*a1.y + a2.y*a2.y;

    float D = 2 * (a3.x - a2.x);
    float E = 2 * (a3.y - a2.y);
    float F = d2*d2 - d3*d3 - a2.x*a2.x + a3.x*a3.x
              - a2.y*a2.y + a3.y*a3.y;

    Position result;
    result.x = (C*E - F*B) / (E*A - B*D);
    result.y = (C*D - A*F) / (B*D - A*E);
    result.z = 0;  // 2D only, use 4th anchor for 3D

    return result;
}
```

**Best For:**
- 3D positioning required
- Small-to-medium swarms (1-4 tags)
- Fixed infrastructure (anchors on tripods/walls)
- Indoor positioning systems

### 2.3 Mesh Topology (NOT RECOMMENDED for Arduino Uno)

```
   Drone 1 ─────── Drone 2
      │   ╲       ╱   │
      │    ╲     ╱    │
      │     ╲   ╱     │
      │      ╲ ╱      │
      │       ╳       │
      │      ╱ ╲      │
      │     ╱   ╲     │
      │    ╱     ╲    │
      │   ╱       ╲   │
   Drone 3 ─────── Drone 4
```

**Implementation:**
- All nodes can act as both anchor and tag
- Peer-to-peer ranging
- Distributed position computation

**Challenges:**
1. **N×(N-1)/2 ranging pairs** - 4 nodes = 6 pairs, 10 nodes = 45 pairs
2. **No global reference** - relative positioning only
3. **Complex synchronization** - no master clock
4. **High message collision** - all nodes transmitting

**Protocol Requirements:**
- TDMA with distributed slot allocation
- Consensus algorithms for position
- Multi-hop routing for out-of-range nodes
- Conflict resolution for simultaneous ranges

**Conclusion:** Mesh topology requires ESP32 + custom protocol implementation. Not feasible on Arduino Uno.

### 2.4 Hybrid Topology (Recommended for 6-15 Drones with ESP32)

```
    Ground Anchors (Fixed)
    A1          A2          A3
     │           │           │
     └───────────┼───────────┘
                 │
        ┌────────┼────────┐
        │        │        │
      Drone 1  Drone 2  Drone 3
        │╲       │       ╱│
        │ ╲      │      ╱ │
        │  ╲     │     ╱  │
        │   ╲    │    ╱   │
        │    Drone  4─────┘
        │      │
        └──────┘

    Legend:
    │  = Anchor-Tag ranging (absolute position)
    ╲  = Tag-Tag ranging (relative distance)
```

**Implementation:**
- Fixed anchors provide absolute position reference
- Tags range with anchors for global position
- Tags range with each other for collision avoidance
- Two-tier communication protocol

**Advantages:**
- ✓ Absolute positioning from anchors
- ✓ Collision avoidance via peer ranging
- ✓ Fault tolerance (multiple anchors)
- ✓ Scalable to medium swarms

**Disadvantages:**
- ✗ Complex protocol (anchor vs peer ranging)
- ✗ Requires ESP32 (memory + processing)
- ✗ Higher message traffic

**Best For:**
- Production drone swarms
- 6-15 drones
- Large area coverage
- Safety-critical applications (collision avoidance)

### 2.5 Topology Recommendation Matrix

| Swarm Size | Positioning Needs | Hardware | Recommended Topology | Update Rate |
|------------|------------------|----------|----------------------|-------------|
| 2-4 drones | 2D, single area | Arduino Uno | **Star (1 anchor)** | 10 Hz |
| 2-4 drones | 3D, high accuracy | Arduino Uno | **Multi-anchor Star (3-4 anchors)** | 2.5 Hz |
| 5-6 drones | 2D positioning | Arduino Uno | Star with TDMA | 5 Hz |
| 6-10 drones | 3D positioning | **ESP32** | **Hybrid (3 anchors + peer ranging)** | 5-10 Hz |
| 10-20 drones | 3D + collision avoid | ESP32 | Hybrid (4+ anchors) | 2-5 Hz |
| 20+ drones | Research platform | ESP32 + Ground PC | Hierarchical | 1-2 Hz |

---

## 3. Role-Switching Protocols

### 3.1 Can DW1000Ranging Switch Roles Dynamically?

**Answer: NO** - the library does not support dynamic role switching.

**Evidence from Code Analysis:**

1. **Role is set once in setup():**
```cpp
// From DW1000Ranging.cpp line 189 (anchor) and 220 (tag)
_type = ANCHOR;  // or _type = TAG;
// No function exists to change _type after initialization
```

2. **Protocol state machine depends on fixed role:**
```cpp
// From DW1000Ranging.cpp lines 515-678
if(_type == ANCHOR) {
    // Handle anchor-specific messages (POLL, RANGE)
}
else if(_type == TAG) {
    // Handle tag-specific messages (POLL_ACK, RANGE_REPORT)
}
```

3. **Different device management:**
```cpp
// From DW1000Ranging.cpp lines 268-271
if(_type == ANCHOR) {
    _networkDevicesNumber = 0;  // Anchors only track 1 tag
}
```

### 3.2 Time-Division Role Switching (Theoretical)

**Concept:** Nodes take turns being anchor vs tag in different time slots.

```
Node A Timeline:
┌─────────┬─────────┬─────────┬─────────┐
│ Slot 0  │ Slot 1  │ Slot 2  │ Slot 3  │
│ ANCHOR  │  TAG    │ ANCHOR  │  TAG    │
│ (wait)  │ (range) │ (wait)  │ (range) │
└─────────┴─────────┴─────────┴─────────┘

Node B Timeline:
┌─────────┬─────────┬─────────┬─────────┐
│ Slot 0  │ Slot 1  │ Slot 2  │ Slot 3  │
│  TAG    │ ANCHOR  │  TAG    │ ANCHOR  │
│ (range) │ (wait)  │ (range) │ (wait)  │
└─────────┴─────────┴─────────┴─────────┘
```

**Protocol:**
1. Nodes agree on time slot schedule (via beacon)
2. In "anchor" slots: listen for POLL, respond with POLL_ACK
3. In "tag" slots: send POLL, process RANGE

**Implementation Challenges:**

**1. State Management:**
```cpp
// Pseudo-code (NOT WORKING with current library)
void loop() {
    uint8_t currentSlot = (millis() / SLOT_DURATION) % NUM_SLOTS;

    if (slotSchedule[currentSlot] == ANCHOR_ROLE) {
        // Switch to anchor mode
        // Problem: Cannot call startAsAnchor() in loop()
        handleAnchorSlot();
    } else {
        // Switch to tag mode
        // Problem: Cannot call startAsTag() in loop()
        handleTagSlot();
    }
}
```

**2. Timestamp Validity:**
- Tags store `timePollSent`, anchors store `timePollReceived`
- Role switch invalidates all in-flight timestamps
- Must wait for all pending ranging operations to complete

**3. Device List Management:**
- Tags track anchors, anchors track tags
- Role switch requires flushing device list
- Discovery protocol must re-run after each switch

**Conclusion:** Time-division role switching requires **complete library rewrite**.

### 3.3 Asymmetric Roles (Recommended Approach)

**Concept:** Some nodes are permanently anchors, others permanently tags.

```
Anchor Drones (Fixed Positions):
├─ Drone 1: Hover at (0, 0, 5m)
├─ Drone 2: Hover at (10, 0, 5m)
└─ Drone 3: Hover at (5, 10, 5m)

Tag Drones (Mobile):
├─ Drone 4: Navigate freely, range with A1, A2, A3
├─ Drone 5: Navigate freely, range with A1, A2, A3
└─ Drone 6: Navigate freely, range with A1, A2, A3
```

**Advantages:**
- ✓ Works with unmodified DW1000Ranging library
- ✓ Clear separation of responsibilities
- ✓ Simple state management
- ✓ Proven in commercial systems

**Disadvantages:**
- ✗ Anchor drones do not compute position (must maintain GPS or manual positioning)
- ✗ Cannot dynamically reassign roles
- ✗ Loss of anchor drone degrades system

**Mitigation Strategies:**

1. **Anchor Position Maintenance:**
```cpp
// Anchor drones use GPS or visual markers to maintain position
void anchorLoop() {
    Position currentPos = gps.getPosition();
    if (distanceTo(desiredPos, currentPos) > 0.5) {
        // Move back to designated position
        moveToPosition(desiredPos);
    }

    // While maintaining position, perform anchor duties
    DW1000Ranging.loop();
}
```

2. **Anchor Redundancy:**
- Deploy 4+ anchors for 3D positioning (only need 3)
- If 1 anchor fails, system still works
- Tags dynamically select best 3 anchors

3. **Hot Standby:**
- Have extra drone ready as backup anchor
- Manual reflash if anchor fails
- 5-10 minute recovery time

### 3.4 Can a Node Be Anchor for Some Peers, Tag for Others?

**Answer: NO** - not supported by DW1000Ranging library.

**Reason:** Protocol state machine is global per node:
- `_expectedMsgId` variable tracks next expected message type (global)
- Cannot simultaneously expect POLL_ACK (as tag) and POLL (as anchor)
- Device array stores either tags (anchor mode) or anchors (tag mode), not both

**Theoretical Implementation (requires custom protocol):**

```cpp
// Hypothetical dual-role per-peer tracking
struct PeerDevice {
    byte address[2];
    NodeRole myRoleForThisPeer;  // Am I anchor or tag for this peer?
    // ... timestamp fields
};

void loop() {
    if (messageReceived) {
        PeerDevice* peer = findPeer(sourceAddress);

        if (peer->myRoleForThisPeer == ANCHOR) {
            // Handle as anchor for this peer
            if (msgType == POLL) {
                sendPollAck(peer);
            }
        } else {
            // Handle as tag for this peer
            if (msgType == POLL_ACK) {
                sendRange(peer);
            }
        }
    }
}
```

**Complexity:** Very high - essentially rewriting entire protocol.

**Use Case:** Limited - typically want consistent roles for predictable behavior.

---

## 4. Practical Recommendations

### 4.1 For Arduino Uno + DW1000 Setup

**Recommendation:** Use **dedicated firmware** with **asymmetric roles**.

**Deployment Strategy:**

**Small Swarm (3-4 Drones):**
```
Setup 1: Multi-Anchor Star
├─ Anchor firmware: Flash on 3 drones (stationary positions)
│  └─ Code: DW1000Ranging_ANCHOR.ino
└─ Tag firmware: Flash on 1 drone (mobile navigator)
   └─ Code: DW1000Ranging_TAG.ino
```

**Medium Swarm (5-6 Drones):**
```
Setup 2: Multiple Tags with TDMA
├─ Anchor firmware: 3 drones (stationary)
└─ Tag firmware: 3 drones (mobile, TDMA scheduled)
   └─ Modification: Add TDMA slot scheduling to tag firmware
```

**TDMA Implementation for Tags:**

```cpp
// Add to tag firmware
#define NUM_TAGS 3
#define SLOT_DURATION 150  // ms
#define MY_SLOT_ID 0  // Change for each tag: 0, 1, 2

void loop() {
    uint32_t currentTime = millis();
    uint32_t timeInFrame = currentTime % (NUM_TAGS * SLOT_DURATION);
    uint8_t currentSlot = timeInFrame / SLOT_DURATION;

    if (currentSlot == MY_SLOT_ID) {
        // My turn to range
        DW1000Ranging.loop();
    } else {
        // Not my turn, stay idle
        delay(10);
    }
}
```

### 4.2 For Drone Swarm Use Case

**Application Requirements:**
- Collision avoidance: 5-10 Hz position updates
- Navigation: 2-5 Hz position updates
- Formation flying: 10-20 Hz position updates

**System Design:**

**Phase 1: Proof of Concept (Arduino Uno)**
```
Anchors: 3 drones at fixed GPS coordinates
Tags: 1 drone navigating GPS-denied area
Update rate: 2.5 Hz (good for navigation)
Hardware cost: 4× Arduino Uno + DW1000 ≈ $100-120
```

**Phase 2: Full Swarm (Migrate to ESP32)**
```
Anchors: 4 drones (redundancy for 3D positioning)
Tags: 6-10 drones
Update rate: 10 Hz (good for collision avoidance)
Hardware cost: 10× ESP32 + DW1000 ≈ $200-300
```

### 4.3 Code Structure Recommendations

**Directory Structure:**
```
SwarmLoc/
├─ firmware/
│  ├─ anchor_v1/
│  │  └─ anchor_v1.ino        # Stationary anchor firmware
│  ├─ tag_v1/
│  │  └─ tag_v1.ino           # Mobile tag firmware
│  └─ tag_tdma_v2/
│     └─ tag_tdma_v2.ino      # Tag with TDMA support
├─ libraries/
│  └─ DW1000/                 # Arduino-dw1000 library
└─ docs/
   └─ DEPLOYMENT_GUIDE.md     # How to flash and configure
```

**Anchor Firmware Template:**
```cpp
/*
 * Anchor Firmware for SwarmLoc Drone Swarm
 * Role: Stationary anchor at known position
 * Hardware: Arduino Uno + DW1000
 */

#include <SPI.h>
#include "DW1000Ranging.h"

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// UNIQUE ADDRESS FOR EACH ANCHOR
// Anchor 1: "82:17:5B:D5:A9:9A:E2:9A"
// Anchor 2: "82:17:5B:D5:A9:9A:E2:9B"
// Anchor 3: "82:17:5B:D5:A9:9A:E2:9C"
const char* MY_ADDRESS = "82:17:5B:D5:A9:9A:E2:9A";  // CHANGE FOR EACH ANCHOR

// KNOWN POSITION (update after deployment)
float anchorX = 0.0;    // meters
float anchorY = 0.0;    // meters
float anchorZ = 5.0;    // meters (altitude)

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("SwarmLoc Anchor Starting...");
    Serial.print("Position: (");
    Serial.print(anchorX); Serial.print(", ");
    Serial.print(anchorY); Serial.print(", ");
    Serial.print(anchorZ); Serial.println(")");

    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachBlinkDevice(newBlink);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    DW1000Ranging.startAsAnchor(MY_ADDRESS,
                                 DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println("Anchor ready!");
}

void loop() {
    DW1000Ranging.loop();
}

void newRange() {
    Serial.print("Range from tag ");
    Serial.print(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
    Serial.print(": ");
    Serial.print(DW1000Ranging.getDistantDevice()->getRange());
    Serial.println(" m");
}

void newBlink(DW1000Device* device) {
    Serial.print("New tag discovered: ");
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("Tag inactive: ");
    Serial.println(device->getShortAddress(), HEX);
}
```

**Tag Firmware Template:**
```cpp
/*
 * Tag Firmware for SwarmLoc Drone Swarm
 * Role: Mobile tag, computes position from anchor ranges
 * Hardware: Arduino Uno + DW1000
 */

#include <SPI.h>
#include "DW1000Ranging.h"

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

const char* MY_ADDRESS = "7D:00:22:EA:82:60:3B:9C";

// Anchor positions (known)
struct Position { float x, y, z; };
Position anchor1 = {0.0, 0.0, 5.0};
Position anchor2 = {10.0, 0.0, 5.0};
Position anchor3 = {5.0, 10.0, 5.0};

// Measured ranges
float range1 = 0, range2 = 0, range3 = 0;
Position myPosition = {0, 0, 0};

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("SwarmLoc Tag Starting...");

    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    DW1000Ranging.startAsTag(MY_ADDRESS,
                              DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println("Tag ready! Searching for anchors...");
}

void loop() {
    DW1000Ranging.loop();

    // Compute position if we have ranges to all 3 anchors
    static uint32_t lastCompute = 0;
    if (millis() - lastCompute > 500) {  // Compute every 500ms
        if (range1 > 0 && range2 > 0 && range3 > 0) {
            computePosition();
            printPosition();
        }
        lastCompute = millis();
    }
}

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();
    uint16_t addr = device->getShortAddress();
    float distance = device->getRange();

    // Match anchor by short address and store range
    // NOTE: You need to determine anchor addresses after first run
    if (addr == 0x829A) {  // Anchor 1 short address (example)
        range1 = distance;
    } else if (addr == 0x829B) {  // Anchor 2
        range2 = distance;
    } else if (addr == 0x829C) {  // Anchor 3
        range3 = distance;
    }

    Serial.print("Range from anchor ");
    Serial.print(addr, HEX);
    Serial.print(": ");
    Serial.print(distance);
    Serial.println(" m");
}

void newDevice(DW1000Device* device) {
    Serial.print("Anchor discovered: ");
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("Anchor lost: ");
    Serial.println(device->getShortAddress(), HEX);
}

void computePosition() {
    // Simplified 2D trilateration
    // (For 3D, use 4th anchor and solve system of equations)

    float A = 2 * (anchor2.x - anchor1.x);
    float B = 2 * (anchor2.y - anchor1.y);
    float C = range1*range1 - range2*range2
              - anchor1.x*anchor1.x + anchor2.x*anchor2.x
              - anchor1.y*anchor1.y + anchor2.y*anchor2.y;

    float D = 2 * (anchor3.x - anchor2.x);
    float E = 2 * (anchor3.y - anchor2.y);
    float F = range2*range2 - range3*range3
              - anchor2.x*anchor2.x + anchor3.x*anchor3.x
              - anchor2.y*anchor2.y + anchor3.y*anchor3.y;

    float denom = (E*A - B*D);
    if (abs(denom) > 0.001) {  // Avoid division by zero
        myPosition.x = (C*E - F*B) / denom;
        myPosition.y = (C*D - A*F) / (B*D - A*E);
        myPosition.z = 2.0;  // Assume constant altitude for now
    }
}

void printPosition() {
    Serial.print("Position: (");
    Serial.print(myPosition.x, 2);
    Serial.print(", ");
    Serial.print(myPosition.y, 2);
    Serial.print(", ");
    Serial.print(myPosition.z, 2);
    Serial.println(")");
}
```

### 4.4 Testing Strategy

**Test 1: Single Pair Ranging**
- Objective: Validate basic ranging works
- Setup: 1 anchor + 1 tag, 1m apart
- Expected: Range reported as ~1.0m ± 0.2m
- Duration: 5 minutes

**Test 2: Multi-Anchor Ranging**
- Objective: Validate tag can range with multiple anchors
- Setup: 3 anchors in triangle, 1 tag in center
- Expected: Tag reports ranges to all 3 anchors
- Duration: 10 minutes

**Test 3: Position Computation**
- Objective: Validate trilateration algorithm
- Setup: 3 anchors at known positions, tag at test positions
- Procedure:
  1. Place tag at position (2, 2)
  2. Record computed position vs actual
  3. Repeat for 10 test positions
- Expected: Error < 0.5m for 80% of positions

**Test 4: TDMA Multi-Tag**
- Objective: Validate collision-free ranging with multiple tags
- Setup: 3 anchors + 3 tags with TDMA
- Expected: All 3 tags successfully range without collisions
- Duration: 30 minutes continuous

**Test 5: Dynamic Movement**
- Objective: Validate position tracking during motion
- Setup: 3 anchors, 1 tag moving at 0.5 m/s
- Expected: Position updates at 2.5 Hz, smooth trajectory
- Duration: 5 minutes

**Success Criteria:**
- [ ] Ranging accuracy: ±20-50cm (Arduino Uno limitation)
- [ ] Update rate: ≥2 Hz per tag
- [ ] Packet loss: <10%
- [ ] System uptime: >1 hour continuous operation
- [ ] No firmware crashes or resets

---

## 5. Code Examples

### 5.1 Role Determination at Boot

```cpp
/*
 * Compile-time role selection
 * Choose role by uncommenting one line
 */

// Uncomment ONLY ONE of these:
#define ROLE_ANCHOR
// #define ROLE_TAG

void setup() {
    Serial.begin(115200);
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

#ifdef ROLE_ANCHOR
    // Anchor configuration
    DW1000Ranging.attachNewRange(anchorNewRange);
    DW1000Ranging.attachBlinkDevice(newBlink);
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",
                                 DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    Serial.println("MODE: ANCHOR");
#elif defined(ROLE_TAG)
    // Tag configuration
    DW1000Ranging.attachNewRange(tagNewRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                              DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    Serial.println("MODE: TAG");
#else
    #error "Must define ROLE_ANCHOR or ROLE_TAG"
#endif
}
```

### 5.2 Runtime Role Detection (Pin-Based)

```cpp
/*
 * Runtime role selection based on hardware jumper
 * Connect PIN 7 to GND for ANCHOR mode
 * Leave PIN 7 floating for TAG mode
 */

#define ROLE_SELECT_PIN 7

void setup() {
    Serial.begin(115200);

    // Configure role select pin
    pinMode(ROLE_SELECT_PIN, INPUT_PULLUP);
    delay(100);

    bool isAnchor = (digitalRead(ROLE_SELECT_PIN) == LOW);

    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    if (isAnchor) {
        DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",
                                     DW1000.MODE_LONGDATA_RANGE_ACCURACY);
        Serial.println("MODE: ANCHOR (pin 7 grounded)");
    } else {
        DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                                  DW1000.MODE_LONGDATA_RANGE_ACCURACY);
        Serial.println("MODE: TAG (pin 7 floating)");
    }
}
```

### 5.3 Multi-Anchor Position Computation

```cpp
/*
 * 3D trilateration with 4 anchors
 * Uses least-squares solution for overdetermined system
 */

struct Anchor {
    float x, y, z;
    float range;
};

Anchor anchors[4] = {
    {0, 0, 5, 0},      // Anchor 1
    {10, 0, 5, 0},     // Anchor 2
    {5, 10, 5, 0},     // Anchor 3
    {5, 5, 10, 0}      // Anchor 4
};

void computePosition3D(float* x, float* y, float* z) {
    // Using anchor 1 as reference
    float A[3][3], b[3];

    for (int i = 0; i < 3; i++) {
        A[i][0] = 2 * (anchors[i+1].x - anchors[0].x);
        A[i][1] = 2 * (anchors[i+1].y - anchors[0].y);
        A[i][2] = 2 * (anchors[i+1].z - anchors[0].z);

        b[i] = anchors[0].range * anchors[0].range
               - anchors[i+1].range * anchors[i+1].range
               - anchors[0].x * anchors[0].x + anchors[i+1].x * anchors[i+1].x
               - anchors[0].y * anchors[0].y + anchors[i+1].y * anchors[i+1].y
               - anchors[0].z * anchors[0].z + anchors[i+1].z * anchors[i+1].z;
    }

    // Solve 3×3 system using Gaussian elimination
    // (Simplified - use matrix library for robustness)
    solveLinearSystem3x3(A, b, x, y, z);
}

void solveLinearSystem3x3(float A[3][3], float b[3],
                          float* x, float* y, float* z) {
    // Gaussian elimination (simplified implementation)
    // For production, use a robust matrix library

    // Forward elimination
    for (int i = 0; i < 3; i++) {
        // ... (omitted for brevity, see full implementation)
    }

    // Back substitution
    *z = b[2] / A[2][2];
    *y = (b[1] - A[1][2] * (*z)) / A[1][1];
    *x = (b[0] - A[0][1] * (*y) - A[0][2] * (*z)) / A[0][0];
}
```

### 5.4 TDMA Slot Scheduler

```cpp
/*
 * TDMA slot scheduler for multiple tags
 * Prevents collision by time-division multiple access
 */

#define NUM_TAGS 3
#define SLOT_DURATION_MS 150
#define FRAME_DURATION_MS (NUM_TAGS * SLOT_DURATION_MS)

// Set this differently for each tag: 0, 1, or 2
#define MY_SLOT_ID 0

uint32_t frameStartTime = 0;
bool synchronized = false;

void setup() {
    // ... standard setup ...

    // Wait for synchronization beacon from anchor
    synchronizeToFrame();
}

void synchronizeToFrame() {
    // Listen for sync beacon (optional enhancement)
    // For now, just use millis() as frame reference
    frameStartTime = millis();
    synchronized = true;

    Serial.print("Synced. My slot: ");
    Serial.println(MY_SLOT_ID);
}

void loop() {
    if (!synchronized) {
        synchronizeToFrame();
        return;
    }

    uint32_t currentTime = millis();
    uint32_t timeInFrame = (currentTime - frameStartTime) % FRAME_DURATION_MS;
    uint8_t currentSlot = timeInFrame / SLOT_DURATION_MS;

    if (currentSlot == MY_SLOT_ID) {
        // My slot - perform ranging
        DW1000Ranging.loop();
    } else {
        // Not my slot - stay idle or listen
        delay(10);
    }

    // Re-synchronize every 10 seconds
    if (currentTime - frameStartTime > 10000) {
        synchronized = false;
    }
}
```

### 5.5 Collision Avoidance Logic

```cpp
/*
 * Collision avoidance using UWB ranging
 * Maintains minimum safe distance between drones
 */

#define COLLISION_THRESHOLD 1.0  // meters
#define WARNING_THRESHOLD 2.0    // meters

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();
    float distance = device->getRange();

    checkCollisionRisk(distance, device->getShortAddress());
}

void checkCollisionRisk(float distance, uint16_t peerAddress) {
    if (distance < COLLISION_THRESHOLD) {
        // CRITICAL: Emergency stop
        Serial.println("COLLISION ALERT!");
        Serial.print("Distance to ");
        Serial.print(peerAddress, HEX);
        Serial.print(": ");
        Serial.print(distance);
        Serial.println(" m");

        // Trigger emergency stop via flight controller
        triggerEmergencyStop();
    }
    else if (distance < WARNING_THRESHOLD) {
        // WARNING: Slow down
        Serial.println("PROXIMITY WARNING");
        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.println(" m");

        // Reduce velocity
        reduceVelocity(0.5);  // 50% speed
    }
}

void triggerEmergencyStop() {
    // Interface with flight controller (e.g., PWM, UART)
    // Set velocity to zero or trigger hover mode
    // Example: digitalWrite(EMERGENCY_STOP_PIN, HIGH);
}

void reduceVelocity(float factor) {
    // Interface with flight controller to reduce speed
    // Example: Send velocity command via serial
}
```

---

## 6. Testing Strategy

### 6.1 Test Plan

**Phase 1: Hardware Validation (Week 1)**

| Test | Objective | Setup | Pass Criteria |
|------|-----------|-------|---------------|
| 1.1 DW1000 Chip ID | Verify DW1000 responds | 1 Arduino + DW1000 | Chip ID: 0xDECA0130 |
| 1.2 Basic TX/RX | Verify radio works | 2 boards, 1m apart | Messages received |
| 1.3 Ranging Pair | Validate Two-Way Ranging | 1 anchor + 1 tag | Range ≈ actual distance ± 0.3m |

**Phase 2: Multi-Node Validation (Week 2)**

| Test | Objective | Setup | Pass Criteria |
|------|-----------|-------|---------------|
| 2.1 Multi-Anchor | Tag ranges with 3 anchors | 3 anchors + 1 tag | All 3 ranges reported |
| 2.2 Position Accuracy | Validate trilateration | Anchors at (0,0), (10,0), (5,10) | Error < 0.5m at 10 test points |
| 2.3 Update Rate | Measure ranging frequency | 3 anchors + 1 tag | ≥2 Hz update rate |

**Phase 3: Swarm Validation (Week 3-4)**

| Test | Objective | Setup | Pass Criteria |
|------|-----------|-------|---------------|
| 3.1 TDMA Multi-Tag | Collision-free ranging | 3 anchors + 3 tags | <5% packet loss |
| 3.2 Dynamic Tracking | Position during motion | Tag moving 0.5 m/s | Smooth trajectory, <0.5m error |
| 3.3 Long-Term Stability | System reliability | Run 2 hours continuous | No crashes, <10% packet loss |
| 3.4 Collision Detection | Early warning system | 2 tags approaching | Alert at 2m, stop at 1m |

### 6.2 Test Procedures

**Test 2.2: Position Accuracy Test**

**Setup:**
```
Anchor 1 at (0, 0, 1.5m)
Anchor 2 at (10, 0, 1.5m)
Anchor 3 at (5, 10, 1.5m)

Test positions (ground truth):
1. (2, 2)    6. (7, 3)
2. (5, 5)    7. (3, 7)
3. (8, 2)    8. (6, 8)
4. (4, 6)    9. (5, 3)
5. (6, 4)    10. (4, 4)
```

**Procedure:**
1. Deploy 3 anchors at known positions (use measuring tape)
2. Flash anchor firmware on 3 boards
3. Flash tag firmware on 1 board
4. For each test position:
   a. Place tag at position, mark with tape
   b. Wait 30 seconds for ranging to stabilize
   c. Record 10 position estimates from tag
   d. Compute mean and standard deviation
   e. Measure actual position with tape measure
   f. Calculate error: |computed - actual|

**Data Collection:**
```
Position | Actual X | Actual Y | Computed X | Computed Y | Error (m)
---------|----------|----------|------------|------------|----------
1        | 2.0      | 2.0      | 2.1        | 2.3        | 0.32
2        | 5.0      | 5.0      | 4.8        | 5.2        | 0.28
...
```

**Analysis:**
- Mean error across all positions
- Maximum error
- Standard deviation
- Positions with >0.5m error (investigate why)

### 6.3 Performance Metrics

**Key Metrics to Track:**

1. **Ranging Accuracy**
   - Mean error: target <0.3m
   - Maximum error: target <0.5m
   - Standard deviation: target <0.2m

2. **Update Rate**
   - Position updates per second: target >2 Hz
   - Measured via: Serial.println timestamps

3. **Reliability**
   - Packet loss rate: target <10%
   - Calculated: (sent - received) / sent
   - Time to first range: target <5 seconds

4. **System Uptime**
   - Continuous operation: target >1 hour
   - Resets/crashes: target 0
   - Memory leaks: monitor free RAM over time

**Logging:**
```cpp
void logMetrics() {
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 10000) {  // Log every 10s
        Serial.print("Uptime: ");
        Serial.print(millis() / 1000);
        Serial.print(" s, Free RAM: ");
        Serial.print(freeRAM());
        Serial.print(" bytes, Ranges: ");
        Serial.println(rangeCount);

        lastLog = millis();
    }
}

int freeRAM() {
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
```

---

## 7. Migration to ESP32 (Future Work)

### 7.1 When to Migrate

**Upgrade Triggers:**
- [ ] Swarm size >6 nodes
- [ ] Update rate >10 Hz required
- [ ] Dual-role firmware needed
- [ ] Memory errors on Arduino Uno
- [ ] Advanced algorithms (Kalman filter, path planning)

### 7.2 ESP32 Advantages

| Feature | Arduino Uno | ESP32 | Improvement |
|---------|-------------|-------|-------------|
| CPU | 16 MHz | 240 MHz | 15× faster |
| SRAM | 2 KB | 520 KB | 260× more |
| Flash | 32 KB | 4 MB | 125× more |
| WiFi/BT | No | Yes | Remote telemetry |
| FreeRTOS | No | Yes | Multi-tasking |

### 7.3 ESP32 Dual-Role Implementation

**Concept:** With ESP32's resources, dual-role becomes feasible.

```cpp
/*
 * ESP32 dual-role firmware (conceptual)
 * Uses FreeRTOS tasks for role management
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "DW1000Ranging.h"

enum NodeRole { ANCHOR, TAG, SWITCHING };
NodeRole currentRole = ANCHOR;

void setup() {
    Serial.begin(115200);
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Start in anchor mode
    startAnchorRole();

    // Create tasks
    xTaskCreate(rangingTask, "Ranging", 4096, NULL, 1, NULL);
    xTaskCreate(roleManagerTask, "RoleManager", 4096, NULL, 1, NULL);
}

void loop() {
    // Empty - all work in tasks
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void rangingTask(void* parameter) {
    while (true) {
        if (currentRole == ANCHOR || currentRole == TAG) {
            DW1000Ranging.loop();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void roleManagerTask(void* parameter) {
    while (true) {
        // Check for role switch command (e.g., via WiFi)
        if (shouldSwitchRole()) {
            currentRole = SWITCHING;

            // Stop current role
            // (ESP32 has enough RAM to just reinitialize)

            if (currentRole == ANCHOR) {
                startTagRole();
            } else {
                startAnchorRole();
            }
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void startAnchorRole() {
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C",
                                 DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    currentRole = ANCHOR;
    Serial.println("Role: ANCHOR");
}

void startTagRole() {
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                              DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    currentRole = TAG;
    Serial.println("Role: TAG");
}

bool shouldSwitchRole() {
    // Check for role switch command via WiFi, serial, or logic
    // Example: If anchor loses GPS, switch to tag mode
    return false;  // Placeholder
}
```

---

## 8. References

### 8.1 Existing Documentation

1. **UWB Swarm Ranging Architecture Research**
   File: `/home/devel/Desktop/SwarmLoc/findings/UWB_Swarm_Ranging_Architecture_Research.md`
   Comprehensive research on dual-role architectures, network topologies, and DW1000 capabilities.

2. **UWB Communication Saturation Mitigation**
   File: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/docs/findings/UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md`
   Detailed analysis of TDMA, CSMA/CA, and scalability limits.

### 8.2 Library Source Code

1. **DW1000Ranging.h**
   File: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000Ranging.h`
   Header file showing `startAsAnchor()` and `startAsTag()` declarations (lines 84-85).

2. **DW1000Ranging.cpp**
   File: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000Ranging.cpp`
   Implementation of role-specific protocol handling (lines 162-223, 515-678).

3. **DW1000Device.h**
   File: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src/DW1000Device.h`
   Device structure showing timestamp fields (lines 82-87) and memory footprint (74 bytes).

### 8.3 Example Firmware

1. **Tag Example**
   File: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/examples/DW1000Ranging_TAG/DW1000Ranging_TAG.ino`
   Working example of tag firmware using DW1000Ranging library.

2. **Anchor Example**
   File: `/home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/examples/DW1000Ranging_ANCHOR/DW1000Ranging_ANCHOR.ino`
   Working example of anchor firmware using DW1000Ranging library.

### 8.4 Academic References

1. **IEEE 802.15.4a-2007 Standard**
   UWB PHY layer specification defining Two-Way Ranging protocol.

2. **Decawave DW1000 User Manual**
   Chip-level documentation for register programming and advanced features.

3. **"Time-Slotted UWB Ranging for Swarm Robotics"**
   Research on TDMA implementation for multi-robot systems.

---

## 9. Conclusions and Recommendations

### 9.1 Summary of Findings

**Question 1: Should each drone support BOTH anchor and tag roles?**
- **Answer:** NO for Arduino Uno, YES for ESP32 if needed.
- **Reason:** Arduino Uno lacks memory and library does not support role switching.

**Question 2: Use dedicated anchor/tag firmware?**
- **Answer:** YES - strongly recommended.
- **Reason:** Proven approach, works with existing library, simpler debugging.

**Question 3: Can DW1000Ranging switch roles dynamically?**
- **Answer:** NO - not implemented in library.
- **Reason:** Fixed role set at initialization, no mechanism to change at runtime.

**Question 4: Which topology is best for 5-10 drone swarms?**
- **Answer:**
  - 5-6 drones: **Multi-anchor star** with TDMA (Arduino Uno)
  - 6-10 drones: **Hybrid topology** (requires ESP32)

### 9.2 Final Recommendations

**For Current Arduino Uno Setup:**

1. **Use separate firmware** (anchor.ino and tag.ino)
2. **Deploy 3 anchor drones** at fixed positions
3. **Deploy 1-3 tag drones** with TDMA scheduling
4. **Target 2.5 Hz** position update rate
5. **Plan ESP32 migration** for swarms >6 nodes

**Deployment Checklist:**
- [ ] Flash 3 boards with anchor firmware (unique addresses)
- [ ] Flash 1-3 boards with tag firmware (unique TDMA slots)
- [ ] Measure and record anchor positions
- [ ] Test single pair ranging (1 anchor, 1 tag)
- [ ] Test multi-anchor ranging (3 anchors, 1 tag)
- [ ] Validate position computation accuracy
- [ ] Deploy on drones and test in flight

**Code Structure:**
```
SwarmLoc/
├─ firmware/
│  ├─ anchor_v1/anchor_v1.ino          # Ready to flash
│  ├─ tag_v1/tag_v1.ino                # Ready to flash
│  └─ tag_tdma_v2/tag_tdma_v2.ino      # For multi-tag swarms
├─ docs/
│  └─ DUAL_ROLE_ARCHITECTURE.md        # This document
└─ test_data/
   └─ position_accuracy_test.csv       # Test results
```

### 9.3 Next Steps

**Week 1-2: Implement and Test**
1. Adapt anchor and tag templates to your drone platform
2. Run hardware validation tests (see Section 6.1)
3. Measure accuracy and update rate
4. Document findings

**Week 3-4: Deploy on Drones**
1. Integrate with flight controller
2. Test collision avoidance logic
3. Validate position tracking during flight
4. Collect flight test data

**Future: ESP32 Migration**
1. Port firmware to ESP32 when swarm grows
2. Implement dual-role capability
3. Add WiFi telemetry for ground station
4. Scale to 10+ drone swarm

---

**Document Version:** 1.0
**Created:** 2026-01-11
**Status:** Ready for Implementation
**Next Review:** After initial flight tests
