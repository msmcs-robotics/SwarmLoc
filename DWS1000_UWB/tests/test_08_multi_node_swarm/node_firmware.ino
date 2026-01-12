/**
 * Test 8: Multi-Node Swarm Firmware
 *
 * Project: SwarmLoc - GPS-Denied Positioning System
 * Hardware: Arduino Uno + DW1000 (PCL298336 v1.3 Shield)
 * Date: 2026-01-11
 *
 * PURPOSE: Unified firmware for 3-5 node swarm testing
 *
 * Features:
 * - Configurable node ID (1-5)
 * - Automatic role assignment (Node 1 = coordinator/anchor, 2+ = mobile tags)
 * - TDMA time slot allocation to prevent collisions
 * - Inter-node ranging with all nodes
 * - Position calculation (if 3+ anchors available)
 * - Message passing capability via serial
 * - LED status indicators
 * - Structured serial output for logging
 *
 * Configuration:
 * - Edit config.h to set node ID before uploading
 * - Node 1 is always the coordinator (anchor)
 * - Nodes 2+ are mobile tags with assigned TDMA slots
 *
 * Output Format:
 * CSV: timestamp,node_id,target_id,distance,rx_power
 *
 * See: README.md for setup and usage instructions
 */

#include <SPI.h>
#include <EEPROM.h>
#include "DW1000Ranging.h"
#include "config.h"

// ============================================================================
// PIN CONFIGURATION
// ============================================================================

const uint8_t PIN_RST = 9;   // DW1000 Reset
const uint8_t PIN_IRQ = 2;   // DW1000 Interrupt (INT0)
const uint8_t PIN_SS = SS;   // SPI Chip Select (D10)
const uint8_t LED_PIN = 13;  // Status LED (built-in)

// ============================================================================
// ROLE DETERMINATION
// ============================================================================

enum NodeRole {
    COORDINATOR,  // Node 1 - Acts as primary anchor and coordinator
    MOBILE        // Nodes 2+ - Mobile tags
};

NodeRole myRole = (NODE_ID == 1) ? COORDINATOR : MOBILE;

// ============================================================================
// GLOBAL STATE
// ============================================================================

// TDMA timing
uint32_t frameStartTime = 0;
uint32_t slotStartTime = 0;
bool inMySlot = false;

// Statistics
uint32_t rangeCount = 0;
uint32_t errorCount = 0;
uint32_t lastHeartbeat = 0;
uint32_t lastLEDBlink = 0;
bool ledState = false;

// Position tracking (for mobile nodes)
struct Position {
    float x, y, z;
    bool valid;
    uint32_t timestamp;
};

Position myPosition = {0.0, 0.0, 0.0, false, 0};

// Range tracking for all nodes
struct RangeData {
    float distance;
    float rxPower;
    uint32_t timestamp;
    bool valid;
};

RangeData ranges[MAX_NODES];  // Range to each other node

// Known anchor positions (only Node 1 initially)
Position anchorPositions[MAX_NODES] = {
    {0.0, 0.0, COORDINATOR_HEIGHT, true, 0},  // Node 1 (coordinator)
    {0.0, 0.0, 0.0, false, 0},                // Node 2 (unknown until calculated)
    {0.0, 0.0, 0.0, false, 0},                // Node 3
    {0.0, 0.0, 0.0, false, 0},                // Node 4
    {0.0, 0.0, 0.0, false, 0}                 // Node 5
};

