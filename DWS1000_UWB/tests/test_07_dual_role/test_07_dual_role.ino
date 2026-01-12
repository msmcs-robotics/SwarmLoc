/**
 * Test 7: Dual-Role UWB Firmware Prototype
 *
 * Project: SwarmLoc - GPS-Denied Positioning System
 * Hardware: Arduino Uno + DW1000 (PCL298336 v1.3 Shield)
 * Date: 2026-01-11
 *
 * PROOF OF CONCEPT: Role-switching firmware for drone swarms
 *
 * Features:
 * - Switch between ANCHOR and TAG roles via serial commands
 * - 'A' = Become ANCHOR (responder/fixed position)
 * - 'T' = Become TAG (initiator/mobile)
 * - 'S' = Show current status
 * - 'R' = Reset and restart
 *
 * Architecture:
 * - Uses compile-time role selection (DW1000Ranging limitation)
 * - Role switch requires full Arduino reset
 * - EEPROM stores desired role for persistence
 * - Simple TDMA slot configuration for multi-node operation
 *
 * Design Decisions:
 * 1. CANNOT use DW1000Ranging.startAsAnchor()/startAsTag() dynamically
 *    - Library limitation: role is fixed at initialization
 *    - Workaround: Store role in EEPROM, reset Arduino to switch
 *
 * 2. Memory constraints (Arduino Uno: 2KB SRAM)
 *    - Single role active at a time
 *    - Minimal state tracking
 *
 * 3. TDMA timing for multi-node operation
 *    - Configurable time slots (default: 150ms per node)
 *    - Node ID determines slot assignment
 *
 * Limitations:
 * - Role switching requires Arduino reset (not runtime switching)
 * - This is a proof-of-concept, not production-ready
 * - For true dual-role, migrate to ESP32 with custom protocol
 *
 * See: docs/findings/DUAL_ROLE_ARCHITECTURE.md for full research
 */

#include <SPI.h>
#include <EEPROM.h>
#include "DW1000Ranging.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

// Pin Configuration
const uint8_t PIN_RST = 9;  // Reset pin
const uint8_t PIN_IRQ = 2;  // Interrupt pin (INT0)
const uint8_t PIN_SS = SS;  // SPI chip select (D10)

// Node Configuration
const uint8_t NODE_ID = 1;  // Unique node ID (1-4)
const char* NODE_NAME = "Node01";

// Address Configuration (unique per node)
// Format: "AA:BB:CC:DD:EE:FF:GG:HH"
const char* MY_ANCHOR_ADDRESS = "82:17:5B:D5:A9:9A:E2:9A";  // Address when acting as anchor
const char* MY_TAG_ADDRESS = "7D:00:22:EA:82:60:3B:9A";     // Address when acting as tag

// TDMA Configuration (for multi-node operation)
#define NUM_NODES 4
#define SLOT_DURATION_MS 150
#define FRAME_DURATION_MS (NUM_NODES * SLOT_DURATION_MS)
#define MY_SLOT_ID (NODE_ID - 1)  // 0-indexed slot ID

// EEPROM Configuration
#define EEPROM_ROLE_ADDR 0  // Address to store role
#define EEPROM_MAGIC_ADDR 1 // Magic byte to verify initialization
#define EEPROM_MAGIC_VALUE 0x42

// Role Enumeration
enum NodeRole {
    ROLE_ANCHOR = 'A',
    ROLE_TAG = 'T',
    ROLE_UNDEFINED = 0
};

// ============================================================================
// GLOBAL STATE
// ============================================================================

NodeRole currentRole = ROLE_UNDEFINED;
uint32_t frameStartTime = 0;
uint32_t lastHeartbeat = 0;
uint32_t rangeCount = 0;
uint32_t errorCount = 0;
bool tdmaEnabled = false;

