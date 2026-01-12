/*
 * DW1000 Calibration Test Firmware
 *
 * Purpose: Simplified ranging code for antenna delay calibration
 * Output Format: CSV (timestamp_ms,distance_m)
 *
 * Features:
 * - Clean CSV output for automated parsing
 * - Configurable antenna delay via serial command
 * - Minimal debug output
 * - High measurement rate
 *
 * Usage:
 * 1. Upload to TAG device (set IS_TAG = true)
 * 2. Upload to ANCHOR device (set IS_TAG = false)
 * 3. Open serial monitor (115200 baud)
 * 4. Data format: timestamp_ms,distance_m
 *
 * Serial Commands:
 * - "D16450" - Set antenna delay to 16450
 * - "S" - Show current settings
 * - "R" - Reset statistics
 */

#include <SPI.h>
#include <DW1000.h>

// ========================================
// CONFIGURATION
// ========================================

// Device role
const bool IS_TAG = true;  // Set to false for ANCHOR

// Pin definitions
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Calibration settings
uint16_t ANTENNA_DELAY = 16450;  // Adjustable via serial command

// Network configuration
const uint16_t NETWORK_ID = 10;
const uint16_t TAG_ADDRESS = 2;
const uint16_t ANCHOR_ADDRESS = 1;

// Protocol messages
#define MSG_POLL 0
#define MSG_POLL_ACK 1
#define MSG_RANGE 2
#define MSG_RANGE_REPORT 3
#define MSG_RANGE_FAILED 255

// Timing
const uint16_t REPLY_DELAY_US = 3000;
const uint32_t RESET_PERIOD_MS = 250;

// Output control
const bool OUTPUT_CSV = true;       // Enable CSV output
const bool OUTPUT_DEBUG = false;    // Enable debug messages

// ========================================
// GLOBAL VARIABLES
// ========================================

// State management
volatile byte expectedMsgId;
volatile boolean sentAck = false;
volatile boolean receivedAck = false;
boolean protocolFailed = false;

// Timestamps
DW1000Time timePollSent;
DW1000Time timePollReceived;
DW1000Time timePollAckSent;
DW1000Time timePollAckReceived;
DW1000Time timeRangeSent;
DW1000Time timeRangeReceived;
DW1000Time timeComputedRange;

// Data buffer
#define LEN_DATA 16
byte data[LEN_DATA];

// Activity tracking
uint32_t lastActivity = 0;

// Statistics
uint32_t totalRanges = 0;
uint32_t failedRanges = 0;
uint32_t startTime = 0;

// ========================================
// SETUP
// ========================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Print header
    if (OUTPUT_DEBUG) {
        Serial.println(F("# DW1000 Calibration Test"));
        Serial.print(F("# Role: "));
        Serial.println(IS_TAG ? F("TAG") : F("ANCHOR"));
        Serial.print(F("# Antenna Delay: "));
        Serial.println(ANTENNA_DELAY);
        Serial.println(F("#"));
    }

    // Initialize DW1000
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    if (OUTPUT_DEBUG) {
        Serial.println(F("# DW1000 initialized"));
    }

    // Configure DW1000
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(IS_TAG ? TAG_ADDRESS : ANCHOR_ADDRESS);
    DW1000.setNetworkId(NETWORK_ID);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setAntennaDelay(ANTENNA_DELAY);
    DW1000.commitConfiguration();

    if (OUTPUT_DEBUG) {
        Serial.println(F("# Configuration committed"));
    }

    // Attach handlers
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);

    // Start protocol
    receiver();

    if (IS_TAG) {
        expectedMsgId = MSG_POLL_ACK;
        transmitPoll();
    } else {
        expectedMsgId = MSG_POLL;
    }

    noteActivity();
    startTime = millis();

    // Print CSV header
    if (OUTPUT_CSV) {
        Serial.println(F("# timestamp_ms,distance_m"));
    }

    if (OUTPUT_DEBUG) {
        Serial.println(F("# Ready"));
    }
}

// ========================================
// MAIN LOOP
// ========================================

void loop() {
    // Check for serial commands
    checkSerialCommands();

    // Handle ranging protocol
    if (IS_TAG) {
        loopTag();
    } else {
        loopAnchor();
    }
}