// Forward declarations
void newRange();
void newBlink(DW1000Device* device);
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);
void updatePosition();
void printRangeData(uint16_t targetAddr, float distance, float rxPower);
void printStatus();
void blinkLED();
int freeRAM();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Initialize Serial
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Print banner
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("SwarmLoc Multi-Node Swarm Test"));
    Serial.println(F("========================================"));
    Serial.print(F("Node ID: "));
    Serial.println(NODE_ID);
    Serial.print(F("Role: "));
    Serial.println(myRole == COORDINATOR ? F("COORDINATOR") : F("MOBILE"));

    if (myRole == MOBILE) {
        Serial.print(F("TDMA Slot: "));
        Serial.println(MY_SLOT_ID);
    }

    Serial.print(F("Max Nodes: "));
    Serial.println(MAX_NODES);
    Serial.println();

    // Initialize ranges
    for (int i = 0; i < MAX_NODES; i++) {
        ranges[i].valid = false;
        ranges[i].distance = 0.0;
        ranges[i].rxPower = 0.0;
        ranges[i].timestamp = 0;
    }

    // Initialize DW1000
    Serial.println(F("Initializing DW1000..."));
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Attach callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start in appropriate role
    if (myRole == COORDINATOR) {
        // Coordinator is an anchor
        Serial.println(F("Starting as COORDINATOR (anchor)..."));
        Serial.print(F("Address: "));
        Serial.println(COORD_ADDRESS);

        DW1000Ranging.attachBlinkDevice(newBlink);
        DW1000Ranging.startAsAnchor(COORD_ADDRESS, DW1000.MODE_LONGDATA_RANGE_ACCURACY);

        Serial.println(F("COORDINATOR ready - waiting for mobile nodes..."));
    } else {
        // Mobile node is a tag
        Serial.println(F("Starting as MOBILE (tag)..."));
        Serial.print(F("Address: "));
        Serial.println(MY_ADDRESS);

        DW1000Ranging.attachNewDevice(newDevice);
        DW1000Ranging.startAsTag(MY_ADDRESS, DW1000.MODE_LONGDATA_RANGE_ACCURACY);

        Serial.println(F("MOBILE ready - searching for anchors..."));

        // Initialize TDMA
        frameStartTime = millis();
        Serial.print(F("TDMA initialized - Frame: "));
        Serial.print(FRAME_DURATION_MS);
        Serial.print(F("ms, Slot: "));
        Serial.print(SLOT_DURATION_MS);
        Serial.println(F("ms"));
    }

    // Print CSV header
    Serial.println();
    Serial.println(F("CSV Output Format:"));
    Serial.println(F("timestamp,node_id,target_id,distance,rx_power"));
    Serial.println(F("========================================"));
    Serial.println();

    lastHeartbeat = millis();
    lastLEDBlink = millis();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    uint32_t currentTime = millis();

    // TDMA management for mobile nodes
    if (myRole == MOBILE && ENABLE_TDMA) {
        uint32_t timeInFrame = (currentTime - frameStartTime) % FRAME_DURATION_MS;
        uint8_t currentSlot = timeInFrame / SLOT_DURATION_MS;

        if (currentSlot == MY_SLOT_ID) {
            if (!inMySlot) {
                // Entering my slot
                inMySlot = true;
                slotStartTime = currentTime;

                if (DEBUG_TDMA) {
                    Serial.print(F("[TDMA] Slot "));
                    Serial.print(MY_SLOT_ID);
                    Serial.println(F(" START"));
                }
            }

            // Active ranging during my slot
            DW1000Ranging.loop();

        } else {
            if (inMySlot) {
                // Exiting my slot
                inMySlot = false;

                if (DEBUG_TDMA) {
                    Serial.print(F("[TDMA] Slot "));
                    Serial.print(MY_SLOT_ID);
                    Serial.println(F(" END"));
                }
            }

            // Idle during other slots
            delay(10);
        }

        // Re-sync frame periodically
        if (currentTime - frameStartTime > FRAME_RESYNC_MS) {
            frameStartTime = currentTime;
            if (DEBUG_TDMA) {
                Serial.println(F("[TDMA] Frame resync"));
            }
        }

    } else {
        // Coordinator mode - always active
        DW1000Ranging.loop();
    }

    // Update position calculation for mobile nodes
    if (myRole == MOBILE && ENABLE_POSITION_CALC) {
        static uint32_t lastPositionUpdate = 0;
        if (currentTime - lastPositionUpdate > POSITION_UPDATE_MS) {
            updatePosition();
            lastPositionUpdate = currentTime;
        }
    }

    // Status LED blink
    if (currentTime - lastLEDBlink > LED_BLINK_MS) {
        blinkLED();
        lastLEDBlink = currentTime;
    }

    // Periodic heartbeat
    if (currentTime - lastHeartbeat > HEARTBEAT_MS) {
        printStatus();
        lastHeartbeat = currentTime;
    }

    // Process serial commands
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'S' || cmd == 's') {
            printStatus();
        }
    }
}

// ============================================================================
// DW1000 CALLBACKS
// ============================================================================

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();
    uint16_t addr = device->getShortAddress();
    float distance = device->getRange();
    float rxPower = device->getRXPower();

    // Determine which node this is
    int targetNodeId = -1;
    if (myRole == COORDINATOR) {
        // Coordinator receiving from mobile nodes (2-5)
        // Mobile addresses are configured with node ID in last byte
        for (int i = 1; i < MAX_NODES; i++) {
            // Extract last byte of address to match node ID
            // This is a simplified matching - in production use address table
            targetNodeId = i + 1;  // Will be refined based on address mapping
            break;  // For now, accept any mobile node
        }
    } else {
        // Mobile node receiving from coordinator or other nodes
        if (addr == (uint16_t)(COORD_ADDRESS[6] << 8 | COORD_ADDRESS[7])) {
            targetNodeId = 1;  // Coordinator
        }
    }

    // Update range data
    if (targetNodeId > 0 && targetNodeId <= MAX_NODES) {
        int idx = targetNodeId - 1;
        ranges[idx].distance = distance;
        ranges[idx].rxPower = rxPower;
        ranges[idx].timestamp = millis();
        ranges[idx].valid = true;
    }

    // Print range data
    printRangeData(addr, distance, rxPower);
    rangeCount++;

    // LED activity indicator
    digitalWrite(LED_PIN, HIGH);
    delay(5);
    digitalWrite(LED_PIN, LOW);
}

void newBlink(DW1000Device* device) {
    Serial.print(F("[DISCOVER] New mobile node: 0x"));
    Serial.println(device->getShortAddress(), HEX);
}

void newDevice(DW1000Device* device) {
    Serial.print(F("[DISCOVER] New anchor found: 0x"));
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print(F("[DISCONNECT] Device lost: 0x"));
    Serial.println(device->getShortAddress(), HEX);
    errorCount++;
}

