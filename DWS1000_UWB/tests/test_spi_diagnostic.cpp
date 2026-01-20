/**
 * SPI Diagnostic Test
 *
 * Tests SPI communication reliability by reading various registers
 * and comparing against known/expected values.
 *
 * This helps identify if the RX status corruption is:
 * 1. SPI timing issue
 * 2. Register-specific issue
 * 3. Mode-specific issue (only in RX mode)
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Register addresses
#define DEV_ID_REG      0x00
#define SYS_CFG_REG     0x04
#define SYS_STATUS_REG  0x0F
#define SYS_STATE_REG   0x19
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Known values
// DEV_ID should be: RIDTAG=0xDECA, MODEL=1, VER=3, REV=0 -> 0xDECA0130
#define EXPECTED_DEV_ID_LOW  0x30  // Byte 0: REV=0, VER=3
#define EXPECTED_DEV_ID_HIGH 0x01  // Byte 1: MODEL=1

uint32_t totalReads = 0;
uint32_t corruptReads = 0;
uint32_t goodReads = 0;

// Direct SPI read - bypasses library caching
void readRegisterDirect(uint8_t reg, uint8_t subAddr, uint8_t* data, uint8_t len) {
    // Build header
    uint8_t header[3];
    uint8_t headerLen = 1;

    // Read operation, no sub-address extension initially
    header[0] = reg & 0x3F;  // Register address, read mode (bit 7 = 0)

    if (subAddr > 0) {
        header[0] |= 0x40;  // Sub-address present
        header[1] = subAddr & 0x7F;
        headerLen = 2;
        if (subAddr > 127) {
            header[1] |= 0x80;
            header[2] = (subAddr >> 7) & 0xFF;
            headerLen = 3;
        }
    }

    // Slow SPI for testing
    SPI.beginTransaction(SPISettings(2000000L, MSBFIRST, SPI_MODE0));

    digitalWrite(PIN_SS, LOW);
    delayMicroseconds(5);  // CS setup time

    for (int i = 0; i < headerLen; i++) {
        SPI.transfer(header[i]);
    }

    for (int i = 0; i < len; i++) {
        data[i] = SPI.transfer(0x00);
    }

    delayMicroseconds(5);  // CS hold time
    digitalWrite(PIN_SS, HIGH);

    SPI.endTransaction();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== SPI Diagnostic Test ==="));

    // Manual SPI init
    pinMode(PIN_SS, OUTPUT);
    digitalWrite(PIN_SS, HIGH);
    pinMode(PIN_RST, OUTPUT);

    // Hard reset
    Serial.println(F("Hard reset..."));
    digitalWrite(PIN_RST, LOW);
    delay(10);
    digitalWrite(PIN_RST, HIGH);
    delay(10);

    SPI.begin();

    Serial.println(F("Testing register reads...\n"));

    // Test 1: Read DEV_ID (should always be 0xDECA0130)
    Serial.println(F("=== Test 1: DEV_ID Register ==="));
    for (int i = 0; i < 10; i++) {
        uint8_t devId[4];
        readRegisterDirect(DEV_ID_REG, 0, devId, 4);

        totalReads++;
        bool valid = (devId[0] == EXPECTED_DEV_ID_LOW && devId[1] == EXPECTED_DEV_ID_HIGH);
        if (valid) goodReads++; else corruptReads++;

        Serial.print(F("  DEV_ID: "));
        for (int j = 0; j < 4; j++) {
            if (devId[j] < 16) Serial.print("0");
            Serial.print(devId[j], HEX);
            Serial.print(" ");
        }
        Serial.println(valid ? F("OK") : F("CORRUPT!"));
        delay(10);
    }

    // Now init DW1000 library (without IRQ)
    Serial.println(F("\n=== Initializing DW1000 Library ==="));
    DW1000.begin(0xFF, PIN_RST);  // No IRQ
    DW1000.select(PIN_SS);

    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device ID via library: "));
    Serial.println(msg);

    // Apply LDO tuning
    Serial.println(F("Applying LDO tuning..."));
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);
    Serial.print(F("  OTP LDO: 0x"));
    Serial.println(ldoTune[0], HEX);

    if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        aonCtrl[0] |= 0x40;  // Set OTP_LDO bit
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        delay(1);
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        Serial.println(F("  LDO applied"));
    }

    // Test 2: Read status in IDLE mode
    Serial.println(F("\n=== Test 2: SYS_STATUS in IDLE mode ==="));
    for (int i = 0; i < 10; i++) {
        uint8_t status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

        totalReads++;
        // In idle, we expect CPLOCK (bit 1) set, not all FFs
        bool valid = !(status[0] == 0xFF && status[1] == 0xFF);
        if (valid) goodReads++; else corruptReads++;

        Serial.print(F("  STATUS: "));
        for (int j = 0; j < 5; j++) {
            if (status[j] < 16) Serial.print("0");
            Serial.print(status[j], HEX);
            Serial.print(" ");
        }
        Serial.println(valid ? F("OK") : F("CORRUPT!"));
        delay(50);
    }

    // Configure for RX
    Serial.println(F("\n=== Configuring for RX ==="));
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_SHORTDATA_FAST_LOWPOWER);
    DW1000.setFrameFilter(false);
    DW1000.suppressFrameCheck(false);
    DW1000.commitConfiguration();

    // Re-apply LDO after config
    if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        aonCtrl[0] |= 0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        delay(1);
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
    }

    // Test 3: Read status after config but before RX
    Serial.println(F("\n=== Test 3: SYS_STATUS after config ==="));
    for (int i = 0; i < 10; i++) {
        uint8_t status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

        totalReads++;
        bool valid = !(status[0] == 0xFF && status[1] == 0xFF);
        if (valid) goodReads++; else corruptReads++;

        Serial.print(F("  STATUS: "));
        for (int j = 0; j < 5; j++) {
            if (status[j] < 16) Serial.print("0");
            Serial.print(status[j], HEX);
            Serial.print(" ");
        }
        Serial.println(valid ? F("OK") : F("CORRUPT!"));
        delay(50);
    }

    // Test 4: Start RX and read status
    Serial.println(F("\n=== Test 4: SYS_STATUS during RX ==="));
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.startReceive();

    Serial.println(F("  RX started, waiting 500ms..."));
    delay(500);

    for (int i = 0; i < 20; i++) {
        uint8_t status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

        totalReads++;
        bool valid = !(status[0] == 0xFF && status[1] == 0xFF && status[2] == 0xFF);
        if (valid) goodReads++; else corruptReads++;

        Serial.print(F("  STATUS: "));
        for (int j = 0; j < 5; j++) {
            if (status[j] < 16) Serial.print("0");
            Serial.print(status[j], HEX);
            Serial.print(" ");
        }
        Serial.println(valid ? F("OK") : F("CORRUPT!"));
        delay(100);  // Slower polling
    }

    DW1000.idle();

    // Test 5: Read status after returning to IDLE
    Serial.println(F("\n=== Test 5: SYS_STATUS back in IDLE ==="));
    delay(100);
    for (int i = 0; i < 10; i++) {
        uint8_t status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

        totalReads++;
        bool valid = !(status[0] == 0xFF && status[1] == 0xFF);
        if (valid) goodReads++; else corruptReads++;

        Serial.print(F("  STATUS: "));
        for (int j = 0; j < 5; j++) {
            if (status[j] < 16) Serial.print("0");
            Serial.print(status[j], HEX);
            Serial.print(" ");
        }
        Serial.println(valid ? F("OK") : F("CORRUPT!"));
        delay(50);
    }

    // Summary
    Serial.println(F("\n========== SUMMARY =========="));
    Serial.print(F("Total reads: "));
    Serial.println(totalReads);
    Serial.print(F("Good reads:  "));
    Serial.println(goodReads);
    Serial.print(F("Corrupt:     "));
    Serial.println(corruptReads);
    Serial.print(F("Success rate: "));
    Serial.print((goodReads * 100) / totalReads);
    Serial.println(F("%"));

    if (corruptReads > 0) {
        Serial.println(F("\n*** SPI CORRUPTION DETECTED ***"));
        Serial.println(F("Check: wiring, power supply, SPI speed"));
    } else {
        Serial.println(F("\nSPI communication appears stable."));
    }

    Serial.println(F("\nTest complete. Entering idle loop."));
}

void loop() {
    // Periodic status check
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 5000) {
        lastCheck = millis();

        uint8_t status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

        Serial.print(F("[Idle check] STATUS: "));
        for (int j = 0; j < 5; j++) {
            if (status[j] < 16) Serial.print("0");
            Serial.print(status[j], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    delay(100);
}
