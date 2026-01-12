/**
 * DW1000 Ranging Test - Verbose Debug Version
 * Bug fix applied: LEN_SYS_MASK corrected in DW1000.cpp
 * Expected distance: 45.72 cm (18 inches)
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

  Serial.println("");
  Serial.println("========================================");
  Serial.println("DW1000 Ranging Test (Bug Fixed)");
  Serial.println(IS_ANCHOR ? "Mode: ANCHOR" : "Mode: TAG");
  Serial.println("Expected distance: 45.72 cm (18 inches)");
  Serial.println("========================================");
  Serial.println("");

  // WAIT FOR USER INPUT BEFORE STARTING
  Serial.println(">>> Send any character to start ranging <<<");
  Serial.println("(This allows all devices to be uploaded first)");
  Serial.println("");

  while (!Serial.available()) {
    delay(100);  // Wait for user input
  }

  // Clear the input buffer
  while (Serial.available()) {
    Serial.read();
  }

  Serial.println("[USER] Start command received!");
  Serial.println("");

  Serial.println("[INIT] Starting DW1000 initialization...");
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  Serial.println("[INIT] Communication initialized");

  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);
  Serial.println("[INIT] Handlers attached");

  if (IS_ANCHOR) {
    Serial.println("[INIT] Starting as ANCHOR...");
    DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    Serial.println("[READY] ANCHOR mode active - Listening for TAGs");
  } else {
    Serial.println("[INIT] Starting as TAG...");
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
    Serial.println("[READY] TAG mode active - Searching for ANCHORs");
  }
  Serial.println("");
}

void loop() {
  DW1000Ranging.loop();
}

void newRange() {
  float range_m = DW1000Ranging.getDistantDevice()->getRange();
  float range_cm = range_m * 100.0;
  float error_cm = range_cm - 45.72;  // Expected distance

  Serial.print("[RANGE] ");
  Serial.print(range_m);
  Serial.print(" m (");
  Serial.print(range_cm);
  Serial.print(" cm) | Error: ");
  Serial.print(error_cm > 0 ? "+" : "");
  Serial.print(error_cm);
  Serial.print(" cm | from ");
  Serial.println(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
}

void newDevice(DW1000Device* device) {
  Serial.print("[DEVICE] Found: ");
  Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
  Serial.print("[DEVICE] Lost: ");
  Serial.println(device->getShortAddress(), HEX);
}
