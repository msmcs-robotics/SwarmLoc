/**
 * Clean DW1000 Ranging Test - No Heartbeat
 * Bug fix applied: LEN_SYS_MASK corrected in DW1000.cpp
 */
#include <SPI.h>
#include "DW1000Ranging.h"

// Forward declarations
void newRange();
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);

// connection pins
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Set to true for ANCHOR, false for TAG
#define IS_ANCHOR false

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("DW1000 Ranging Test (Bug Fixed)");
  Serial.println(IS_ANCHOR ? "Mode: ANCHOR" : "Mode: TAG");

  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);

  if (IS_ANCHOR) {
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    Serial.println("ANCHOR ready");
  } else {
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    Serial.println("TAG ready");
  }
}

void loop() {
  DW1000Ranging.loop();
}

void newRange() {
  Serial.print("Range: ");
  Serial.print(DW1000Ranging.getDistantDevice()->getRange());
  Serial.print(" m (");
  Serial.print(DW1000Ranging.getDistantDevice()->getRange() * 100);
  Serial.print(" cm) from ");
  Serial.println(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
}

void newDevice(DW1000Device* device) {
  Serial.print("Device found: ");
  Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
  Serial.print("Device lost: ");
  Serial.println(device->getShortAddress(), HEX);
}
