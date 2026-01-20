/**
 * DW1000Ranging Tag Test
 *
 * Uses the battle-tested DW1000Ranging library to see if ranging works.
 * This handles all the low-level TX/RX details automatically.
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000Ranging.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// LDO tuning
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);
    Serial.print(F("LDO: 0x"));
    Serial.println(ldoTune[0], HEX);

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

void newRange() {
    Serial.print(F("Range: "));
    Serial.print(DW1000Ranging.getDistantDevice()->getRange());
    Serial.print(F(" m to anchor "));
    Serial.println(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
}

void newDevice(DW1000Device* device) {
    Serial.print(F("Found anchor: "));
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print(F("Lost anchor: "));
    Serial.println(device->getShortAddress(), HEX);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== DW1000 Ranging - TAG ==="));

    // Initialize DW1000
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

    // Apply LDO tuning before starting
    applyLDOTuning();

    // Attach callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as tag with address "AA:BB:CC:DD:EE:FF:00:02"
    // Using long range mode for better reliability
    DW1000Ranging.startAsTag("AA:BB:CC:DD:EE:FF:00:02",
                             DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);

    // Re-apply LDO after configuration
    applyLDOTuning();

    Serial.println(F("Tag ready, looking for anchors..."));
}

void loop() {
    DW1000Ranging.loop();
}
