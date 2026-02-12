/**
 * RX Test - Polling Based (v3)
 *
 * Avoids IRQ handler entirely since SPI is unreliable during RX mode.
 * Instead, polls status register and processes frames in main loop
 * where we can idle the chip first (making SPI reliable) before reading data.
 *
 * Upload to: /dev/ttyACM1 while TX runs on /dev/ttyACM0
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Register addresses
#define SYS_STATUS_REG  0x0F
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Stats
uint32_t rxGood = 0;
uint32_t rxFailed = 0;
uint32_t pollCount = 0;
uint32_t corruptCount = 0;

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);

    Serial.print(F("  OTP LDO: 0x"));
    Serial.println(ldoTune[0], HEX);

    if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        aonCtrl[0] |= 0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        delay(1);
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        Serial.println(F("  LDO APPLIED"));
    }
}

void startReceiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(false);  // We restart manually
    DW1000.startReceive();
}

// Read status with retry to filter SPI glitches
uint32_t readStatusReliable() {
    byte s1[5], s2[5];

    DW1000.readBytes(SYS_STATUS_REG, 0x00, s1, 5);
    DW1000.readBytes(SYS_STATUS_REG, 0x00, s2, 5);

    // If both reads match, trust the result
    if (s1[0] == s2[0] && s1[1] == s2[1] && s1[2] == s2[2] && s1[3] == s2[3]) {
        return (uint32_t)s1[0] | ((uint32_t)s1[1] << 8) |
               ((uint32_t)s1[2] << 16) | ((uint32_t)s1[3] << 24);
    }

    // Reads disagree - do a third read as tiebreaker
    byte s3[5];
    DW1000.readBytes(SYS_STATUS_REG, 0x00, s3, 5);
    corruptCount++;

    // Use the value that appeared at least twice
    if (s1[0] == s3[0] && s1[1] == s3[1]) {
        return (uint32_t)s1[0] | ((uint32_t)s1[1] << 8) |
               ((uint32_t)s1[2] << 16) | ((uint32_t)s1[3] << 24);
    }
    return (uint32_t)s2[0] | ((uint32_t)s2[1] << 8) |
           ((uint32_t)s2[2] << 16) | ((uint32_t)s2[3] << 24);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Polling Test v3 ==="));

    // Init WITHOUT IRQ to avoid ISR SPI conflicts
    DW1000.begin(0xFF, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    // Configure - match TX settings
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    applyLDOTuning();

    DW1000.getPrintableDeviceMode(msg);
    Serial.print(F("Mode: "));
    Serial.println(msg);

    // Clear all status
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delay(10);

    // Start receiver
    startReceiver();

    // Verify we're in RX mode
    uint32_t status = readStatusReliable();
    Serial.print(F("Status after RX start: 0x"));
    Serial.println(status, HEX);
    Serial.println(F("Polling for frames...\n"));
}

void loop() {
    pollCount++;

    // Read status register (with retry)
    uint32_t status = readStatusReliable();

    // Skip obviously corrupt reads
    if (status == 0xFFFFFFFF) {
        delay(10);
        return;
    }

    // Check for received frame
    // RXDFR (bit 13) = data frame received
    // RXFCG (bit 14) = frame check good (CRC passed)
    bool rxDone = status & (1UL << 13);  // RXDFR
    bool rxGoodCRC = status & (1UL << 14);  // RXFCG

    // Check for receive errors
    bool rxCRCErr = status & (1UL << 16);   // RXFCE
    bool rxHeaderErr = status & (1UL << 12);  // RXPHE
    bool rxOverrun = status & (1UL << 20);  // RXOVRR
    bool ldeErr = status & (1UL << 19);     // LDEERR

    if (rxGoodCRC) {
        // Frame received with good CRC!
        // Read data BEFORE going idle (idle clears RX buffer)
        rxGood++;

        byte data[128];
        int dataLen = DW1000.getDataLength();
        if (dataLen > 127) dataLen = 127;
        if (dataLen > 0) {
            DW1000.getData(data, dataLen);
        }

        // NOW go idle for reliable SPI
        DW1000.idle();
        delay(1);

        Serial.print(F("RX #"));
        Serial.print(rxGood);
        Serial.print(F(" len="));
        Serial.print(dataLen);
        Serial.print(F(" \""));
        for (int i = 0; i < dataLen && i < 32; i++) {
            if (data[i] >= 32 && data[i] < 127) {
                Serial.print((char)data[i]);
            } else {
                Serial.print('.');
            }
        }
        Serial.print(F("\" ("));
        Serial.print(rxGood);
        Serial.println(F(") OK"));

        // Clear status and restart receiver
        byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
        startReceiver();

    } else if (rxDone && !rxGoodCRC) {
        // Frame received but CRC failed
        DW1000.idle();
        rxFailed++;
        Serial.print(F("[CRC fail #"));
        Serial.print(rxFailed);
        Serial.print(F(" S:0x"));
        Serial.print(status, HEX);
        Serial.println(F("]"));

        byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
        startReceiver();

    } else if (rxCRCErr || rxHeaderErr || rxOverrun || ldeErr) {
        // Receive error
        DW1000.idle();
        rxFailed++;

        byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
        startReceiver();
    }

    // Periodic status report and watchdog every 5 seconds
    static uint32_t lastReport = 0;
    static uint32_t lastRxTime = 0;
    if (millis() - lastReport >= 5000) {
        lastReport = millis();

        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] G:"));
        Serial.print(rxGood);
        Serial.print(F(" F:"));
        Serial.print(rxFailed);
        Serial.print(F(" C:"));
        Serial.print(corruptCount);
        Serial.print(F(" S:0x"));
        Serial.println(status, HEX);

        // Watchdog: if status is 0 (stuck) or no activity for 10s, restart
        static uint32_t prevGood = 0;
        if (status == 0 || (millis() - lastRxTime > 10000 && millis() > 15000)) {
            Serial.println(F("  >> Restarting RX"));
            DW1000.idle();
            delay(5);
            byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
            delay(5);
            startReceiver();
        }
        if (rxGood > prevGood) {
            lastRxTime = millis();
            prevGood = rxGood;
        }
    }

    delay(5);  // 5ms polling interval
}
