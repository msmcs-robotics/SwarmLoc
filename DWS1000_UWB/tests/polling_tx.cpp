/**
 * DWS1000 Polling Mode Test - TRANSMITTER
 *
 * This test uses POLLING instead of interrupts to diagnose communication.
 * It directly reads SYS_STATUS register to detect TX/RX completion.
 *
 * Upload to: /dev/ttyACM0
 */

#include <SPI.h>
#include "DW1000.h"

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;  // Not used in polling mode
const uint8_t PIN_SS = SS;

// SYS_STATUS register bits
#define SYS_STATUS_REG 0x0F
#define TXFRS_BIT 7   // TX frame sent
#define RXDFR_BIT 13  // RX data frame ready
#define RXFCG_BIT 14  // RX FCS good (CRC ok)
#define RXFCE_BIT 15  // RX FCS error

unsigned long txCount = 0;
unsigned long rxCount = 0;
unsigned long lastTx = 0;

void clearStatus() {
    byte clear[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clear, 5);
}

uint32_t readStatusLow32() {
    byte status[4];
    DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 4);
    return ((uint32_t)status[3] << 24) | ((uint32_t)status[2] << 16) |
           ((uint32_t)status[1] << 8) | status[0];
}

void printStatusBits(uint32_t status) {
    Serial.print("  Status=0x");
    Serial.print(status, HEX);
    if (status & (1UL << TXFRS_BIT)) Serial.print(" TXFRS");
    if (status & (1UL << RXDFR_BIT)) Serial.print(" RXDFR");
    if (status & (1UL << RXFCG_BIT)) Serial.print(" RXFCG");
    if (status & (1UL << RXFCE_BIT)) Serial.print(" RXFCE");
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("DWS1000 POLLING MODE DIAGNOSTIC - TX");
    Serial.println("==========================================");
    Serial.println();

    // Initialize DW1000
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char deviceId[32];
    DW1000.getPrintableDeviceIdentifier(deviceId);
    Serial.print("Device ID: ");
    Serial.println(deviceId);

    if (strstr(deviceId, "DECA") == NULL) {
        Serial.println("[FAIL] DW1000 not detected!");
        while(1);
    }

    // Configure
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    char mode[128];
    DW1000.getPrintableDeviceMode(mode);
    Serial.print("Mode: ");
    Serial.println(mode);

    clearStatus();

    Serial.println("\nStarting TX test - sending PING every 2 seconds...\n");
}

bool pollTxComplete(unsigned long timeout) {
    unsigned long start = millis();
    while (millis() - start < timeout) {
        uint32_t status = readStatusLow32();
        if (status & (1UL << TXFRS_BIT)) {
            return true;
        }
    }
    return false;
}

bool pollRxComplete(unsigned long timeout, uint32_t* statusOut) {
    unsigned long start = millis();
    while (millis() - start < timeout) {
        uint32_t status = readStatusLow32();
        *statusOut = status;
        if (status & (1UL << RXFCG_BIT)) {
            return true;  // Good frame received
        }
        if (status & (1UL << RXFCE_BIT)) {
            Serial.println("  [CRC ERROR]");
            return false;
        }
    }
    return false;
}

void loop() {
    if (millis() - lastTx < 2000) return;
    lastTx = millis();
    txCount++;

    Serial.print("[TX #");
    Serial.print(txCount);
    Serial.print("] ");

    // Clear status and send
    clearStatus();

    // Prepare TX frame
    DW1000.newTransmit();
    DW1000.setDefaults();

    // Create message - SIMPLE ASCII
    byte msg[] = "PING1234";
    DW1000.setData(msg, 8);
    DW1000.startTransmit();

    // Poll for TX complete
    if (pollTxComplete(100)) {
        Serial.print("SENT ");

        // Now switch to RX and wait for response
        clearStatus();
        DW1000.newReceive();
        DW1000.setDefaults();
        DW1000.receivePermanently(false);
        DW1000.startReceive();

        Serial.print("| Waiting for PONG...");

        uint32_t rxStatus;
        if (pollRxComplete(1000, &rxStatus)) {
            rxCount++;

            // Read the data
            uint16_t len = DW1000.getDataLength();
            byte rxData[64];
            memset(rxData, 0, 64);
            DW1000.getData(rxData, len);

            Serial.print(" GOT len=");
            Serial.print(len);
            Serial.print(" hex=");
            for (int i = 0; i < min((int)len, 16); i++) {
                if (rxData[i] < 16) Serial.print("0");
                Serial.print(rxData[i], HEX);
                Serial.print(" ");
            }
            Serial.print(" str=\"");
            for (int i = 0; i < min((int)len, 16); i++) {
                char c = rxData[i];
                if (c >= 32 && c < 127) Serial.print(c);
                else Serial.print('.');
            }
            Serial.println("\"");
        } else {
            Serial.println(" TIMEOUT");
            printStatusBits(rxStatus);
        }
    } else {
        Serial.println("TX FAILED");
    }

    Serial.print("Stats: TX=");
    Serial.print(txCount);
    Serial.print(" RX=");
    Serial.println(rxCount);
    Serial.println();
}
