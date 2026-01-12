/**
 * DWS1000 Interrupt-Based Test with Library Defaults
 *
 * Testing with the ORIGINAL library pin configuration that worked
 * in the archive code. Remove jumper wire D8->D2 before testing.
 *
 * Pin Configuration (Library Defaults - proven to work):
 * RST = D9, IRQ = D2, SS = D10, SPI = D11/12/13
 *
 * This test combines:
 * 1. Original working pin config from archive
 * 2. Proper interrupt handling from library
 * 3. Data reads inside ISR to prevent corruption
 */

#include <SPI.h>
#include "DW1000.h"

// Forward declarations
void clearStatus();
uint32_t readStatus();
void printStatus(uint32_t status);
void startReceiver();
void transmitMessage(const char* msg);
bool waitForTxComplete(unsigned long timeout);
bool waitForRxComplete(unsigned long timeout);
void handleSent();
void handleReceived();
void handleReceiveError();

// ORIGINAL LIBRARY DEFAULTS - from archive code that worked!
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;  // D10

// SYS_STATUS bit masks (from DW1000 User Manual)
#define SYS_STATUS_TXFRS  0x00000080UL  // TX frame sent
#define SYS_STATUS_RXDFR  0x00002000UL  // RX data frame ready
#define SYS_STATUS_RXFCG  0x00004000UL  // RX FCS good
#define SYS_STATUS_RXFCE  0x00008000UL  // RX FCS error
#define SYS_STATUS_RXOVRR 0x00100000UL  // RX overrun
#define SYS_STATUS_RXPTO  0x00200000UL  // Preamble timeout
#define SYS_STATUS_RXSFDTO 0x04000000UL // SFD timeout

// Mode: 0 = receiver, 1 = transmitter
#define IS_TRANSMITTER false

// Message buffer
#define MSG_LEN 16
byte txMsg[MSG_LEN];
byte rxMsg[MSG_LEN];

unsigned long txCount = 0;
unsigned long rxCount = 0;
unsigned long errCount = 0;
unsigned long lastTx = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("=============================================");
    Serial.println("DWS1000 INTERRUPT MODE - LIBRARY DEFAULTS");
    Serial.println(IS_TRANSMITTER ? "Mode: TRANSMITTER" : "Mode: RECEIVER");
    Serial.println("PIN_RST=9, PIN_IRQ=2, PIN_SS=10");
    Serial.println("IMPORTANT: REMOVE jumper wire D8->D2!");
    Serial.println("=============================================");
    Serial.println();

    // Initialize WITH IRQ using library's proper initialization
    Serial.println("[INIT] Starting DW1000 with interrupts...");

    // Use library's begin() which handles RST properly
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Read device ID to verify SPI
    char deviceId[32];
    DW1000.getPrintableDeviceIdentifier(deviceId);
    Serial.print("[INIT] Device ID: ");
    Serial.println(deviceId);

    if (strstr(deviceId, "DECA") == NULL) {
        Serial.println("[FAIL] DW1000 not detected!");
        while(1) delay(1000);
    }
    Serial.println("[PASS] SPI working - DW1000 detected!");

    // Configure DW1000 - match configuration from archive code
    Serial.println("[INIT] Configuring...");
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(IS_TRANSMITTER ? 1 : 2);
    DW1000.setNetworkId(10);  // Same as archive code
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    // Print configuration
    char msg[128];
    DW1000.getPrintableExtendedUniqueIdentifier(msg);
    Serial.print("[INIT] EUI: ");
    Serial.println(msg);
    DW1000.getPrintableNetworkIdAndShortAddress(msg);
    Serial.print("[INIT] Net/Addr: ");
    Serial.println(msg);
    DW1000.getPrintableDeviceMode(msg);
    Serial.print("[INIT] Mode: ");
    Serial.println(msg);

    // Attach interrupt handlers - CRITICAL for proper operation
    Serial.println("[INIT] Attaching interrupt handlers...");
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);
    DW1000.attachReceiveFailedHandler(handleReceiveError);

    // Clear status register
    clearStatus();

    if (!IS_TRANSMITTER) {
        // Start receiver
        Serial.println("[INIT] Starting receiver...");
        startReceiver();
    }

    Serial.println();
    Serial.println("=============================================");
    Serial.println("RUNNING - Using interrupt-based operation");
    Serial.println("=============================================");
    Serial.println();
}

void clearStatus() {
    // Clear all status bits by writing 1s
    byte clear[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(0x0F, 0x00, clear, 5);
}

uint32_t readStatus() {
    byte status[5];
    DW1000.readBytes(0x0F, 0x00, status, 5);
    // Return lower 32 bits
    return ((uint32_t)status[3] << 24) | ((uint32_t)status[2] << 16) |
           ((uint32_t)status[1] << 8) | status[0];
}

void printStatus(uint32_t status) {
    Serial.print("  SYS_STATUS: 0x");
    Serial.println(status, HEX);
    if (status & SYS_STATUS_TXFRS) Serial.println("  - TXFRS: TX complete");
    if (status & SYS_STATUS_RXDFR) Serial.println("  - RXDFR: RX data ready");
    if (status & SYS_STATUS_RXFCG) Serial.println("  - RXFCG: Good CRC");
    if (status & SYS_STATUS_RXFCE) Serial.println("  - RXFCE: CRC error!");
    if (status & SYS_STATUS_RXOVRR) Serial.println("  - RXOVRR: RX overrun!");
    if (status & SYS_STATUS_RXPTO) Serial.println("  - RXPTO: Preamble timeout");
}

void startReceiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(false);  // Single shot
    DW1000.startReceive();
}

