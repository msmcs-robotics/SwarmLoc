/**
 * TX Test - 6.8Mbps Mode
 *
 * Same as main TX but uses MODE_LONGDATA_FAST_LOWPOWER (6.8Mbps).
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

    Serial.println(F("\n=== TX 6.8Mbps Test ==="));

    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_FAST_LOWPOWER);
    DW1000.commitConfiguration();
    applyLDOTuning();

    DW1000.getPrintableDeviceMode(msg);
    Serial.print(F("Mode: "));
    Serial.println(msg);

    DW1000.attachSentHandler(handleSent);

    Serial.println(F("Ready"));
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
