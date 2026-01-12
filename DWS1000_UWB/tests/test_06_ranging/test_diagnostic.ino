/**
 * Diagnostic DW1000 Ranging Test
 * Verbose output to debug why ranging isn't starting
 */
#include <SPI.h>
#include "DW1000Ranging.h"

// Forward declarations
void newRange();
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);

// connection pins
const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = SS; // spi select pin

// Set to true for ANCHOR, false for TAG
#define IS_ANCHOR false  // Change this for each board

void setup() {
  Serial.begin(115200);
  delay(2000);  // Give time for serial to connect

  Serial.println("========================================");
  Serial.println("DW1000 Ranging Diagnostic Test");
  Serial.println("========================================");

  if (IS_ANCHOR) {
    Serial.println("Mode: ANCHOR");
  } else {
    Serial.println("Mode: TAG");
  }

  Serial.println("Initializing DW1000...");
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  Serial.println("DW1000 initialized!");

  // Attach callbacks
  Serial.println("Attaching callbacks...");
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);
  Serial.println("Callbacks attached!");

  // Start as anchor or tag
  if (IS_ANCHOR) {
    Serial.println("Starting as ANCHOR...");
    Serial.println("Address: 82:17:5B:D5:A9:9A:E2:9C");
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    Serial.println("ANCHOR started! Waiting for tags...");
  } else {
    Serial.println("Starting as TAG...");
    Serial.println("Address: 7D:00:22:EA:82:60:3B:9C");
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    Serial.println("TAG started! Scanning for anchors...");
  }

  Serial.println("========================================");
  Serial.println("Entering main loop...");
  Serial.println("========================================");
}

unsigned long lastPrint = 0;

void loop() {
  DW1000Ranging.loop();

  // Print heartbeat every 5 seconds
  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    Serial.print("Heartbeat [");
    Serial.print(millis() / 1000);
    Serial.println("s] - Still running...");
  }
}

void newRange() {
  Serial.println("*** NEW RANGE MEASUREMENT ***");
  Serial.print("  from: ");
  Serial.print(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
  Serial.print("\t Range: ");
  Serial.print(DW1000Ranging.getDistantDevice()->getRange());
  Serial.print(" m");
  Serial.print("\t RX power: ");
  Serial.print(DW1000Ranging.getDistantDevice()->getRXPower());
  Serial.println(" dBm");
}

void newDevice(DW1000Device* device) {
  Serial.println("*** NEW DEVICE DETECTED ***");
  Serial.print("  Ranging init; device added! -> short: ");
  Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
  Serial.println("*** DEVICE INACTIVE ***");
  Serial.print("  Deleting inactive device: ");
  Serial.println(device->getShortAddress(), HEX);
}