// ========================================
// TAG LOOP
// ========================================

void loopTag() {
    // Check for timeout
    if (!sentAck && !receivedAck) {
        if (millis() - lastActivity > RESET_PERIOD_MS) {
            resetInactive();
        }
        return;
    }

    // Handle sent acknowledgment
    if (sentAck) {
        sentAck = false;
        byte msgId = data[0];

        if (msgId == MSG_POLL) {
            DW1000.getTransmitTimestamp(timePollSent);
        } else if (msgId == MSG_RANGE) {
            DW1000.getTransmitTimestamp(timeRangeSent);
        }
    }

    // Handle received message
    if (receivedAck) {
        receivedAck = false;
        DW1000.getData(data, LEN_DATA);
        byte msgId = data[0];

        if (msgId != expectedMsgId) {
            // Unexpected message, reset
            expectedMsgId = MSG_POLL_ACK;
            transmitPoll();
            return;
        }

        if (msgId == MSG_POLL_ACK) {
            DW1000.getReceiveTimestamp(timePollAckReceived);
            expectedMsgId = MSG_RANGE_REPORT;
            transmitRange();
            noteActivity();
        } else if (msgId == MSG_RANGE_REPORT) {
            expectedMsgId = MSG_POLL_ACK;

            // Extract range
            float curRange;
            memcpy(&curRange, data + 1, 4);

            // Output measurement
            outputMeasurement(curRange);

            // Continue ranging
            transmitPoll();
            noteActivity();
        } else if (msgId == MSG_RANGE_FAILED) {
            expectedMsgId = MSG_POLL_ACK;
            failedRanges++;
            transmitPoll();
            noteActivity();
        }
    }
}

// ========================================
// ANCHOR LOOP
// ========================================

void loopAnchor() {
    // Check for timeout
    if (!sentAck && !receivedAck) {
        if (millis() - lastActivity > RESET_PERIOD_MS) {
            resetInactive();
        }
        return;
    }

    // Handle sent acknowledgment
    if (sentAck) {
        sentAck = false;
        byte msgId = data[0];

        if (msgId == MSG_POLL_ACK) {
            DW1000.getTransmitTimestamp(timePollAckSent);
            noteActivity();
        }
    }

    // Handle received message
    if (receivedAck) {
        receivedAck = false;
        DW1000.getData(data, LEN_DATA);
        byte msgId = data[0];

        if (msgId != expectedMsgId) {
            protocolFailed = true;
        }

        if (msgId == MSG_POLL) {
            protocolFailed = false;
            DW1000.getReceiveTimestamp(timePollReceived);
            expectedMsgId = MSG_RANGE;
            transmitPollAck();
            noteActivity();
        } else if (msgId == MSG_RANGE) {
            DW1000.getReceiveTimestamp(timeRangeReceived);
            expectedMsgId = MSG_POLL;

            if (!protocolFailed) {
                // Extract timestamps from TAG
                timePollSent.setTimestamp(data + 1);
                timePollAckReceived.setTimestamp(data + 6);
                timeRangeSent.setTimestamp(data + 11);

                // Compute range
                computeRangeAsymmetric();

                // Send range report
                transmitRangeReport(timeComputedRange.getAsMicroSeconds());
            } else {
                transmitRangeFailed();
            }

            noteActivity();
        }
    }
}

// ========================================
// RANGING COMPUTATION
// ========================================

void computeRangeAsymmetric() {
    // Asymmetric DS-TWR formula
    DW1000Time round1 = (timePollAckReceived - timePollSent).wrap();
    DW1000Time reply1 = (timePollAckSent - timePollReceived).wrap();
    DW1000Time round2 = (timeRangeReceived - timePollAckSent).wrap();
    DW1000Time reply2 = (timeRangeSent - timePollAckReceived).wrap();

    DW1000Time tof = (round1 * round2 - reply1 * reply2) /
                     (round1 + round2 + reply1 + reply2);

    timeComputedRange.setTimestamp(tof);
}

// ========================================
// OUTPUT
// ========================================

