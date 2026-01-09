/**
 * Test 1: Simple Chip ID Read (No Serial Wait)
 *
 * Simplified version that doesn't wait for Serial connection
 */

#include <SPI.h>

#define PIN_CS   10
#define PIN_RST  9

#define REG_DEV_ID  0x00
#define SPI_SPEED_SLOW  2000000

SPISettings spiSettings(SPI_SPEED_SLOW, MSBFIRST, SPI_MODE0);

uint32_t readDeviceID();

void setup() {
  Serial.begin(9600);
  delay(2000);  // Give time for serial to initialize

  Serial.println();
  Serial.println("=== Test 1: Chip ID ===");

  // Init SPI
  SPI.begin();
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH);

  // Reset
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(100);

  // Read ID
  uint32_t id = readDeviceID();

  Serial.print("Device ID: 0x");
  Serial.println(id, HEX);

  if (id == 0xDECA0302) {
    Serial.println("SUCCESS: DWM3000 detected!");
  } else if (id == 0x00000000) {
    Serial.println("FAIL: No response (check wiring)");
  } else if (id == 0xFFFFFFFF) {
    Serial.println("FAIL: Bus floating (check power)");
  } else {
    Serial.print("FAIL: Unexpected ID: 0x");
    Serial.println(id, HEX);
  }
}

void loop() {
  delay(1000);
}

uint32_t readDeviceID() {
  SPI.beginTransaction(spiSettings);
  digitalWrite(PIN_CS, LOW);

  SPI.transfer(0x00);  // Register address

  uint8_t b0 = SPI.transfer(0x00);
  uint8_t b1 = SPI.transfer(0x00);
  uint8_t b2 = SPI.transfer(0x00);
  uint8_t b3 = SPI.transfer(0x00);

  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();

  return (uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);
}