// Forward declarations
void newRange();
void newBlink(DW1000Device* device);
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);
void processSerialCommand();
void printHelp();
void printStatus();
void saveRoleToEEPROM(NodeRole role);
NodeRole loadRoleFromEEPROM();
void resetAndRestart();
int freeRAM();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Initialize Serial
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("SwarmLoc Dual-Role Firmware v1.0");
    Serial.println("========================================");
    Serial.print("Node: ");
    Serial.println(NODE_NAME);
    Serial.print("Node ID: ");
    Serial.println(NODE_ID);
    Serial.print("TDMA Slot: ");
    Serial.println(MY_SLOT_ID);
    Serial.println();

    // Check if EEPROM is initialized
    if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
        Serial.println("First boot - initializing EEPROM...");
        EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
        EEPROM.write(EEPROM_ROLE_ADDR, ROLE_TAG);  // Default to TAG role
        Serial.println("Default role: TAG");
    }

    // Load role from EEPROM
    currentRole = loadRoleFromEEPROM();

    if (currentRole != ROLE_ANCHOR && currentRole != ROLE_TAG) {
        Serial.println("WARNING: Invalid role in EEPROM, defaulting to TAG");
        currentRole = ROLE_TAG;
        saveRoleToEEPROM(currentRole);
    }

    Serial.print("Current role: ");
    Serial.println(currentRole == ROLE_ANCHOR ? "ANCHOR" : "TAG");
    Serial.println();

    // Initialize DW1000
    Serial.println("Initializing DW1000...");
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Configure based on role
    if (currentRole == ROLE_ANCHOR) {
        Serial.println("Starting as ANCHOR (responder/fixed position)");
        Serial.print("Address: ");
        Serial.println(MY_ANCHOR_ADDRESS);

        DW1000Ranging.attachNewRange(newRange);
        DW1000Ranging.attachBlinkDevice(newBlink);
        DW1000Ranging.attachInactiveDevice(inactiveDevice);

        DW1000Ranging.startAsAnchor(MY_ANCHOR_ADDRESS,
                                     DW1000.MODE_LONGDATA_RANGE_ACCURACY);

        Serial.println("ANCHOR mode active - waiting for tags...");
    }
    else if (currentRole == ROLE_TAG) {
        Serial.println("Starting as TAG (initiator/mobile)");
        Serial.print("Address: ");
        Serial.println(MY_TAG_ADDRESS);

        DW1000Ranging.attachNewRange(newRange);
        DW1000Ranging.attachNewDevice(newDevice);
        DW1000Ranging.attachInactiveDevice(inactiveDevice);

        DW1000Ranging.startAsTag(MY_TAG_ADDRESS,
                                  DW1000.MODE_LONGDATA_RANGE_ACCURACY);

        Serial.println("TAG mode active - searching for anchors...");

        // Enable TDMA for tags if multiple nodes
        if (NUM_NODES > 1) {
            tdmaEnabled = true;
            frameStartTime = millis();
            Serial.print("TDMA enabled - Slot ");
            Serial.print(MY_SLOT_ID);
            Serial.print(" of ");
            Serial.println(NUM_NODES);
        }
    }

    Serial.println();
    Serial.println("Commands:");
    Serial.println("  A - Switch to ANCHOR role");
    Serial.println("  T - Switch to TAG role");
    Serial.println("  S - Show status");
    Serial.println("  R - Reset and restart");
    Serial.println("  H - Help");
    Serial.println();
    Serial.println("Ready!");
    Serial.println("========================================");
    Serial.println();

    lastHeartbeat = millis();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Process serial commands
    if (Serial.available() > 0) {
        processSerialCommand();
    }

    // TDMA slot management for TAG mode
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

        // Re-synchronize frame every 10 seconds
        if (currentTime - frameStartTime > 10000) {
            frameStartTime = currentTime;
        }
    } else {
        // ANCHOR mode or single-node TAG mode - always active
        DW1000Ranging.loop();
    }

    // Periodic heartbeat (every 10 seconds)
    if (millis() - lastHeartbeat > 10000) {
        Serial.print("[HEARTBEAT] Uptime: ");
        Serial.print(millis() / 1000);
        Serial.print("s | Ranges: ");
        Serial.print(rangeCount);
        Serial.print(" | Errors: ");
        Serial.print(errorCount);
        Serial.print(" | Free RAM: ");
        Serial.print(freeRAM());
        Serial.println(" bytes");

        lastHeartbeat = millis();
    }
}

// ============================================================================
// DW1000 CALLBACKS
// ============================================================================

void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();

    Serial.print("[RANGE] ");
    if (currentRole == ROLE_ANCHOR) {
        Serial.print("Tag ");
    } else {
        Serial.print("Anchor ");
    }
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" | Distance: ");
    Serial.print(device->getRange(), 2);
    Serial.print(" m | RX Power: ");
    Serial.print(device->getRXPower(), 1);
    Serial.println(" dBm");

    rangeCount++;
}

void newBlink(DW1000Device* device) {
    Serial.print("[DISCOVERY] New tag detected: 0x");
    Serial.println(device->getShortAddress(), HEX);
}

void newDevice(DW1000Device* device) {
    Serial.print("[DISCOVERY] New anchor found: 0x");
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("[DISCONNECT] Device lost: 0x");
    Serial.println(device->getShortAddress(), HEX);
    errorCount++;
}

// ============================================================================
// SERIAL COMMAND PROCESSING
// ============================================================================

