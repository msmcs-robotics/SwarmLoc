/**
 * RX Debug Test v2
 *
 * Polls IRQ pin level and only processes when status indicates real frame.
 * Avoids spurious IRQ flood from edge-triggered interrupt.
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Register addresses
#define SYS_STATUS_REG  0x0F
#define SYS_MASK_REG    0x0E
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Status bit positions
#define RXDFR_BIT   13  // RX Data Frame Ready
#define RXFCG_BIT   14  // RX Frame Check Good (CRC OK)
#define RXFCE_BIT   15  // RX FCS Error (CRC bad)
#define RXRFSL_BIT  16  // RX Reed-Solomon Error
#define RXRFTO_BIT  17  // RX Frame Wait Timeout
#define RXPHE_BIT   10  // RX PHY Header Error
#define LDEERR_BIT  12  // LDE Error

// Stats
uint32_t rxTotal = 0;
uint32_t rxGood = 0;
uint32_t rxBadCrc = 0;
uint32_t rxOther = 0;
uint32_t pollCount = 0;
uint32_t irqHighCount = 0;

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);

    if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        aonCtrl[0] |= 0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        delay(1);
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
    }
}

void printHex(byte* data, int len) {
    for (int i = 0; i < len; i++) {
        if (data[i] < 16) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
}

void restartReceiver() {
    // Clear all status bits
    byte clearStatus[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0, clearStatus, 5);

    // Restart receive
    DW1000.newReceive();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Debug Test v2 ==="));
    Serial.println(F("Using IRQ level polling"));

    // Initialize WITHOUT library IRQ
    DW1000.begin(0xFF, PIN_RST);
    DW1000.select(PIN_SS);

    // Device info
    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    // Apply LDO tuning
    applyLDOTuning();

    // Configure
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_SHORTDATA_FAST_LOWPOWER);
    DW1000.setFrameFilter(false);
    DW1000.suppressFrameCheck(false);  // Check CRC
    DW1000.interruptOnReceived(true);
    DW1000.interruptOnReceiveFailed(true);
    DW1000.commitConfiguration();

    // Re-apply LDO after config
    applyLDOTuning();

    // IRQ pin as input (no interrupt attached)
    pinMode(PIN_IRQ, INPUT);

    // Clear status and start receiver
    restartReceiver();

    Serial.println(F("Receiver started. Polling IRQ pin..."));
    Serial.println();
}

void processReceive() {
    // Read status register
    byte status[5];
    DW1000.readBytes(SYS_STATUS_REG, 0, status, 5);

    // Check for SPI corruption (all FFs usually means corrupt read)
    if (status[0] == 0xFF && status[1] == 0xFF &&
        status[2] == 0xFF && status[3] == 0xFF) {
        // Likely corrupt read during RX - just clear and restart
        restartReceiver();
        return;
    }

    uint32_t s = status[0] | ((uint32_t)status[1] << 8) |
                 ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);

    // Only process if we have actual RX-related bits set
    bool hasRxBits = (s & ((1UL << RXDFR_BIT) | (1UL << RXFCG_BIT) |
                          (1UL << RXFCE_BIT) | (1UL << RXPHE_BIT) |
                          (1UL << LDEERR_BIT) | (1UL << RXRFTO_BIT)));

    if (!hasRxBits) {
        // Spurious - just clear and continue
        byte clearStatus[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0, clearStatus, 5);
        return;
    }

    rxTotal++;

    // Check for valid frame (RXFCG set = CRC good)
    if (s & (1UL << RXFCG_BIT)) {
        rxGood++;

        uint16_t len = DW1000.getDataLength();
        Serial.print(F("[RX #"));
        Serial.print(rxGood);
        Serial.print(F("] len="));
        Serial.print(len);

        if (len > 0 && len < 128) {
            byte data[128];
            DW1000.getData(data, len);
            Serial.print(F(" data=\""));
            for (int i = 0; i < min((int)len, 32); i++) {
                Serial.write(data[i] >= 32 && data[i] < 127 ? data[i] : '.');
            }
            Serial.print(F("\""));
        }

        Serial.print(F(" ("));
        Serial.print(rxGood);
        Serial.print(F("/"));
        Serial.print(rxTotal);
        Serial.println(F(")"));
    }
    else if (s & (1UL << RXFCE_BIT)) {
        // Bad CRC
        rxBadCrc++;
    }
    else {
        // Other error
        rxOther++;
    }

    // Restart receiver
    restartReceiver();
}

void loop() {
    static uint32_t lastStats = 0;

    pollCount++;

    // Check IRQ pin level
    if (digitalRead(PIN_IRQ)) {
        irqHighCount++;
        processReceive();
    }

    // Print stats periodically
    if (millis() - lastStats >= 5000) {
        lastStats = millis();

        Serial.print(F("[Stats t="));
        Serial.print(millis() / 1000);
        Serial.print(F("s] Good:"));
        Serial.print(rxGood);
        Serial.print(F(" BadCRC:"));
        Serial.print(rxBadCrc);
        Serial.print(F(" Other:"));
        Serial.print(rxOther);
        Serial.print(F(" Total:"));
        Serial.print(rxTotal);
        Serial.print(F(" IRQ:"));
        Serial.print(irqHighCount);
        Serial.println();
    }

    // Small delay to prevent tight polling
    delayMicroseconds(100);
}
