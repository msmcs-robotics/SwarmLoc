/**
 * DW1000Ranging Tag - Debug Version
 * Adds verbose debug output to identify issues
 */
#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

// Forward declarations
void newRange();
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);

// connection pins
const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = SS; // spi select pin

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== DW1000 Tag Debug ===");
  Serial.print("RST Pin: "); Serial.println(PIN_RST);
  Serial.print("IRQ Pin: "); Serial.println(PIN_IRQ);
  Serial.print("SS Pin: "); Serial.println(PIN_SS);

  Serial.println("Initializing DW1000...");

  //init the configuration
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);

  Serial.println("Communication initialized");

  //define the sketch as tag
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);

  Serial.println("Callbacks attached");

  //Enable the filter to smooth the distance
  //DW1000Ranging.useRangeFilter(true);

  Serial.println("Starting as tag...");

  //we start the module as a tag
  DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);

  Serial.println("Tag started successfully");
  Serial.println("Searching for anchors...");
  Serial.println("==============================");
}

unsigned long lastPrint = 0;
unsigned long loopCount = 0;

void loop() {
  DW1000Ranging.loop();
  loopCount++;

  // Print status every 5 seconds
  if (millis() - lastPrint > 5000) {
    Serial.print("ALIVE - Loop count: ");
    Serial.print(loopCount);
    Serial.print(" - Millis: ");
    Serial.println(millis());
    lastPrint = millis();
    loopCount = 0;
  }
}

void newRange() {
  Serial.println(">>> NEW RANGE EVENT <<<");
  Serial.print("from: "); Serial.print(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
  Serial.print("\t Range: "); Serial.print(DW1000Ranging.getDistantDevice()->getRange()); Serial.print(" m");
  Serial.print("\t RX power: "); Serial.print(DW1000Ranging.getDistantDevice()->getRXPower()); Serial.println(" dBm");
}

void newDevice(DW1000Device* device) {
  Serial.println(">>> NEW DEVICE EVENT <<<");
  Serial.print("ranging init; 1 device added ! -> ");
  Serial.print(" short:");
  Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
  Serial.println(">>> INACTIVE DEVICE EVENT <<<");
  Serial.print("delete inactive device: ");
  Serial.println(device->getShortAddress(), HEX);
}
