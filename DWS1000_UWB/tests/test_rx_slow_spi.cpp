/**
 * RX Test with Slow SPI
 *
 * Forces 2MHz SPI for all reads to test if SPI speed is the issue.
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Slow SPI settings (2MHz)
SPISettings slowSPI(2000000L, MSBFIRST, SPI_MODE0);

// Register addresses
#define SYS_STATUS_REG  0x0F
#define RX_FINFO_REG    0x10
#define RX_BUFFER_REG   0x11
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Stats
uint32_t rxEvents = 0;
uint32_t rxGood = 0;
uint32_t rxBad = 0;

// Direct slow SPI read
void readBytesSlow(byte reg, uint16_t offset, byte* data, uint16_t len) {
    byte header[3];
    uint8_t headerLen = 1;

    if (offset == 0) {
        header[0] = reg & 0x3F;  // Read mode
    } else {
        header[0] = 0x40 | (reg & 0x3F);  // Read with sub-address
        if (offset < 128) {
            header[1] = offset & 0x7F;
            headerLen = 2;
        } else {
            header[1] = 0x80 | (offset & 0x7F);
            header[2] = (offset >> 7) & 0xFF;
            headerLen = 3;
        }
    }

    SPI.beginTransaction(slowSPI);
    digitalWrite(PIN_SS, LOW);
    delayMicroseconds(5);

    for (int i = 0; i < headerLen; i++) {
        SPI.transfer(header[i]);
    }
    for (int i = 0; i < len; i++) {
        data[i] = SPI.transfer(0x00);
    }

    delayMicroseconds(5);
    digitalWrite(PIN_SS, HIGH);
    SPI.endTransaction();
}

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);
    Serial.print(F("LDO: 0x"));
    Serial.println(ldoTune[0], HEX);

    if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        aonCtrl[0] |= 0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        delay(1);
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
    }
}

void printHex(byte* data, int len) {
    for (int i = 0; i < len; i++) {
        if (data[i] < 16) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Test with Slow SPI ==="));

    // Init without IRQ
    DW1000.begin(0xFF, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    // Configure
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setFrameFilter(false);
    DW1000.suppressFrameCheck(false);
    DW1000.commitConfiguration();

    applyLDOTuning();

    DW1000.getPrintableDeviceMode(msg);
    Serial.print(F("Mode: "));
    Serial.println(msg);

    // Clear status
    byte clear[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0, clear, 5);

    // Start receive
    DW1000.newReceive();
    DW1000.receivePermanently(true);
    DW1000.startReceive();

    Serial.println(F("RX started with slow SPI polling"));
    Serial.println(F("Polling every 50ms..."));
    Serial.println();
}

void loop() {
    static uint32_t lastPoll = 0;
    static uint32_t lastStats = 0;

    // Poll status using SLOW SPI (every 50ms)
    if (millis() - lastPoll >= 50) {
        lastPoll = millis();

        byte status[5];
        readBytesSlow(SYS_STATUS_REG, 0, status, 5);

        // Check for obvious corruption
        if (status[0] == 0xFF && status[1] == 0xFF &&
            status[2] == 0xFF && status[3] == 0xFF) {
            // Skip corrupt read
            return;
        }

        uint32_t s = status[0] | ((uint32_t)status[1] << 8) |
                     ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);

        // Check for RX events
        bool rxdfr = s & (1UL << 13);
        bool rxfcg = s & (1UL << 14);
        bool rxfce = s & (1UL << 15);

        if (rxdfr || rxfcg || rxfce) {
            rxEvents++;

            Serial.print(F("[RX] S=0x"));
            Serial.print(s, HEX);

            if (rxfcg) {
                // Read frame info with slow SPI
                byte finfo[4];
                readBytesSlow(RX_FINFO_REG, 0, finfo, 4);
                uint16_t len = ((finfo[1] << 8) | finfo[0]) & 0x3FF;

                Serial.print(F(" GOOD len="));
                Serial.print(len);

                if (len > 0 && len < 128) {
                    rxGood++;
                    byte data[128];
                    readBytesSlow(RX_BUFFER_REG, 0, data, len);
                    Serial.print(F(" \""));
                    for (int i = 0; i < min((int)len, 20); i++) {
                        char c = data[i];
                        Serial.write((c >= 32 && c < 127) ? c : '.');
                    }
                    Serial.print(F("\""));
                } else {
                    rxBad++;
                }
            } else {
                rxBad++;
                Serial.print(F(" BAD"));
            }
            Serial.println();

            // Clear and restart
            byte clear[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            DW1000.writeBytes(SYS_STATUS_REG, 0, clear, 5);
            DW1000.newReceive();
            DW1000.receivePermanently(true);
            DW1000.startReceive();
        }
    }

    // Stats every 5s
    if (millis() - lastStats >= 5000) {
        lastStats = millis();

        Serial.print(F("[Stats] Events:"));
        Serial.print(rxEvents);
        Serial.print(F(" Good:"));
        Serial.print(rxGood);
        Serial.print(F(" Bad:"));
        Serial.println(rxBad);
    }
}
