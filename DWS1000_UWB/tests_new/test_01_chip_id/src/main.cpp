/**
 * Test 1: Basic SPI Communication - Read Chip ID
 *
 * This test verifies that we can communicate with the DWM3000 chip via SPI.
 * We read the device ID register and check if it matches the expected value.
 *
 * Expected Device ID: 0xDECA0302 (DW3110 chip in DWM3000 module)
 *
 * Hardware: Arduino Uno + PCL298336 DWM3000EVB Shield
 *
 * Pins:
 *   MOSI: D11
 *   MISO: D12
 *   SCK:  D13
 *   CS:   D10
 *   RST:  D9
 *   IRQ:  D2
 */

#include <SPI.h>

// Pin definitions
#define PIN_CS   10
#define PIN_RST  9
#define PIN_IRQ  2

// DWM3000 Register Addresses
#define REG_DEV_ID  0x00  // Device ID register (4 bytes)

// SPI Settings
#define SPI_SPEED_SLOW  2000000  // 2 MHz for initialization
SPISettings spiSettings(SPI_SPEED_SLOW, MSBFIRST, SPI_MODE0);

// Expected device ID for DW3110 (DWM3000)
#define EXPECTED_DEV_ID  0xDECA0302

// Function prototype
uint32_t readDeviceID();

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect
  }

  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  Test 1: DWM3000 Chip ID Read");
  Serial.println("========================================");
  Serial.println();

  // Initialize SPI
  Serial.println("[1] Initializing SPI...");
  SPI.begin();
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);  // CS idle high
  Serial.println("    SPI initialized at 2 MHz");

  // Initialize Reset pin
  Serial.println("[2] Configuring RST pin...");
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH);
  Serial.println("    RST pin set HIGH");

  // Perform hardware reset
  Serial.println("[3] Performing hardware reset...");
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(10);
  Serial.println("    Reset complete");

  // Small delay for chip to initialize
  delay(100);

  // Read Device ID
  Serial.println("[4] Reading Device ID register...");
  Serial.println("    Register address: 0x00");
  Serial.println("    Expected value: 0xDECA0302");
  Serial.println();

  uint32_t deviceID = readDeviceID();

  Serial.print("    Device ID: 0x");
  Serial.println(deviceID, HEX);
  Serial.println();

  // Check result
  if (deviceID == EXPECTED_DEV_ID) {
    Serial.println("========================================");
    Serial.println("  ✓ SUCCESS: DWM3000 chip detected!");
    Serial.println("========================================");
    Serial.println();
    Serial.println("SPI communication is working correctly.");
    Serial.println("The DWM3000 module is responding.");
    Serial.println();
    Serial.println("Next step: Run Test 2 (GPIO & Reset)");
  } else if (deviceID == 0x00000000) {
    Serial.println("========================================");
    Serial.println("  ✗ FAIL: No response from chip");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Possible issues:");
    Serial.println("  1. Shield not properly seated on Arduino");
    Serial.println("  2. No power to shield (check 3.3V connection)");
    Serial.println("  3. SPI wiring issue (MOSI/MISO swapped?)");
    Serial.println("  4. CS pin not connected");
    Serial.println();
    Serial.println("Troubleshooting steps:");
    Serial.println("  1. Check that shield is fully inserted");
    Serial.println("  2. Measure voltage at shield: should be 3.3V");
    Serial.println("  3. Try re-seating the shield");
    Serial.println("  4. Check for bent pins on shield headers");
  } else if (deviceID == 0xFFFFFFFF) {
    Serial.println("========================================");
    Serial.println("  ✗ FAIL: SPI bus floating");
    Serial.println("========================================");
    Serial.println();
    Serial.println("All bits HIGH suggests:");
    Serial.println("  1. MISO line not connected");
    Serial.println("  2. Shield not powered");
    Serial.println("  3. Wrong CS pin selected");
  } else {
    Serial.println("========================================");
    Serial.println("  ✗ FAIL: Unexpected device ID");
    Serial.println("========================================");
    Serial.println();
    Serial.print("  Expected: 0x");
    Serial.println(EXPECTED_DEV_ID, HEX);
    Serial.print("  Got:      0x");
    Serial.println(deviceID, HEX);
    Serial.println();
    Serial.println("This might indicate:");
    Serial.println("  1. Wrong chip (not DWM3000/DW3110)");
    Serial.println("  2. Byte order issue");
    Serial.println("  3. SPI mode mismatch");
    Serial.println();
    Serial.println("Trying byte-swapped interpretation...");
    uint32_t swapped = ((deviceID & 0xFF) << 24) |
                      ((deviceID & 0xFF00) << 8) |
                      ((deviceID & 0xFF0000) >> 8) |
                      ((deviceID & 0xFF000000) >> 24);
    Serial.print("  Byte-swapped: 0x");
    Serial.println(swapped, HEX);
  }

  Serial.println();
  Serial.println("Test complete.");
}

void loop() {
  // Nothing to do in loop - test runs once in setup()
  delay(1000);
}

/**
 * Read the 32-bit Device ID from register 0x00
 *
 * DWM3000 SPI Protocol:
 *   - First byte: Header (register address + read/write bit)
 *   - Subsequent bytes: Data
 *
 * For reading:
 *   Header byte = register_address (bit 7 = 0 for read)
 *
 * Device ID is 4 bytes at address 0x00
 */
uint32_t readDeviceID() {
  uint8_t header = 0x00;  // Register 0x00, read mode
  uint32_t deviceID = 0;

  // Begin SPI transaction
  SPI.beginTransaction(spiSettings);
  digitalWrite(PIN_CS, LOW);

  // Send header byte (register address)
  SPI.transfer(header);

  // Read 4 bytes of device ID (little-endian)
  uint8_t byte0 = SPI.transfer(0x00);  // LSB
  uint8_t byte1 = SPI.transfer(0x00);
  uint8_t byte2 = SPI.transfer(0x00);
  uint8_t byte3 = SPI.transfer(0x00);  // MSB

  // End SPI transaction
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();

  // Combine bytes into 32-bit value (little-endian)
  deviceID = (uint32_t)byte0 |
             ((uint32_t)byte1 << 8) |
             ((uint32_t)byte2 << 16) |
             ((uint32_t)byte3 << 24);

  // Debug output
  Serial.print("    Raw bytes: 0x");
  if (byte0 < 0x10) Serial.print("0");
  Serial.print(byte0, HEX);
  Serial.print(" 0x");
  if (byte1 < 0x10) Serial.print("0");
  Serial.print(byte1, HEX);
  Serial.print(" 0x");
  if (byte2 < 0x10) Serial.print("0");
  Serial.print(byte2, HEX);
  Serial.print(" 0x");
  if (byte3 < 0x10) Serial.print("0");
  Serial.println(byte3, HEX);

  return deviceID;
}
