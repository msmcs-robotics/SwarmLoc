/*
 * Simple Ping-Pong Test - RECEIVER
 *
 * Purpose: Test basic DW1000 TX/RX without ranging complexity
 *
 * Hardware: Arduino Uno + DWS1000 Shield
 * Pin Configuration:
 *   - RST = 7 (Reset)
 *   - IRQ = 2 (Interrupt - requires jumper D8->D2)
 *   - SS  = 10 (SPI Chip Select)
 *
 * Behavior:
 *   1. Continuously listens for incoming messages
 *   2. When "PING" is received, responds with "PONG"
 *   3. Prints verbose debug output including IRQ counts and status
 *   4. Tracks success/failure statistics
 *
 * Test Date: 2026-01-12
 */

#include <SPI.h>
#include <DW1000.h>

// ============================================================================
// PIN CONFIGURATION - DWS1000 Shield
// ============================================================================
constexpr uint8_t PIN_RST = 7;   // Reset pin
constexpr uint8_t PIN_IRQ = 2;   // IRQ pin (needs jumper D8->D2)
constexpr uint8_t PIN_SS  = 10;  // SPI Slave Select

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================
constexpr unsigned long STATUS_INTERVAL_MS  = 10000; // Print stats every 10 seconds
constexpr unsigned long HEARTBEAT_INTERVAL_MS = 5000; // Print heartbeat every 5 seconds

// ============================================================================
// MESSAGE DEFINITIONS
// ============================================================================
const char* PING_MSG = "PING";
const char* PONG_MSG = "PONG";

// ============================================================================
// STATE MACHINE
// ============================================================================
enum State {
    STATE_LISTENING,
    STATE_SENDING_PONG,
    STATE_PONG_SENT
};

volatile State currentState = STATE_LISTENING;

// ============================================================================
// INTERRUPT FLAGS (set by ISR handlers)
// ============================================================================
volatile bool sentFlag = false;
volatile bool receivedFlag = false;
volatile bool receiveFailedFlag = false;
volatile bool receiveTimeoutFlag = false;

// ============================================================================
// STATISTICS
// ============================================================================
volatile unsigned long irqCount = 0;           // Total interrupts received
unsigned long pingsReceived = 0;               // Total PINGs received
unsigned long pongsSent = 0;                   // Total PONGs transmitted
unsigned long rxErrors = 0;                    // Receive error count
unsigned long txErrors = 0;                    // Transmit error count
unsigned long unknownMessages = 0;             // Unknown message count
unsigned long lastStatusTime = 0;              // Time of last status print
unsigned long lastHeartbeatTime = 0;           // Time of last heartbeat
unsigned long startTime = 0;                   // Program start time

// ============================================================================
// INTERRUPT HANDLERS
// ============================================================================
void handleSent() {
    irqCount++;
    sentFlag = true;
}

void handleReceived() {
    irqCount++;
    receivedFlag = true;
}

void handleReceiveFailed() {
    irqCount++;
    receiveFailedFlag = true;
}

void handleReceiveTimeout() {
    irqCount++;
    receiveTimeoutFlag = true;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
void printSeparator() {
    Serial.println(F("========================================"));
}

void printDeviceInfo() {
    char msgBuffer[128];

    printSeparator();
    Serial.println(F("DW1000 DEVICE INFORMATION"));
    printSeparator();

    DW1000.getPrintableDeviceIdentifier(msgBuffer);
    Serial.print(F("Device ID:      ")); Serial.println(msgBuffer);

    DW1000.getPrintableExtendedUniqueIdentifier(msgBuffer);
    Serial.print(F("Unique ID:      ")); Serial.println(msgBuffer);

    DW1000.getPrintableNetworkIdAndShortAddress(msgBuffer);
    Serial.print(F("Network/Addr:   ")); Serial.println(msgBuffer);

    DW1000.getPrintableDeviceMode(msgBuffer);
    Serial.print(F("Device Mode:    ")); Serial.println(msgBuffer);

    printSeparator();
}

void printStatistics() {
    unsigned long uptime = (millis() - startTime) / 1000;
    float successRate = (pingsReceived > 0) ? (100.0 * pongsSent / pingsReceived) : 0.0;

    printSeparator();
    Serial.println(F("STATISTICS"));
    printSeparator();

    Serial.print(F("Uptime:           ")); Serial.print(uptime); Serial.println(F(" seconds"));
    Serial.print(F("IRQ Count:        ")); Serial.println(irqCount);
    Serial.print(F("PINGs Received:   ")); Serial.println(pingsReceived);
    Serial.print(F("PONGs Sent:       ")); Serial.println(pongsSent);
    Serial.print(F("RX Errors:        ")); Serial.println(rxErrors);
    Serial.print(F("TX Errors:        ")); Serial.println(txErrors);
    Serial.print(F("Unknown Msgs:     ")); Serial.println(unknownMessages);
    Serial.print(F("Response Rate:    ")); Serial.print(successRate, 1); Serial.println(F("%"));

    printSeparator();
}

void startReceiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);  // Continuous receive mode
    DW1000.startReceive();
    Serial.println(F("[DEBUG] Receiver started, listening for PING..."));
}

void sendPong() {
    Serial.println();
    Serial.print(F("[TX] Sending PONG #")); Serial.println(pongsSent + 1);

    // Stop receiver first
    DW1000.idle();

    DW1000.newTransmit();
    DW1000.setDefaults();
    DW1000.setData(PONG_MSG);
    DW1000.startTransmit();

    currentState = STATE_SENDING_PONG;
}