// ============================================================================
// POSITION CALCULATION
// ============================================================================

void updatePosition() {
    // For now, implement simple 2D trilateration with coordinator as reference
    // Need at least 3 valid ranges for positioning

    int validRangeCount = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        if (ranges[i].valid && anchorPositions[i].valid) {
            validRangeCount++;
        }
    }

    if (validRangeCount < 3) {
        // Not enough anchors for trilateration
        myPosition.valid = false;
        return;
    }

    // Simple 2D trilateration using first 3 valid anchors
    // In a real implementation, use least-squares with all available anchors

    int anchorIdx[3] = {-1, -1, -1};
    int count = 0;
    for (int i = 0; i < MAX_NODES && count < 3; i++) {
        if (ranges[i].valid && anchorPositions[i].valid) {
            anchorIdx[count++] = i;
        }
    }

    if (count < 3) {
        myPosition.valid = false;
        return;
    }

    // Get anchor positions and ranges
    float x1 = anchorPositions[anchorIdx[0]].x;
    float y1 = anchorPositions[anchorIdx[0]].y;
    float r1 = ranges[anchorIdx[0]].distance;

    float x2 = anchorPositions[anchorIdx[1]].x;
    float y2 = anchorPositions[anchorIdx[1]].y;
    float r2 = ranges[anchorIdx[1]].distance;

    float x3 = anchorPositions[anchorIdx[2]].x;
    float y3 = anchorPositions[anchorIdx[2]].y;
    float r3 = ranges[anchorIdx[2]].distance;

    // Trilateration equations
    float A = 2 * (x2 - x1);
    float B = 2 * (y2 - y1);
    float C = r1*r1 - r2*r2 - x1*x1 + x2*x2 - y1*y1 + y2*y2;

    float D = 2 * (x3 - x2);
    float E = 2 * (y3 - y2);
    float F = r2*r2 - r3*r3 - x2*x2 + x3*x3 - y2*y2 + y3*y3;

    float denom = (E*A - B*D);

    if (abs(denom) > 0.001) {
        myPosition.x = (C*E - F*B) / denom;
        myPosition.y = (C*D - A*F) / (B*D - A*E);
        myPosition.z = DEFAULT_TAG_HEIGHT;
        myPosition.valid = true;
        myPosition.timestamp = millis();

        if (DEBUG_POSITION) {
            Serial.print(F("[POSITION] Node "));
            Serial.print(NODE_ID);
            Serial.print(F(": ("));
            Serial.print(myPosition.x, 2);
            Serial.print(F(", "));
            Serial.print(myPosition.y, 2);
            Serial.print(F(", "));
            Serial.print(myPosition.z, 2);
            Serial.println(F(")"));
        }
    } else {
        myPosition.valid = false;
    }
}

// ============================================================================
// OUTPUT FUNCTIONS
// ============================================================================

void printRangeData(uint16_t targetAddr, float distance, float rxPower) {
    // CSV format: timestamp,node_id,target_id,distance,rx_power
    Serial.print(millis());
    Serial.print(F(","));
    Serial.print(NODE_ID);
    Serial.print(F(","));
    Serial.print(targetAddr, HEX);
    Serial.print(F(","));
    Serial.print(distance, 3);
    Serial.print(F(","));
    Serial.println(rxPower, 2);
}

void printStatus() {
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("STATUS"));
    Serial.println(F("========================================"));
    Serial.print(F("Node ID: "));
    Serial.println(NODE_ID);
    Serial.print(F("Role: "));
    Serial.println(myRole == COORDINATOR ? F("COORDINATOR") : F("MOBILE"));
    Serial.print(F("Uptime: "));
    Serial.print(millis() / 1000);
    Serial.println(F(" s"));
    Serial.print(F("Ranges: "));
    Serial.println(rangeCount);
    Serial.print(F("Errors: "));
    Serial.println(errorCount);
    Serial.print(F("Free RAM: "));
    Serial.print(freeRAM());
    Serial.println(F(" bytes"));

    // Show valid ranges
    Serial.println();
    Serial.println(F("Valid Ranges:"));
    for (int i = 0; i < MAX_NODES; i++) {
        if (ranges[i].valid && (millis() - ranges[i].timestamp < 5000)) {
            Serial.print(F("  Node "));
            Serial.print(i + 1);
            Serial.print(F(": "));
            Serial.print(ranges[i].distance, 2);
            Serial.print(F(" m ("));
            Serial.print(ranges[i].rxPower, 1);
            Serial.println(F(" dBm)"));
        }
    }

    // Show position if available
    if (myPosition.valid && myRole == MOBILE) {
        Serial.println();
        Serial.print(F("Position: ("));
        Serial.print(myPosition.x, 2);
        Serial.print(F(", "));
        Serial.print(myPosition.y, 2);
        Serial.print(F(", "));
        Serial.print(myPosition.z, 2);
        Serial.println(F(")"));
    }

    Serial.println(F("========================================"));
    Serial.println();
}

void blinkLED() {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

int freeRAM() {
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