void transmitMessage(const char* msg) {
    DW1000.newTransmit();
    DW1000.setDefaults();

    // Copy message
    memset(txMsg, 0, MSG_LEN);
    strncpy((char*)txMsg, msg, MSG_LEN - 1);

    DW1000.setData(txMsg, MSG_LEN);
    DW1000.startTransmit();
}

// Wait for TX complete by polling
bool waitForTxComplete(unsigned long timeout) {
    unsigned long start = millis();
    while (millis() - start < timeout) {
        uint32_t status = readStatus();
        if (status & SYS_STATUS_TXFRS) {
            clearStatus();
            return true;
        }
    }
    return false;
}

// Wait for RX complete by polling
bool waitForRxComplete(unsigned long timeout) {
    unsigned long start = millis();
    while (millis() - start < timeout) {
        uint32_t status = readStatus();

        if (status & SYS_STATUS_RXFCG) {
            // Good frame received
            clearStatus();
            return true;
        }

        if (status & (SYS_STATUS_RXFCE | SYS_STATUS_RXOVRR | SYS_STATUS_RXPTO | SYS_STATUS_RXSFDTO)) {
            // Error - restart receiver
            clearStatus();
            startReceiver();
        }
    }
    return false;
}

// Interrupt flags - set by ISRs
volatile boolean sentAck = false;
volatile boolean receivedAck = false;
volatile boolean rxError = false;

// Buffer for data read in ISR (prevents corruption)
volatile byte isrRxBuffer[MSG_LEN];
volatile uint16_t isrRxLen = 0;

// Interrupt handlers - read data immediately in ISR to prevent corruption
void handleSent() {
    sentAck = true;
}

void handleReceived() {
    // CRITICAL: Read data immediately in ISR before buffer changes!
    isrRxLen = DW1000.getDataLength();
    if (isrRxLen > MSG_LEN) isrRxLen = MSG_LEN;
    DW1000.getData((byte*)isrRxBuffer, isrRxLen);
    receivedAck = true;
}

void handleReceiveError() {
    rxError = true;
}

unsigned long lastPrint = 0;

void loop() {
    // Handle RX errors
    if (rxError) {
        rxError = false;
        errCount++;
        Serial.println("[ERR] Receive failed");
        startReceiver();
    }

    if (IS_TRANSMITTER) {
        // === TRANSMITTER MODE (using interrupts) ===
        if (millis() - lastTx > 1000) {
            lastTx = millis();
            txCount++;

            Serial.print("[TX #");
            Serial.print(txCount);
            Serial.print("] Sending PING...");

            sentAck = false;
            transmitMessage("PING");

            // Wait for TX complete interrupt
            unsigned long start = millis();
            while (!sentAck && millis() - start < 100) {
                // Wait for interrupt
            }

            if (sentAck) {
                Serial.println(" SENT!");

                // Start receiver and wait for PONG
                receivedAck = false;
                startReceiver();
                Serial.print("[RX] Waiting for PONG...");

                start = millis();
                while (!receivedAck && !rxError && millis() - start < 500) {
                    // Wait for interrupt
                }

                if (receivedAck) {
                    // Copy from ISR buffer
                    memcpy(rxMsg, (const void*)isrRxBuffer, MSG_LEN);

                    Serial.print(" GOT: ");
                    // Print raw bytes
                    for (int i = 0; i < min((int)isrRxLen, 8); i++) {
                        if (rxMsg[i] < 16) Serial.print("0");
                        Serial.print(rxMsg[i], HEX);
                        Serial.print(" ");
                    }
                    Serial.print(" -> ");
                    Serial.println((char*)rxMsg);

                    if (strncmp((char*)rxMsg, "PONG", 4) == 0) {
                        rxCount++;
                        Serial.println("[SUCCESS] PONG received correctly!");
                    }
                } else {
                    Serial.println(" TIMEOUT");
                    errCount++;
                }
            } else {
                Serial.println(" TX FAILED!");
                errCount++;
            }
            Serial.println();
        }
    } else {
        // === RECEIVER MODE (using interrupts) ===
        if (receivedAck) {
            receivedAck = false;
            rxCount++;

            // Copy from ISR buffer
            memcpy(rxMsg, (const void*)isrRxBuffer, MSG_LEN);

            Serial.print("[RX #");
            Serial.print(rxCount);
            Serial.print("] Len=");
            Serial.print(isrRxLen);
            Serial.print(" Raw: ");
            for (int i = 0; i < min((int)isrRxLen, MSG_LEN); i++) {
                if (rxMsg[i] < 16) Serial.print("0");
                Serial.print(rxMsg[i], HEX);
                Serial.print(" ");
            }
            Serial.print(" Str: ");
            Serial.println((char*)rxMsg);

            if (strncmp((char*)rxMsg, "PING", 4) == 0) {
                // Send PONG response
                Serial.print("[TX] Sending PONG...");
                sentAck = false;
                transmitMessage("PONG");

                unsigned long start = millis();
                while (!sentAck && millis() - start < 100) {
                    // Wait for interrupt
                }

                if (sentAck) {
                    txCount++;
                    Serial.println(" SENT!");
                } else {
                    Serial.println(" FAILED!");
                    errCount++;
                }
            }

            // Restart receiver
            startReceiver();
            Serial.println();
        }
    }

    // Print stats every 10 seconds
    if (millis() - lastPrint > 10000) {
        lastPrint = millis();
        Serial.println("--- STATS ---");
        Serial.print("TX: ");
        Serial.print(txCount);
        Serial.print(" | RX: ");
        Serial.print(rxCount);
        Serial.print(" | Errors: ");
        Serial.println(errCount);
        Serial.print("IRQ working: ");
        Serial.println((txCount > 0 || rxCount > 0) ? "YES" : "Testing...");
        Serial.println();
    }
}