void outputMeasurement(float rangeUs) {
    totalRanges++;

    // Convert time-of-flight to distance
    // Speed of light: 299,702,547 m/s
    float distance_m = rangeUs / 1000000.0 * 299702547.0;

    if (OUTPUT_CSV) {
        // CSV format: timestamp_ms,distance_m
        Serial.print(millis());
        Serial.print(',');
        Serial.println(distance_m, 6);
    }
}

// ========================================
// PROTOCOL FUNCTIONS
// ========================================

void transmitPoll() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = MSG_POLL;
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void transmitPollAck() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = MSG_POLL_ACK;
    DW1000Time deltaTime = DW1000Time(REPLY_DELAY_US, DW1000Time::MICROSECONDS);
    DW1000.setDelay(deltaTime);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void transmitRange() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = MSG_RANGE;
    DW1000Time deltaTime = DW1000Time(REPLY_DELAY_US, DW1000Time::MICROSECONDS);
    timeRangeSent = DW1000.setDelay(deltaTime);
    timePollSent.getTimestamp(data + 1);
    timePollAckReceived.getTimestamp(data + 6);
    timeRangeSent.getTimestamp(data + 11);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void transmitRangeReport(float curRange) {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = MSG_RANGE_REPORT;
    memcpy(data + 1, &curRange, 4);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void transmitRangeFailed() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = MSG_RANGE_FAILED;
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void receiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}

void noteActivity() {
    lastActivity = millis();
}

void resetInactive() {
    if (IS_TAG) {
        expectedMsgId = MSG_POLL_ACK;
        transmitPoll();
    } else {
        expectedMsgId = MSG_POLL;
        receiver();
    }
    noteActivity();
}

// ========================================
// INTERRUPT HANDLERS
// ========================================

void handleSent() {
    sentAck = true;
}

void handleReceived() {
    receivedAck = true;
}

// ========================================
// SERIAL COMMANDS
// ========================================

void checkSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command.length() == 0) {
            return;
        }

        // Command: Dxxxx - Set antenna delay
        if (command.charAt(0) == 'D' || command.charAt(0) == 'd') {
            uint16_t newDelay = command.substring(1).toInt();

            if (newDelay >= 16000 && newDelay <= 17000) {
                ANTENNA_DELAY = newDelay;
                DW1000.setAntennaDelay(ANTENNA_DELAY);

                Serial.print(F("# Antenna delay set to: "));
                Serial.println(ANTENNA_DELAY);
            } else {
                Serial.println(F("# ERROR: Invalid delay (use 16000-17000)"));
            }
        }
        // Command: S - Show settings
        else if (command.charAt(0) == 'S' || command.charAt(0) == 's') {
            Serial.println(F("#"));
            Serial.println(F("# Current Settings:"));
            Serial.print(F("#   Role: "));
            Serial.println(IS_TAG ? F("TAG") : F("ANCHOR"));
            Serial.print(F("#   Antenna Delay: "));
            Serial.println(ANTENNA_DELAY);
            Serial.print(F("#   Total Ranges: "));
            Serial.println(totalRanges);
            Serial.print(F("#   Failed Ranges: "));
            Serial.println(failedRanges);
            Serial.print(F("#   Success Rate: "));
            if (totalRanges > 0) {
                Serial.print((float)(totalRanges - failedRanges) / totalRanges * 100.0, 1);
                Serial.println(F("%"));
            } else {
                Serial.println(F("N/A"));
            }
            Serial.print(F("#   Uptime: "));
            Serial.print((millis() - startTime) / 1000);
            Serial.println(F(" s"));
            Serial.println(F("#"));
        }
        // Command: R - Reset statistics
        else if (command.charAt(0) == 'R' || command.charAt(0) == 'r') {
            totalRanges = 0;
            failedRanges = 0;
            startTime = millis();
            Serial.println(F("# Statistics reset"));
        }
        // Unknown command
        else {
            Serial.println(F("# Unknown command"));
            Serial.println(F("# Commands:"));
            Serial.println(F("#   Dxxxx - Set antenna delay"));
            Serial.println(F("#   S     - Show settings"));
            Serial.println(F("#   R     - Reset statistics"));
        }
    }
}
