/*
 * Simple Ping-Pong Test - SENDER
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
 *   1. Sends "PING" every second
 *   2. Waits for "PONG" response (with timeout)
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
constexpr unsigned long PING_INTERVAL_MS    = 1000;  // Send PING every 1 second
constexpr unsigned long PONG_TIMEOUT_MS     = 500;   // Wait up to 500ms for PONG
constexpr unsigned long STATUS_INTERVAL_MS  = 10000; // Print stats every 10 seconds

// ============================================================================
// MESSAGE DEFINITIONS
// ============================================================================
const char* PING_MSG = "PING";
const char* PONG_MSG = "PONG";

// ============================================================================
// STATE MACHINE
// ============================================================================
enum State {
    STATE_IDLE,
    STATE_SENDING_PING,
    STATE_WAITING_PONG,
    STATE_RECEIVED_PONG
};

volatile State currentState = STATE_IDLE;

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
unsigned long pingsSent = 0;                   // Total PINGs transmitted
unsigned long pongsReceived = 0;               // Total PONGs received
unsigned long timeouts = 0;                    // PONG timeout count
unsigned long rxErrors = 0;                    // Receive error count
unsigned long txErrors = 0;                    // Transmit error count
unsigned long lastPingTime = 0;                // Time of last PING sent
unsigned long lastStatusTime = 0;              // Time of last status print
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
    float successRate = (pingsSent > 0) ? (100.0 * pongsReceived / pingsSent) : 0.0;

    printSeparator();
    Serial.println(F("STATISTICS"));
    printSeparator();

    Serial.print(F("Uptime:           ")); Serial.print(uptime); Serial.println(F(" seconds"));
    Serial.print(F("IRQ Count:        ")); Serial.println(irqCount);
    Serial.print(F("PINGs Sent:       ")); Serial.println(pingsSent);
    Serial.print(F("PONGs Received:   ")); Serial.println(pongsReceived);
    Serial.print(F("Timeouts:         ")); Serial.println(timeouts);
    Serial.print(F("RX Errors:        ")); Serial.println(rxErrors);
    Serial.print(F("TX Errors:        ")); Serial.println(txErrors);
    Serial.print(F("Success Rate:     ")); Serial.print(successRate, 1); Serial.println(F("%"));

    printSeparator();
}

void startReceiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(false);  // Single receive mode
    DW1000.startReceive();
    Serial.println(F("[DEBUG] Receiver started, waiting for PONG..."));
}

void sendPing() {
    Serial.println();
    Serial.print(F("[TX] Sending PING #")); Serial.println(pingsSent + 1);

    DW1000.newTransmit();
    DW1000.setDefaults();
    DW1000.setData(PING_MSG);
    DW1000.startTransmit();

    currentState = STATE_SENDING_PING;
    lastPingTime = millis();
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

    // Check if it's a PONG response
    if (strncmp((char*)data, PONG_MSG, 4) == 0) {
        pongsReceived++;
        unsigned long rtt = millis() - lastPingTime;
        Serial.print(F("[OK] PONG received! Round-trip time: "));
        Serial.print(rtt); Serial.println(F(" ms"));
        currentState = STATE_RECEIVED_PONG;
    } else {
        Serial.print(F("[WARN] Unexpected message: ")); Serial.println((char*)data);
    }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);  // Wait for serial to stabilize

    printSeparator();
    Serial.println(F("DW1000 PING-PONG TEST - SENDER"));
    Serial.println(F("Sends PING, waits for PONG response"));
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
    DW1000.setDeviceAddress(1);     // Sender = address 1
    DW1000.setNetworkId(0xDECA);    // Network ID
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setChannel(5);           // Use channel 5
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
    Serial.println(F("\n[INFO] Starting ping-pong test in 2 seconds..."));
    Serial.println(F("[INFO] Make sure receiver is running!\n"));

    delay(2000);  // Give receiver time to start

    startTime = millis();
    lastStatusTime = millis();
    currentState = STATE_IDLE;
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    unsigned long now = millis();

    // ========================================================================
    // Handle transmit complete
    // ========================================================================
    if (sentFlag) {
        sentFlag = false;
        if (currentState == STATE_SENDING_PING) {
            pingsSent++;
            Serial.print(F("[DEBUG] PING #")); Serial.print(pingsSent);
            Serial.print(F(" transmitted (IRQ count: ")); Serial.print(irqCount);
            Serial.println(F(")"));

            // Start listening for PONG
            startReceiver();
            currentState = STATE_WAITING_PONG;
        }
    }

    // ========================================================================
    // Handle receive complete
    // ========================================================================
    if (receivedFlag) {
        receivedFlag = false;
        Serial.print(F("[DEBUG] Receive interrupt (IRQ count: "));
        Serial.print(irqCount); Serial.println(F(")"));

        if (currentState == STATE_WAITING_PONG) {
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
        currentState = STATE_IDLE;
    }

    if (receiveTimeoutFlag) {
        receiveTimeoutFlag = false;
        Serial.println(F("[WARN] Receive timeout from DW1000"));
    }

    // ========================================================================
    // Check for PONG timeout (software timeout)
    // ========================================================================
    if (currentState == STATE_WAITING_PONG) {
        if (now - lastPingTime > PONG_TIMEOUT_MS) {
            timeouts++;
            Serial.print(F("[TIMEOUT] No PONG received within "));
            Serial.print(PONG_TIMEOUT_MS);
            Serial.print(F("ms (Total timeouts: "));
            Serial.print(timeouts); Serial.println(F(")"));

            // Stop receiver and go idle
            DW1000.idle();
            currentState = STATE_IDLE;
        }
    }

    // ========================================================================
    // Send next PING (if idle and interval elapsed)
    // ========================================================================
    if (currentState == STATE_IDLE || currentState == STATE_RECEIVED_PONG) {
        if (now - lastPingTime >= PING_INTERVAL_MS) {
            sendPing();
        }
    }

    // ========================================================================
    // Print periodic statistics
    // ========================================================================
    if (now - lastStatusTime >= STATUS_INTERVAL_MS) {
        lastStatusTime = now;
        printStatistics();
    }
}
