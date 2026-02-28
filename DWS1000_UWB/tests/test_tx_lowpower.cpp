/**
 * TX Test - Minimum Power
 *
 * Same as main TX but sets TX_POWER to minimum (0x00000000).
 * Tests whether signal saturation at close range (<1m) causes
 * the receiver's PHY header errors.
 *
 * PIN_RST = 7 (correct for DWS1000 shield)
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 7;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02
#define TX_POWER_REG    0x1E

volatile uint32_t txCount = 0;
volatile uint32_t txGood = 0;
volatile bool txDone = false;

void handleSent() {
    txGood++;
    txDone = true;
}

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

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== TX Low Power Test ==="));

    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();
    applyLDOTuning();

    // Read default TX power
    byte txPow[4];
    DW1000.readBytes(TX_POWER_REG, 0x00, txPow, 4);
    Serial.print(F("Default TX_POWER: 0x"));
    for (int i = 3; i >= 0; i--) {
        if (txPow[i] < 0x10) Serial.print('0');
        Serial.print(txPow[i], HEX);
    }
    Serial.println();

    // Set MINIMUM TX power
    byte minPow[4] = {0x00, 0x00, 0x00, 0x00};
    DW1000.writeBytes(TX_POWER_REG, 0x00, minPow, 4);

    // Verify
    DW1000.readBytes(TX_POWER_REG, 0x00, txPow, 4);
    Serial.print(F("New TX_POWER: 0x"));
    for (int i = 3; i >= 0; i--) {
        if (txPow[i] < 0x10) Serial.print('0');
        Serial.print(txPow[i], HEX);
    }
    Serial.println();

    DW1000.attachSentHandler(handleSent);

    Serial.println(F("Ready (MIN POWER)"));
    Serial.println();
}

void loop() {
    static uint32_t lastTx = 0;
    if (millis() - lastTx >= 2000) {
        lastTx = millis();
        txCount++;

        char data[32];
        snprintf(data, sizeof(data), "PING#%05lu", txCount);

        txDone = false;

        DW1000.newTransmit();
        DW1000.setDefaults();
        DW1000.setData((byte*)data, strlen(data));
        DW1000.startTransmit();

        uint32_t timeout = millis() + 100;
        while (!txDone && millis() < timeout) {
            delayMicroseconds(100);
        }

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
        } else {
            Serial.println(F("TIMEOUT"));
        }
    }

    delay(10);
}
