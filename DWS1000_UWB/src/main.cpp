/**
 * DWS1000 Polling Mode Test - NO INTERRUPTS
 *
 * Since IRQ isn't reaching D2, this test uses polling instead.
 * Polls SYS_STATUS register directly to detect TX/RX events.
 *
 * DWS1000 Shield Pin Configuration:
 * RST = D7, SS = D10, SPI = D11/12/13
 * IRQ = NOT USED (polling mode)
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

const uint8_t PIN_RST = 7;
const uint8_t PIN_SS = 10;

// SYS_STATUS bit masks (from DW1000 User Manual)
#define SYS_STATUS_TXFRS  0x00000080UL  // TX frame sent
#define SYS_STATUS_RXDFR  0x00002000UL  // RX data frame ready
#define SYS_STATUS_RXFCG  0x00004000UL  // RX FCS good
#define SYS_STATUS_RXFCE  0x00008000UL  // RX FCS error
#define SYS_STATUS_RXOVRR 0x00100000UL  // RX overrun
#define SYS_STATUS_RXPTO  0x00200000UL  // Preamble timeout
#define SYS_STATUS_RXSFDTO 0x04000000UL // SFD timeout

// Mode: 0 = receiver, 1 = transmitter
#define IS_TRANSMITTER true

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
    Serial.println("DWS1000 POLLING MODE TEST - NO INTERRUPTS");
    Serial.println(IS_TRANSMITTER ? "Mode: TRANSMITTER" : "Mode: RECEIVER");
    Serial.println("=============================================");
    Serial.println();

    // Initialize without IRQ (pass 255 or invalid pin)
    Serial.println("[INIT] Starting DW1000 (no IRQ)...");

    // Use begin with just reset pin, no interrupt
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(10);
    digitalWrite(PIN_RST, HIGH);
    delay(10);

    SPI.begin();
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
    Serial.println("[PASS] SPI working");

    // Configure DW1000
    Serial.println("[INIT] Configuring...");
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(IS_TRANSMITTER ? 1 : 2);
    DW1000.setNetworkId(0xDECA);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    char mode[128];
    DW1000.getPrintableDeviceMode(mode);
    Serial.print("[INIT] Mode: ");
    Serial.println(mode);

    // Clear status register
    clearStatus();

    if (!IS_TRANSMITTER) {
        // Start receiver
        Serial.println("[INIT] Starting receiver...");
        startReceiver();
    }

    Serial.println();
    Serial.println("=============================================");
    Serial.println("RUNNING - Polling for TX/RX events");
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

unsigned long lastPrint = 0;

void loop() {
    if (IS_TRANSMITTER) {
        // === TRANSMITTER MODE ===
        if (millis() - lastTx > 1000) {
            lastTx = millis();
            txCount++;

            Serial.print("[TX #");
            Serial.print(txCount);
            Serial.print("] Sending PING...");

            transmitMessage("PING");

            if (waitForTxComplete(100)) {
                Serial.println(" SENT!");

                // Wait for PONG response
                startReceiver();
                Serial.print("[RX] Waiting for PONG...");

                if (waitForRxComplete(500)) {
                    // Read received data
                    DW1000.getData(rxMsg, MSG_LEN);
                    Serial.print(" GOT: ");
                    Serial.println((char*)rxMsg);

                    if (strncmp((char*)rxMsg, "PONG", 4) == 0) {
                        rxCount++;
                        Serial.println("[SUCCESS] PONG received!");
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
        // === RECEIVER MODE ===
        uint32_t status = readStatus();

        if (status & SYS_STATUS_RXFCG) {
            // Good frame received
            rxCount++;
            clearStatus();

            // Get data length first
            uint16_t len = DW1000.getDataLength();
            DW1000.getData(rxMsg, MSG_LEN);

            Serial.print("[RX #");
            Serial.print(rxCount);
            Serial.print("] Len=");
            Serial.print(len);
            Serial.print(" Raw: ");
            for (int i = 0; i < min((int)len, MSG_LEN); i++) {
                if (rxMsg[i] < 16) Serial.print("0");
                Serial.print(rxMsg[i], HEX);
                Serial.print(" ");
            }
            Serial.print(" Str: ");
            Serial.println((char*)rxMsg);

            if (strncmp((char*)rxMsg, "PING", 4) == 0) {
                // Send PONG response
                Serial.print("[TX] Sending PONG...");
                transmitMessage("PONG");

                if (waitForTxComplete(100)) {
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
        else if (status & (SYS_STATUS_RXFCE | SYS_STATUS_RXOVRR)) {
            // Error
            errCount++;
            Serial.print("[ERR] RX error, status: 0x");
            Serial.println(status, HEX);
            clearStatus();
            startReceiver();
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
        Serial.println();
    }
}
