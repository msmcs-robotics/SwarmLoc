/**
 * Simple serial test - verify serial communication works
 */

#include <Arduino.h>
#include <SPI.h>
#include "DW1000.h"

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

void setup() {
    Serial.begin(115200);
    while (!Serial);  // Wait for serial
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("SIMPLE SERIAL TEST");
    Serial.println("=================================");

    Serial.println("Initializing DW1000...");

    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char deviceId[32];
    DW1000.getPrintableDeviceIdentifier(deviceId);

    Serial.print("Device ID: ");
    Serial.println(deviceId);

    if (strstr(deviceId, "DECA") != NULL) {
        Serial.println("STATUS: DW1000 OK");
    } else {
        Serial.println("STATUS: DW1000 FAIL");
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

    Serial.println("Setup complete!");
    Serial.println();
}

int counter = 0;

void loop() {
    counter++;
    Serial.print("Loop ");
    Serial.println(counter);
    delay(2000);
}
