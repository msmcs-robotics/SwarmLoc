/**
 * DWS1000 Polling Mode Test - RECEIVER
 *
 * This test uses POLLING instead of interrupts to diagnose communication.
 * It directly reads SYS_STATUS register to detect TX/RX completion.
 *
 * Upload to: /dev/ttyACM1
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
    Serial.println("DWS1000 POLLING MODE DIAGNOSTIC - RX");
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

    // Configure - SAME settings as transmitter!
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);  // Different address than TX
    DW1000.setNetworkId(10);     // SAME network ID
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    char mode[128];
    DW1000.getPrintableDeviceMode(mode);
    Serial.print("Mode: ");
    Serial.println(mode);

    clearStatus();

    Serial.println("\nStarting RX test - listening for frames...\n");

    // Start receiver in permanent mode
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);  // Keep listening
    DW1000.startReceive();

    Serial.println("Receiver started - waiting for PING...\n");
}

unsigned long lastCheck = 0;
unsigned long lastStatusPrint = 0;

void loop() {
    // Check for received frame
    uint32_t status = readStatusLow32();

    // Check every 10ms
    if (millis() - lastCheck < 10) return;
    lastCheck = millis();

    // Print heartbeat status every 5 seconds
    if (millis() - lastStatusPrint > 5000) {
        lastStatusPrint = millis();
        Serial.print("... waiting (RX=");
        Serial.print(rxCount);
        Serial.print(" TX=");
        Serial.print(txCount);
        Serial.print(") status=0x");
        Serial.println(status, HEX);
    }

    // Good frame received?
    if (status & (1UL << RXFCG_BIT)) {
        rxCount++;

        // Read the received data
        uint16_t len = DW1000.getDataLength();
        byte rxData[64];
        memset(rxData, 0, 64);
        DW1000.getData(rxData, len);

        Serial.print("[RX #");
        Serial.print(rxCount);
        Serial.print("] len=");
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

        // Check if it's a PING and respond with PONG
        if (len >= 4 && rxData[0] == 'P' && rxData[1] == 'I' && rxData[2] == 'N' && rxData[3] == 'G') {
            Serial.print("  -> Sending PONG response... ");

            // Clear status
            clearStatus();

            // Send PONG
            DW1000.newTransmit();
            DW1000.setDefaults();
            byte pong[] = "PONG5678";
            DW1000.setData(pong, 8);
            DW1000.startTransmit();

            // Poll for TX complete
            unsigned long start = millis();
            bool sent = false;
            while (millis() - start < 100) {
                if (readStatusLow32() & (1UL << TXFRS_BIT)) {
                    sent = true;
                    break;
                }
            }

            if (sent) {
                txCount++;
                Serial.println("SENT!");
            } else {
                Serial.println("TX FAILED!");
            }

            // Restart receiver
            clearStatus();
            DW1000.newReceive();
            DW1000.setDefaults();
            DW1000.receivePermanently(true);
            DW1000.startReceive();
        } else {
            // Clear and continue
            clearStatus();
        }
    } else if (status & (1UL << RXFCE_BIT)) {
        // CRC error
        Serial.println("[ERROR] CRC error on received frame");
        clearStatus();
        // Receiver should auto-restart due to receivePermanently(true)
    }
}
