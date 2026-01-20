/**
 * TX Test with IRQ Callbacks
 *
 * Sends numbered test packets for the RX IRQ test to receive.
 * Uses IRQ-based completion instead of polling to match RX approach.
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Register addresses
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Stats
volatile uint32_t txCount = 0;
volatile uint32_t txGood = 0;
volatile bool txDone = false;
volatile bool txError = false;

// Callback handlers
void handleSent() {
    txGood++;
    txDone = true;
}

void handleError() {
    txError = true;
    Serial.println(F("[IRQ: TX Error]"));
}

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);

    if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        aonCtrl[0] |= 0x40;  // Set OTP_LDO bit
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        delay(1);
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== TX Test with IRQ Callbacks ==="));

    // Initialize DW1000 WITH IRQ
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Device info
    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    // Apply LDO tuning
    Serial.println(F("Applying LDO tuning..."));
    applyLDOTuning();

    // Configure
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    // Use long range mode - 110kbps, 2048 preamble, more robust
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    // Re-apply LDO after config
    applyLDOTuning();

    // Attach callbacks
    DW1000.attachSentHandler(handleSent);
    DW1000.attachErrorHandler(handleError);

    Serial.println(F("Ready to transmit"));
    Serial.println();
}

void loop() {
    // Send a packet every 2 seconds
    static uint32_t lastTx = 0;
    if (millis() - lastTx >= 2000) {
        lastTx = millis();
        txCount++;

        // Create message with sequence number
        char data[32];
        snprintf(data, sizeof(data), "PING#%05lu", txCount);

        txDone = false;
        txError = false;

        // Transmit
        DW1000.newTransmit();
        DW1000.setDefaults();
        DW1000.setData((byte*)data, strlen(data));
        DW1000.startTransmit();

        // Wait for completion (IRQ-based)
        uint32_t timeout = millis() + 100;
        while (!txDone && !txError && millis() < timeout) {
            // Busy wait for IRQ
            delayMicroseconds(100);
        }

        // Report result
        Serial.print(F("TX #"));
        Serial.print(txCount);
        Serial.print(F(" \""));
        Serial.print(data);
        Serial.print(F("\" "));

        if (txDone) {
            Serial.print(F("OK ("));
            Serial.print(txGood);
            Serial.print(F("/"));
            Serial.print(txCount);
            Serial.println(F(")"));
        } else if (txError) {
            Serial.println(F("ERROR"));
        } else {
            Serial.println(F("TIMEOUT"));
        }
    }

    delay(10);
}