void processSerialCommand() {
    char cmd = Serial.read();

    // Convert to uppercase
    if (cmd >= 'a' && cmd <= 'z') {
        cmd = cmd - 32;
    }

    switch (cmd) {
        case 'A':
            // Switch to ANCHOR role
            Serial.println();
            Serial.println("========================================");
            Serial.println("ROLE SWITCH REQUEST: ANCHOR");
            Serial.println("========================================");

            if (currentRole == ROLE_ANCHOR) {
                Serial.println("Already in ANCHOR mode!");
            } else {
                Serial.println("Saving ANCHOR role to EEPROM...");
                saveRoleToEEPROM(ROLE_ANCHOR);
                Serial.println("Role saved. Resetting Arduino...");
                delay(1000);
                resetAndRestart();
            }
            break;

        case 'T':
            // Switch to TAG role
            Serial.println();
            Serial.println("========================================");
            Serial.println("ROLE SWITCH REQUEST: TAG");
            Serial.println("========================================");

            if (currentRole == ROLE_TAG) {
                Serial.println("Already in TAG mode!");
            } else {
                Serial.println("Saving TAG role to EEPROM...");
                saveRoleToEEPROM(ROLE_TAG);
                Serial.println("Role saved. Resetting Arduino...");
                delay(1000);
                resetAndRestart();
            }
            break;

        case 'S':
            // Show status
            printStatus();
            break;

        case 'R':
            // Reset and restart
            Serial.println();
            Serial.println("Manual reset requested...");
            delay(500);
            resetAndRestart();
            break;

        case 'H':
        case '?':
            // Help
            printHelp();
            break;

        default:
            // Ignore unknown commands
            break;
    }
}

void printHelp() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("HELP - Available Commands");
    Serial.println("========================================");
    Serial.println("A - Switch to ANCHOR role (responder)");
    Serial.println("    - Fixed position, responds to tags");
    Serial.println("    - Arduino will reset to apply change");
    Serial.println();
    Serial.println("T - Switch to TAG role (initiator)");
    Serial.println("    - Mobile position, ranges with anchors");
    Serial.println("    - Arduino will reset to apply change");
    Serial.println();
    Serial.println("S - Show current status");
    Serial.println("    - Role, ranges, memory, uptime");
    Serial.println();
    Serial.println("R - Reset and restart");
    Serial.println("    - Performs software reset");
    Serial.println();
    Serial.println("H - Show this help message");
    Serial.println("========================================");
    Serial.println();
}

void printStatus() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("SYSTEM STATUS");
    Serial.println("========================================");
    Serial.print("Node Name: ");
    Serial.println(NODE_NAME);
    Serial.print("Node ID: ");
    Serial.println(NODE_ID);
    Serial.print("Current Role: ");
    Serial.println(currentRole == ROLE_ANCHOR ? "ANCHOR" : "TAG");
    Serial.print("Address: ");
    Serial.println(currentRole == ROLE_ANCHOR ? MY_ANCHOR_ADDRESS : MY_TAG_ADDRESS);
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    Serial.print("Range Count: ");
    Serial.println(rangeCount);
    Serial.print("Error Count: ");
    Serial.println(errorCount);
    Serial.print("Free RAM: ");
    Serial.print(freeRAM());
    Serial.println(" bytes");

    if (tdmaEnabled) {
        Serial.println();
        Serial.println("TDMA Configuration:");
        Serial.print("  Enabled: YES");
        Serial.println();
        Serial.print("  My Slot: ");
        Serial.print(MY_SLOT_ID);
        Serial.print(" of ");
        Serial.println(NUM_NODES);
        Serial.print("  Slot Duration: ");
        Serial.print(SLOT_DURATION_MS);
        Serial.println(" ms");
        Serial.print("  Frame Duration: ");
        Serial.print(FRAME_DURATION_MS);
        Serial.println(" ms");
    } else {
        Serial.println();
        Serial.println("TDMA: Disabled (single node or anchor mode)");
    }

    Serial.println("========================================");
    Serial.println();
}

// ============================================================================
// EEPROM FUNCTIONS
// ============================================================================

void saveRoleToEEPROM(NodeRole role) {
    EEPROM.write(EEPROM_ROLE_ADDR, (uint8_t)role);
}

NodeRole loadRoleFromEEPROM() {
    uint8_t roleValue = EEPROM.read(EEPROM_ROLE_ADDR);
    return (NodeRole)roleValue;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void resetAndRestart() {
    Serial.println("Resetting in 3...");
    delay(1000);
    Serial.println("2...");
    delay(1000);
    Serial.println("1...");
    delay(1000);

    // Software reset using watchdog timer
    asm volatile ("jmp 0");
}

int freeRAM() {
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
