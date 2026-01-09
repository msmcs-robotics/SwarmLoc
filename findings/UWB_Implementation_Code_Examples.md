# UWB Swarm Implementation - Code Examples and Algorithms

**Companion Document to**: UWB_Swarm_Ranging_Architecture_Research.md
**Project**: SwarmLoc
**Date**: 2026-01-08

---

## Table of Contents

1. [Complete Initiator Implementation](#complete-initiator-implementation)
2. [Complete Responder Implementation](#complete-responder-implementation)
3. [Trilateration Algorithms](#trilateration-algorithms)
4. [TDMA Implementation](#tdma-implementation)
5. [Data Structures](#data-structures)
6. [Utility Functions](#utility-functions)

---

## 1. Complete Initiator Implementation

### Basic Initiator with TDMA

```cpp
/******************************************************************************
 * initiator.ino - UWB Tag for SwarmLoc
 *
 * Hardware: Arduino Uno + DW1000 (PCL298336 v1.3)
 * Function: Ranges with 3 anchors, computes position via trilateration
 * Protocol: SS-TWR with TDMA
 * Update Rate: 2.5 Hz
 ******************************************************************************/

#include <SPI.h>
#include <DW1000.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

// Hardware Pins
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Network Settings
#define MY_ADDRESS 0x0A
#define NETWORK_ID 0xDECA
#define NUM_ANCHORS 3

// TDMA Settings
#define SLOT_DURATION 100     // milliseconds per slot
#define SLOT_GUARD 10         // guard interval
#define TIMEOUT_MS 80         // response timeout

// Ranging Settings
#define REPLY_DELAY_US 3000   // microseconds

// Message Types
#define POLL 0
#define POLL_ACK 1
#define RANGE 2
#define RANGE_REPORT 3
#define RANGE_FAILED 255

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Anchor Configuration
uint8_t anchorAddresses[NUM_ANCHORS] = {0x01, 0x02, 0x03};

struct AnchorPosition {
    float x, y, z;
};

AnchorPosition anchorPositions[NUM_ANCHORS] = {
    {0.0, 0.0, 10.0},      // Anchor 1
    {10.0, 0.0, 10.0},     // Anchor 2
    {5.0, 10.0, 10.0}      // Anchor 3
};

// Ranging Data
float distances[NUM_ANCHORS] = {0.0, 0.0, 0.0};
bool distanceValid[NUM_ANCHORS] = {false, false, false};

// Timestamps (SS-TWR)
DW1000Time timePollSent;
DW1000Time timePollAckReceived;
DW1000Time timeRangeSent;

// Communication State
volatile bool sentAck = false;
volatile bool receivedAck = false;
byte data[16];

// Position
struct Position {
    float x, y, z;
    bool valid;
} myPosition = {0.0, 0.0, 0.0, false};

// State Machine
enum State {
    IDLE,
    POLL_SENT,
    WAITING_POLL_ACK,
    RANGE_SENT,
    WAITING_RANGE_REPORT
};
State currentState = IDLE;
uint32_t stateStartTime = 0;
uint8_t currentAnchorIndex = 0;

// Statistics
uint32_t successfulRanges = 0;
uint32_t failedRanges = 0;
uint32_t lastStatsTime = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("=== SwarmLoc TAG Initiator ==="));

    // Initialize DW1000
    initDW1000();

    Serial.println(F("Initialization complete"));
    Serial.println(F("Starting ranging cycle..."));
    Serial.println();

    lastStatsTime = millis();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // TDMA slot management
    uint32_t currentTime = millis();
    uint8_t currentSlot = (currentTime / SLOT_DURATION) % (NUM_ANCHORS + 1);

    if (currentSlot < NUM_ANCHORS) {
        // Ranging slot
        if (currentAnchorIndex != currentSlot) {
            // New slot started
            currentAnchorIndex = currentSlot;
            startRangingWithAnchor(currentAnchorIndex);
        } else {
            // Continue ranging state machine
            handleRangingStateMachine();
        }
    } else {
        // Processing slot (slot 3)
        if (currentState != IDLE) {
            // Ranging cycle complete, compute position
            currentState = IDLE;
            computeAndDisplayPosition();

            // Print statistics every second
            if (currentTime - lastStatsTime >= 1000) {
                printStatistics();
                lastStatsTime = currentTime;
            }
        }
    }
}

// ============================================================================
// DW1000 INITIALIZATION
// ============================================================================

void initDW1000() {
    // Initialize the driver
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    Serial.println(F("DW1000 initialized"));

    // Configuration
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(MY_ADDRESS);
    DW1000.setNetworkId(NETWORK_ID);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    Serial.println(F("Configuration committed"));

    // Print device info
    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device ID: "));
    Serial.println(msg);

    DW1000.getPrintableExtendedUniqueIdentifier(msg);
    Serial.print(F("Unique ID: "));
    Serial.println(msg);

    // Attach interrupt handlers
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);

    // Start in receive mode
    receiver();
}

// ============================================================================
// RANGING STATE MACHINE
// ============================================================================

void startRangingWithAnchor(uint8_t anchorIndex) {
    currentState = IDLE;
    distanceValid[anchorIndex] = false;

    // Send POLL message
    transmitPoll(anchorIndex);
    currentState = POLL_SENT;
    stateStartTime = millis();
}

void handleRangingStateMachine() {
    uint32_t elapsed = millis() - stateStartTime;

    switch (currentState) {
        case POLL_SENT:
            if (sentAck) {
                sentAck = false;
                DW1000.getTransmitTimestamp(timePollSent);
                currentState = WAITING_POLL_ACK;
                stateStartTime = millis();
            } else if (elapsed > TIMEOUT_MS) {
                handleTimeout("POLL send");
            }
            break;

        case WAITING_POLL_ACK:
            if (receivedAck) {
                receivedAck = false;
                DW1000.getData(data, 16);

                if (data[0] == POLL_ACK) {
                    DW1000.getReceiveTimestamp(timePollAckReceived);
                    transmitRange(currentAnchorIndex);
                    currentState = RANGE_SENT;
                    stateStartTime = millis();
                } else {
                    handleUnexpectedMessage(data[0]);
                }
            } else if (elapsed > TIMEOUT_MS) {
                handleTimeout("POLL_ACK");
            }
            break;

        case RANGE_SENT:
            if (sentAck) {
                sentAck = false;
                DW1000.getTransmitTimestamp(timeRangeSent);
                currentState = WAITING_RANGE_REPORT;
                stateStartTime = millis();
            } else if (elapsed > TIMEOUT_MS) {
                handleTimeout("RANGE send");
            }
            break;

        case WAITING_RANGE_REPORT:
            if (receivedAck) {
                receivedAck = false;
                DW1000.getData(data, 16);

                if (data[0] == RANGE_REPORT) {
                    float distance;
                    memcpy(&distance, data + 1, 4);
                    handleRangeReport(distance);
                } else if (data[0] == RANGE_FAILED) {
                    handleRangeFailed();
                } else {
                    handleUnexpectedMessage(data[0]);
                }
            } else if (elapsed > TIMEOUT_MS) {
                handleTimeout("RANGE_REPORT");
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// TRANSMISSION FUNCTIONS
// ============================================================================

void transmitPoll(uint8_t anchorIndex) {
    DW1000.newTransmit();
    DW1000.setDefaults();

    data[0] = POLL;
    data[1] = anchorAddresses[anchorIndex];  // Destination
    data[2] = MY_ADDRESS;                    // Source

    DW1000.setData(data, 16);
    DW1000.startTransmit();
}

void transmitRange(uint8_t anchorIndex) {
    DW1000.newTransmit();
    DW1000.setDefaults();

    data[0] = RANGE;

    // Include timestamps for anchor to compute distance
    timePollSent.getTimestamp(data + 1);
    timePollAckReceived.getTimestamp(data + 6);

    // Delayed transmission
    DW1000Time deltaTime = DW1000Time(REPLY_DELAY_US, DW1000Time::MICROSECONDS);
    timeRangeSent = DW1000.setDelay(deltaTime);
    timeRangeSent.getTimestamp(data + 11);

    DW1000.setData(data, 16);
    DW1000.startTransmit();
}

void receiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}

// ============================================================================
// INTERRUPT HANDLERS
// ============================================================================

void handleSent() {
    sentAck = true;
}

void handleReceived() {
    receivedAck = true;
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void handleRangeReport(float distance) {
    if (distance > 0.1 && distance < 100.0) {  // Sanity check
        distances[currentAnchorIndex] = distance;
        distanceValid[currentAnchorIndex] = true;
        successfulRanges++;
    } else {
        distanceValid[currentAnchorIndex] = false;
        failedRanges++;
    }
    currentState = IDLE;
}

void handleRangeFailed() {
    distanceValid[currentAnchorIndex] = false;
    failedRanges++;
    currentState = IDLE;
}

void handleTimeout(const char* stage) {
    Serial.print(F("Timeout: "));
    Serial.print(stage);
    Serial.print(F(" (Anchor "));
    Serial.print(currentAnchorIndex + 1);
    Serial.println(F(")"));

    distanceValid[currentAnchorIndex] = false;
    failedRanges++;
    currentState = IDLE;
}

void handleUnexpectedMessage(byte msgId) {
    Serial.print(F("Unexpected msg: 0x"));
    Serial.println(msgId, HEX);
    currentState = IDLE;
}

// ============================================================================
// POSITION COMPUTATION
// ============================================================================

void computeAndDisplayPosition() {
    // Count valid ranges
    int validCount = 0;
    for (int i = 0; i < NUM_ANCHORS; i++) {
        if (distanceValid[i]) validCount++;
    }

    // Need at least 3 valid ranges for 3D position
    if (validCount >= 3) {
        myPosition = trilaterate3D(anchorPositions, distances, NUM_ANCHORS);
        myPosition.valid = true;

        // Display position
        Serial.println(F("--- Position Update ---"));
        Serial.print(F("X: "));
        Serial.print(myPosition.x, 2);
        Serial.print(F(" m, Y: "));
        Serial.print(myPosition.y, 2);
        Serial.print(F(" m, Z: "));
        Serial.print(myPosition.z, 2);
        Serial.println(F(" m"));
    } else {
        myPosition.valid = false;
        Serial.print(F("Insufficient ranges ("));
        Serial.print(validCount);
        Serial.println(F("/3)"));
    }

    // Display individual ranges
    Serial.println(F("Distances:"));
    for (int i = 0; i < NUM_ANCHORS; i++) {
        Serial.print(F("  A"));
        Serial.print(i + 1);
        Serial.print(F(": "));
        if (distanceValid[i]) {
            Serial.print(distances[i], 2);
            Serial.println(F(" m"));
        } else {
            Serial.println(F("INVALID"));
        }
    }
    Serial.println();
}

// ============================================================================
// TRILATERATION ALGORITHM
// ============================================================================

Position trilaterate3D(AnchorPosition anchors[], float ranges[], int n) {
    Position result = {0.0, 0.0, 0.0, false};

    // Using least-squares approach for 3 anchors
    // Simplified algorithm - can be improved with more anchors

    if (n < 3) return result;

    // Extract anchor positions
    float x1 = anchors[0].x, y1 = anchors[0].y, z1 = anchors[0].z;
    float x2 = anchors[1].x, y2 = anchors[1].y, z2 = anchors[1].z;
    float x3 = anchors[2].x, y3 = anchors[2].y, z3 = anchors[2].z;

    float r1 = ranges[0];
    float r2 = ranges[1];
    float r3 = ranges[2];

    // Compute position using algebraic solution
    // Reference: https://en.wikipedia.org/wiki/Trilateration

    // Translate anchor1 to origin
    float x2_t = x2 - x1;
    float y2_t = y2 - y1;
    float z2_t = z2 - z1;

    float x3_t = x3 - x1;
    float y3_t = y3 - y1;
    float z3_t = z3 - z1;

    // Distance between anchors
    float d = sqrt(x2_t * x2_t + y2_t * y2_t + z2_t * z2_t);

    // Unit vector along x-axis
    float i_x = x2_t / d;
    float i_y = y2_t / d;
    float i_z = z2_t / d;

    // Dot product i · (anchor3 - anchor1)
    float i_p3 = i_x * x3_t + i_y * y3_t + i_z * z3_t;

    // Vector j
    float j_x = x3_t - i_p3 * i_x;
    float j_y = y3_t - i_p3 * i_y;
    float j_z = z3_t - i_p3 * i_z;

    float j_mag = sqrt(j_x * j_x + j_y * j_y + j_z * j_z);

    if (j_mag < 0.001) {
        // Anchors are collinear - cannot solve
        return result;
    }

    j_x /= j_mag;
    j_y /= j_mag;
    j_z /= j_mag;

    // Cross product k = i × j
    float k_x = i_y * j_z - i_z * j_y;
    float k_y = i_z * j_x - i_x * j_z;
    float k_z = i_x * j_y - i_y * j_x;

    // Coordinates in new system
    float j_p3 = j_x * x3_t + j_y * y3_t + j_z * z3_t;

    // Solve for x, y, z
    float x = (r1 * r1 - r2 * r2 + d * d) / (2 * d);
    float y = (r1 * r1 - r3 * r3 + i_p3 * i_p3 + j_p3 * j_p3) / (2 * j_p3) - (i_p3 / j_p3) * x;
    float z_sq = r1 * r1 - x * x - y * y;

    if (z_sq < 0) {
        // No solution (ranges inconsistent)
        return result;
    }

    float z = sqrt(z_sq);  // Take positive solution

    // Transform back to original coordinate system
    result.x = x1 + x * i_x + y * j_x + z * k_x;
    result.y = y1 + x * i_y + y * j_y + z * k_y;
    result.z = z1 + x * i_z + y * j_z + z * k_z;
    result.valid = true;

    return result;
}

// ============================================================================
// STATISTICS
// ============================================================================

void printStatistics() {
    float successRate = 0.0;
    uint32_t total = successfulRanges + failedRanges;

    if (total > 0) {
        successRate = (float)successfulRanges / total * 100.0;
    }

    Serial.print(F("Stats: Success="));
    Serial.print(successfulRanges);
    Serial.print(F(", Failed="));
    Serial.print(failedRanges);
    Serial.print(F(", Rate="));
    Serial.print(successRate, 1);
    Serial.println(F("%"));
}
```

---

## 2. Complete Responder Implementation

```cpp
/******************************************************************************
 * responder.ino - UWB Anchor for SwarmLoc
 *
 * Hardware: Arduino Uno + DW1000 (PCL298336 v1.3)
 * Function: Responds to ranging requests, computes distance
 * Protocol: SS-TWR with TDMA
 *
 * IMPORTANT: Set MY_ADDRESS and myPosition for each anchor
 ******************************************************************************/

#include <SPI.h>
#include <DW1000.h>

// ============================================================================
// CONFIGURATION - CHANGE FOR EACH ANCHOR
// ============================================================================

// Hardware Pins
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Network Settings
#define MY_ADDRESS 0x01       // CHANGE: 0x01, 0x02, 0x03 for each anchor
#define NETWORK_ID 0xDECA

// Anchor Position (known coordinates)
struct Position {
    float x, y, z;
};

// CHANGE FOR EACH ANCHOR:
Position myPosition = {0.0, 0.0, 10.0};  // Anchor 1: (0, 0, 10)
// Anchor 2: {10.0, 0.0, 10.0}
// Anchor 3: {5.0, 10.0, 10.0}

// Ranging Settings
#define REPLY_DELAY_US 3000

// Message Types
#define POLL 0
#define POLL_ACK 1
#define RANGE 2
#define RANGE_REPORT 3
#define RANGE_FAILED 255

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Timestamps (SS-TWR)
DW1000Time timePollSent;
DW1000Time timePollReceived;
DW1000Time timePollAckSent;
DW1000Time timePollAckReceived;
DW1000Time timeRangeSent;
DW1000Time timeRangeReceived;
DW1000Time timeComputedRange;

// Communication State
volatile bool sentAck = false;
volatile bool receivedAck = false;
volatile byte expectedMsgId = POLL;
byte data[16];

// Protocol State
bool protocolFailed = false;
uint32_t lastActivity = 0;
uint32_t resetPeriod = 250;

// Statistics
uint32_t successfulRanges = 0;
uint32_t failedRanges = 0;
uint32_t lastStatsTime = 0;
float samplingRate = 0.0;
uint16_t rangingCount = 0;
uint32_t rangingPeriodStart = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("=== SwarmLoc ANCHOR Responder ==="));
    Serial.print(F("Address: 0x"));
    Serial.println(MY_ADDRESS, HEX);
    Serial.print(F("Position: ("));
    Serial.print(myPosition.x, 2);
    Serial.print(F(", "));
    Serial.print(myPosition.y, 2);
    Serial.print(F(", "));
    Serial.print(myPosition.z, 2);
    Serial.println(F(")"));

    // Initialize DW1000
    initDW1000();

    Serial.println(F("Listening for POLL messages..."));
    Serial.println();

    lastStatsTime = millis();
    rangingPeriodStart = millis();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    uint32_t currentTime = millis();

    // Check for inactivity
    if (!sentAck && !receivedAck) {
        if (currentTime - lastActivity > resetPeriod) {
            resetInactive();
        }
        return;
    }

    // Handle sent messages
    if (sentAck) {
        sentAck = false;
        handleSentMessage();
    }

    // Handle received messages
    if (receivedAck) {
        receivedAck = false;
        handleReceivedMessage();
    }

    // Print statistics every second
    if (currentTime - lastStatsTime >= 1000) {
        printStatistics();
        lastStatsTime = currentTime;
    }
}

// ============================================================================
// DW1000 INITIALIZATION
// ============================================================================

void initDW1000() {
    // Initialize the driver
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    Serial.println(F("DW1000 initialized"));

    // Configuration
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(MY_ADDRESS);
    DW1000.setNetworkId(NETWORK_ID);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    Serial.println(F("Configuration committed"));

    // Print device info
    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device ID: "));
    Serial.println(msg);

    // Attach interrupt handlers
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);

    // Start in receive mode
    receiver();
    noteActivity();
}

// ============================================================================
// MESSAGE HANDLING
// ============================================================================

void handleSentMessage() {
    byte msgId = data[0];

    if (msgId == POLL_ACK) {
        DW1000.getTransmitTimestamp(timePollAckSent);
        noteActivity();
    } else if (msgId == RANGE_REPORT || msgId == RANGE_FAILED) {
        noteActivity();
    }
}

void handleReceivedMessage() {
    // Get message data
    DW1000.getData(data, 16);
    byte msgId = data[0];

    // Check if expected message
    if (msgId != expectedMsgId && msgId != POLL) {
        protocolFailed = true;
        return;
    }

    if (msgId == POLL) {
        // Reset protocol on new POLL
        protocolFailed = false;
        DW1000.getReceiveTimestamp(timePollReceived);
        expectedMsgId = RANGE;
        transmitPollAck();
        noteActivity();
    }
    else if (msgId == RANGE) {
        DW1000.getReceiveTimestamp(timeRangeReceived);
        expectedMsgId = POLL;

        if (!protocolFailed) {
            // Extract timestamps from RANGE message
            timePollSent.setTimestamp(data + 1);
            timePollAckReceived.setTimestamp(data + 6);
            timeRangeSent.setTimestamp(data + 11);

            // Compute range
            computeRangeAsymmetric();

            float distance = timeComputedRange.getAsMeters();

            if (distance > 0.1 && distance < 100.0) {
                transmitRangeReport(distance);
                displayRangeResult(distance);
                successfulRanges++;
                rangingCount++;
            } else {
                transmitRangeFailed();
                Serial.println(F("Range invalid"));
                failedRanges++;
            }
        } else {
            transmitRangeFailed();
            failedRanges++;
        }

        noteActivity();
    }
}

// ============================================================================
// TRANSMISSION FUNCTIONS
// ============================================================================

void transmitPollAck() {
    DW1000.newTransmit();
    DW1000.setDefaults();

    data[0] = POLL_ACK;

    // Delayed transmission (same as tag)
    DW1000Time deltaTime = DW1000Time(REPLY_DELAY_US, DW1000Time::MICROSECONDS);
    DW1000.setDelay(deltaTime);

    DW1000.setData(data, 16);
    DW1000.startTransmit();
}

void transmitRangeReport(float distance) {
    DW1000.newTransmit();
    DW1000.setDefaults();

    data[0] = RANGE_REPORT;
    memcpy(data + 1, &distance, 4);

    DW1000.setData(data, 16);
    DW1000.startTransmit();
}

void transmitRangeFailed() {
    DW1000.newTransmit();
    DW1000.setDefaults();

    data[0] = RANGE_FAILED;

    DW1000.setData(data, 16);
    DW1000.startTransmit();
}

void receiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}

// ============================================================================
// INTERRUPT HANDLERS
// ============================================================================

void handleSent() {
    sentAck = true;
}

void handleReceived() {
    receivedAck = true;
}

// ============================================================================
// RANGING COMPUTATION
// ============================================================================

void computeRangeAsymmetric() {
    // Asymmetric two-way ranging (more accurate than symmetric)
    DW1000Time round1 = (timePollAckReceived - timePollSent).wrap();
    DW1000Time reply1 = (timePollAckSent - timePollReceived).wrap();
    DW1000Time round2 = (timeRangeReceived - timePollAckSent).wrap();
    DW1000Time reply2 = (timeRangeSent - timePollAckReceived).wrap();

    // Time-of-Flight = (round1 × round2 - reply1 × reply2) / (round1 + round2 + reply1 + reply2)
    DW1000Time tof = (round1 * round2 - reply1 * reply2) / (round1 + round2 + reply1 + reply2);

    timeComputedRange.setTimestamp(tof);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void noteActivity() {
    lastActivity = millis();
}

void resetInactive() {
    expectedMsgId = POLL;
    receiver();
    noteActivity();
}

void displayRangeResult(float distance) {
    Serial.print(F("Range: "));
    Serial.print(distance, 3);
    Serial.print(F(" m ("));
    Serial.print(distance * 100.0, 1);
    Serial.print(F(" cm)"));

    // Display signal quality
    float rxPower = DW1000.getReceivePower();
    Serial.print(F(" | RX: "));
    Serial.print(rxPower, 1);
    Serial.print(F(" dBm"));

    // Display sampling rate
    uint32_t currentTime = millis();
    if (currentTime - rangingPeriodStart > 1000) {
        samplingRate = (1000.0 * rangingCount) / (currentTime - rangingPeriodStart);
        rangingPeriodStart = currentTime;
        rangingCount = 0;
    }

    Serial.print(F(" | Rate: "));
    Serial.print(samplingRate, 1);
    Serial.println(F(" Hz"));
}

// ============================================================================
// STATISTICS
// ============================================================================

void printStatistics() {
    float successRate = 0.0;
    uint32_t total = successfulRanges + failedRanges;

    if (total > 0) {
        successRate = (float)successfulRanges / total * 100.0;
    }

    Serial.print(F("Stats: Success="));
    Serial.print(successfulRanges);
    Serial.print(F(", Failed="));
    Serial.print(failedRanges);
    Serial.print(F(", Rate="));
    Serial.print(successRate, 1);
    Serial.println(F("%"));
}
```

---

## 3. Trilateration Algorithms

### 2D Trilateration (Simpler, for ground-based swarms)

```cpp
Position trilaterate2D(AnchorPosition anchors[], float ranges[], int n) {
    Position result = {0.0, 0.0, 0.0, false};

    if (n < 3) return result;

    // Extract positions (ignore Z coordinate)
    float x1 = anchors[0].x, y1 = anchors[0].y;
    float x2 = anchors[1].x, y2 = anchors[1].y;
    float x3 = anchors[2].x, y3 = anchors[2].y;

    float r1 = ranges[0];
    float r2 = ranges[1];
    float r3 = ranges[2];

    // Solve using circle intersection
    float A = 2 * (x2 - x1);
    float B = 2 * (y2 - y1);
    float C = r1*r1 - r2*r2 - x1*x1 + x2*x2 - y1*y1 + y2*y2;

    float D = 2 * (x3 - x2);
    float E = 2 * (y3 - y2);
    float F = r2*r2 - r3*r3 - x2*x2 + x3*x3 - y2*y2 + y3*y3;

    float denom = (E*A - B*D);

    if (abs(denom) < 0.001) {
        // Anchors are collinear
        return result;
    }

    result.x = (C*E - F*B) / denom;
    result.y = (C*D - A*F) / denom;
    result.z = 0.0;  // Ground level
    result.valid = true;

    return result;
}
```

### Weighted Trilateration (Using 4+ anchors with error estimation)

```cpp
Position trilaterateWeighted(AnchorPosition anchors[], float ranges[], float weights[], int n) {
    Position result = {0.0, 0.0, 0.0, false};

    if (n < 3) return result;

    // Weighted least-squares approach
    float sum_wx = 0.0, sum_wy = 0.0, sum_wz = 0.0;
    float sum_w = 0.0;

    for (int i = 0; i < n; i++) {
        float w = weights[i];  // Weight based on signal quality or variance

        sum_wx += w * anchors[i].x;
        sum_wy += w * anchors[i].y;
        sum_wz += w * anchors[i].z;
        sum_w += w;
    }

    if (sum_w < 0.001) return result;

    // Weighted centroid as initial guess
    float x0 = sum_wx / sum_w;
    float y0 = sum_wy / sum_w;
    float z0 = sum_wz / sum_w;

    // Iterative refinement (Gauss-Newton)
    for (int iter = 0; iter < 5; iter++) {
        float sum_dx = 0.0, sum_dy = 0.0, sum_dz = 0.0;
        float sum_w_iter = 0.0;

        for (int i = 0; i < n; i++) {
            float dx = x0 - anchors[i].x;
            float dy = y0 - anchors[i].y;
            float dz = z0 - anchors[i].z;

            float dist_est = sqrt(dx*dx + dy*dy + dz*dz);
            float error = ranges[i] - dist_est;

            if (dist_est < 0.001) continue;  // Avoid division by zero

            float w = weights[i];

            sum_dx += w * error * dx / dist_est;
            sum_dy += w * error * dy / dist_est;
            sum_dz += w * error * dz / dist_est;
            sum_w_iter += w;
        }

        if (sum_w_iter < 0.001) break;

        // Update position estimate
        x0 += sum_dx / sum_w_iter;
        y0 += sum_dy / sum_w_iter;
        z0 += sum_dz / sum_w_iter;
    }

    result.x = x0;
    result.y = y0;
    result.z = z0;
    result.valid = true;

    return result;
}
```

---

## 4. TDMA Implementation

### Synchronized TDMA with Beacon

```cpp
// Anchor: Broadcasts time reference beacon
#define BEACON_INTERVAL 1000  // ms
#define BEACON_MSG 10

uint32_t lastBeaconTime = 0;

void anchorLoop() {
    uint32_t currentTime = millis();

    // Send beacon periodically
    if (currentTime - lastBeaconTime >= BEACON_INTERVAL) {
        sendBeacon(currentTime);
        lastBeaconTime = currentTime;
    }

    // Handle ranging requests
    handleRangingProtocol();
}

void sendBeacon(uint32_t timestamp) {
    DW1000.newTransmit();
    data[0] = BEACON_MSG;
    memcpy(data + 1, &timestamp, 4);
    DW1000.setData(data, 5);
    DW1000.startTransmit();
}

// Tag: Synchronizes to beacon
int32_t timeOffset = 0;
uint32_t lastSyncTime = 0;

void handleBeacon() {
    if (data[0] == BEACON_MSG) {
        uint32_t anchorTime;
        memcpy(&anchorTime, data + 1, 4);

        timeOffset = anchorTime - millis();
        lastSyncTime = millis();

        Serial.print(F("Time sync: offset="));
        Serial.println(timeOffset);
    }
}

uint32_t getSynchronizedTime() {
    return millis() + timeOffset;
}

uint8_t getMyTimeSlot() {
    uint32_t syncTime = getSynchronizedTime();
    return (syncTime / SLOT_DURATION) % NUM_SLOTS;
}
```

---

## 5. Utility Functions

### Distance Filtering

```cpp
// Exponential Moving Average Filter
class DistanceFilter {
private:
    float alpha;  // Smoothing factor (0-1)
    float filtered;
    bool initialized;

public:
    DistanceFilter(float smoothing = 0.3) : alpha(smoothing), filtered(0.0), initialized(false) {}

    float update(float newValue) {
        if (!initialized) {
            filtered = newValue;
            initialized = true;
        } else {
            filtered = alpha * newValue + (1.0 - alpha) * filtered;
        }
        return filtered;
    }

    float get() { return filtered; }
    void reset() { initialized = false; }
};

// Usage:
DistanceFilter filters[NUM_ANCHORS] = {
    DistanceFilter(0.3),
    DistanceFilter(0.3),
    DistanceFilter(0.3)
};

float filteredDistance = filters[anchorIndex].update(rawDistance);
```

### Outlier Rejection

```cpp
bool isOutlier(float newDistance, float previousDistance, float threshold = 0.5) {
    if (previousDistance == 0.0) return false;  // First measurement

    float diff = abs(newDistance - previousDistance);
    return (diff > threshold);
}

// Usage:
if (!isOutlier(newDistance, distances[anchorIndex], 0.5)) {
    distances[anchorIndex] = newDistance;
} else {
    Serial.println(F("Outlier rejected"));
}
```

### Memory Diagnostics

```cpp
extern int __heap_start, *__brkval;

int freeMemory() {
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void printMemoryUsage() {
    int free = freeMemory();
    Serial.print(F("Free RAM: "));
    Serial.print(free);
    Serial.println(F(" bytes"));

    if (free < 200) {
        Serial.println(F("WARNING: Low memory!"));
    }
}
```

---

## 6. Data Structures

### Complete Network State Structure

```cpp
struct AnchorInfo {
    uint8_t address;
    float x, y, z;           // Position
    float lastDistance;
    bool distanceValid;
    uint32_t lastUpdate;
    float signalQuality;     // RX power
    uint16_t successCount;
    uint16_t failureCount;
};

AnchorInfo anchors[NUM_ANCHORS] = {
    {0x01, 0.0, 0.0, 10.0, 0.0, false, 0, 0.0, 0, 0},
    {0x02, 10.0, 0.0, 10.0, 0.0, false, 0, 0.0, 0, 0},
    {0x03, 5.0, 10.0, 10.0, 0.0, false, 0, 0.0, 0, 0}
};

void updateAnchorInfo(uint8_t index, float distance, float rxPower) {
    anchors[index].lastDistance = distance;
    anchors[index].distanceValid = true;
    anchors[index].lastUpdate = millis();
    anchors[index].signalQuality = rxPower;
    anchors[index].successCount++;
}
```

---

**Document Version**: 1.0
**Last Updated**: 2026-01-08
**Purpose**: Companion code examples for UWB swarm implementation
