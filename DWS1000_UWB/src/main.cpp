/**
 * DW1000 Ranging Test - Verbose Debug Version
 * Bug fix applied: LEN_SYS_MASK corrected in DW1000.cpp
 * Expected distance: 86.36 cm (34 inches)
 *
 * CRITICAL FIX 2026-01-11:
 * DWS1000 Shield (PCL298336) uses different pins than library defaults!
 * - RST is on D7 (not D9)
 * - IRQ is on D8 (not D2) - REQUIRES JUMPER WIRE TO D2!
 *
 * You MUST add a jumper wire from D8 to D2 on the Arduino for interrupts to work.
 * Arduino Uno only supports hardware interrupts on pins 2 and 3.
 */
#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

// Forward declarations
void newRange();
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);

// DWS1000 Shield (PCL298336) Pin Configuration
// IMPORTANT: These differ from library defaults!
const uint8_t PIN_RST = 7;   // DWS1000 uses D7 for reset (not D9!)
const uint8_t PIN_IRQ = 2;   // IRQ needs jumper from D8 to D2
const uint8_t PIN_SS = 10;   // SPI chip select

// Set to true for ANCHOR, false for TAG
#define IS_ANCHOR false

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("");
  Serial.println("=============================================");
  Serial.println("DW1000 Ranging Test - DWS1000 Shield");
  Serial.println("Part: PCL298336");
  Serial.println(IS_ANCHOR ? "Mode: ANCHOR" : "Mode: TAG");
  Serial.println("Expected distance: 86.36 cm (34 inches)");
  Serial.println("=============================================");
  Serial.println("");

  // Show pin configuration warning
  Serial.println("[CONFIG] DWS1000 Shield Pin Configuration:");
  Serial.println("  RST = D7, IRQ = D2 (via jumper from D8), SS = D10");
  Serial.println("");
  Serial.println("[WARN] Did you add jumper wire from D8 to D2?");
  Serial.println("       Without it, interrupts won't work!");
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

  // Initialize SPI first for diagnostic
  Serial.println("[INIT] Starting DW1000 initialization...");
  Serial.print("[INIT] Pins: RST=");
  Serial.print(PIN_RST);
  Serial.print(", IRQ=");
  Serial.print(PIN_IRQ);
  Serial.print(", SS=");
  Serial.println(PIN_SS);

  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  Serial.println("[INIT] Communication initialized");

  // Verify SPI is working by reading device ID
  char deviceId[128];
  DW1000.getPrintableDeviceIdentifier(deviceId);
  Serial.print("[DIAG] Device ID: ");
  Serial.println(deviceId);

  if (strstr(deviceId, "DECA") != NULL) {
    Serial.println("[DIAG] SPI OK - DW1000 detected!");
  } else {
    Serial.println("[DIAG] WARNING - Device ID not 'DECA'!");
    Serial.println("[DIAG] Check: Shield seated? Jumper wire? Power?");
  }

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
  float error_cm = range_cm - 86.36;  // Expected distance

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