void processReceivedData() {
    // Get received data
    uint16_t dataLen = DW1000.getDataLength();
    byte data[64];
    DW1000.getData(data, dataLen);

    // Null-terminate for string comparison
    if (dataLen < 64) {
        data[dataLen] = 0;
    }

    // Get signal quality info
    float rxPower = DW1000.getReceivePower();
    float fpPower = DW1000.getFirstPathPower();
    float rxQuality = DW1000.getReceiveQuality();

    Serial.print(F("[RX] Received ")); Serial.print(dataLen); Serial.println(F(" bytes"));
    Serial.print(F("[RX] Data: \"")); Serial.print((char*)data); Serial.println(F("\""));
    Serial.print(F("[RX] RX Power: ")); Serial.print(rxPower, 1); Serial.println(F(" dBm"));
    Serial.print(F("[RX] FP Power: ")); Serial.print(fpPower, 1); Serial.println(F(" dBm"));
    Serial.print(F("[RX] Quality:  ")); Serial.println(rxQuality, 2);

    // Check if it's a PING message
    if (strncmp((char*)data, PING_MSG, 4) == 0) {
        pingsReceived++;
        Serial.print(F("[OK] PING #")); Serial.print(pingsReceived);
        Serial.println(F(" received! Sending PONG..."));
        sendPong();
    } else {
        unknownMessages++;
        Serial.print(F("[WARN] Unknown message: \""));
        Serial.print((char*)data);
        Serial.print(F("\" (Total unknown: "));
        Serial.print(unknownMessages); Serial.println(F(")"));
    }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);  // Wait for serial to stabilize

    printSeparator();
    Serial.println(F("DW1000 PING-PONG TEST - RECEIVER"));
    Serial.println(F("Listens for PING, responds with PONG"));
    printSeparator();

    Serial.println(F("\n[INIT] Pin Configuration:"));
    Serial.print(F("  RST = ")); Serial.println(PIN_RST);
    Serial.print(F("  IRQ = ")); Serial.println(PIN_IRQ);
    Serial.print(F("  SS  = ")); Serial.println(PIN_SS);

    // Initialize DW1000
    Serial.println(F("\n[INIT] Initializing DW1000..."));
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);
    Serial.println(F("[INIT] DW1000 initialized"));

    // Configure DW1000
    Serial.println(F("[INIT] Configuring DW1000..."));
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);     // Receiver = address 2
    DW1000.setNetworkId(0xDECA);    // Same network ID as sender
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setChannel(5);           // Use channel 5 (same as sender)
    DW1000.commitConfiguration();
    Serial.println(F("[INIT] Configuration committed"));

    // Print device information
    printDeviceInfo();

    // Attach interrupt handlers
    Serial.println(F("[INIT] Attaching interrupt handlers..."));
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);
    DW1000.attachReceiveFailedHandler(handleReceiveFailed);
    DW1000.attachReceiveTimeoutHandler(handleReceiveTimeout);

    // Enable interrupts
    DW1000.interruptOnSent(true);
    DW1000.interruptOnReceived(true);
    DW1000.interruptOnReceiveFailed(true);
    DW1000.interruptOnReceiveTimeout(true);

    Serial.println(F("[INIT] Setup complete!"));
    Serial.println(F("\n[INFO] Now listening for PING messages..."));
    Serial.println(F("[INFO] Start the sender when ready!\n"));

    startTime = millis();
    lastStatusTime = millis();
    lastHeartbeatTime = millis();

    // Start listening
    startReceiver();
    currentState = STATE_LISTENING;
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    unsigned long now = millis();

    // ========================================================================
    // Handle transmit complete (PONG sent)
    // ========================================================================
    if (sentFlag) {
        sentFlag = false;
        if (currentState == STATE_SENDING_PONG) {
            pongsSent++;
            Serial.print(F("[DEBUG] PONG #")); Serial.print(pongsSent);
            Serial.print(F(" transmitted (IRQ count: ")); Serial.print(irqCount);
            Serial.println(F(")"));

            // Resume listening
            startReceiver();
            currentState = STATE_LISTENING;
        }
    }

    // ========================================================================
    // Handle receive complete
    // ========================================================================
    if (receivedFlag) {
        receivedFlag = false;
        Serial.print(F("[DEBUG] Receive interrupt (IRQ count: "));
        Serial.print(irqCount); Serial.println(F(")"));

        if (currentState == STATE_LISTENING) {
            processReceivedData();
        }
    }

    // ========================================================================
    // Handle receive errors
    // ========================================================================
    if (receiveFailedFlag) {
        receiveFailedFlag = false;
        rxErrors++;
        Serial.print(F("[ERROR] Receive failed! (Total errors: "));
        Serial.print(rxErrors); Serial.println(F(")"));

        // Restart receiver
        startReceiver();
        currentState = STATE_LISTENING;
    }

    if (receiveTimeoutFlag) {
        receiveTimeoutFlag = false;
        Serial.println(F("[WARN] Receive timeout from DW1000 (this is normal)"));
    }

    // ========================================================================
    // Print periodic heartbeat (shows we're alive even without traffic)
    // ========================================================================
    if (now - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatTime = now;
        unsigned long uptime = (now - startTime) / 1000;
        Serial.print(F("[HEARTBEAT] Uptime: ")); Serial.print(uptime);
        Serial.print(F("s, IRQs: ")); Serial.print(irqCount);
        Serial.print(F(", PINGs: ")); Serial.print(pingsReceived);
        Serial.print(F(", PONGs: ")); Serial.println(pongsSent);
    }

    // ========================================================================
    // Print periodic statistics
    // ========================================================================
    if (now - lastStatusTime >= STATUS_INTERVAL_MS) {
        lastStatusTime = now;
        printStatistics();
    }
}
