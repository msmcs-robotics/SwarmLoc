/**
 * RX Test v8f - Frame Check Suppressed (No CRC) + Raw Reads
 *
 * With frame check suppressed:
 *   - TX doesn't append CRC (frame = just data bytes)
 *   - RX doesn't check CRC (RXFCE never set)
 *   - RXFCG set when frame received regardless of CRC
 *   - Should see more "good" frames if CRC was the only barrier
 *
 * Also reads XTAL trim and key registers for diagnostic info.
 * PIN_RST = 7, 110kbps mode
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 7;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

#define SYS_STATUS_REG  0x0F
#define SYS_CFG_REG     0x04
#define RX_FINFO_REG    0x10
#define RX_BUFFER_REG   0x11
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02
#define FS_CTRL_REG     0x2B
#define FS_XTALT_SUB    0x0E

uint32_t rxGood = 0;
uint32_t rxCRC = 0;
uint32_t rxHDR = 0;
uint32_t cycles = 0;
uint32_t deadCycles = 0;
uint32_t watchdogResets = 0;

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

uint32_t readStatus() {
    byte status[5];
    DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);
    return (uint32_t)status[0] | ((uint32_t)status[1] << 8) |
           ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);
}

uint16_t readRawFrameLength() {
    byte rxInfo[4];
    DW1000.readBytes(RX_FINFO_REG, 0x00, rxInfo, 4);
    return ((uint16_t)rxInfo[1] << 8 | (uint16_t)rxInfo[0]) & 0x03FF;
}

void readRawData(byte* buf, uint16_t len) {
    if (len > 127) len = 127;
    DW1000.readBytes(RX_BUFFER_REG, 0x00, buf, len);
}

bool forceIdleVerified() {
    for (int attempt = 0; attempt < 3; attempt++) {
        DW1000.idle();
        delay(1);
        uint32_t s1 = readStatus();
        delayMicroseconds(200);
        uint32_t s2 = readStatus();
        if (s1 == s2 && s1 != 0xFFFFFFFF) return true;
        if (attempt > 0) delay(2);
    }
    return false;
}

void hardwareReset() {
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(2);
    pinMode(PIN_RST, INPUT);
    delay(10);
}

void fullInit() {
    hardwareReset();
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);
    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setReceiverAutoReenable(false);
    DW1000.suppressFrameCheck(true);
    DW1000.commitConfiguration();
    applyLDOTuning();

    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX v8f - No CRC ==="));

    fullInit();

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    // Print XTAL trim
    byte xtalt[1];
    DW1000.readBytes(FS_CTRL_REG, FS_XTALT_SUB, xtalt, 1);
    Serial.print(F("XTALT: 0x"));
    Serial.print(xtalt[0], HEX);
    Serial.print(F(" trim="));
    Serial.println(xtalt[0] & 0x1F);

    // Print XTAL trim from OTP
    byte otpXtal[4];
    DW1000.readBytesOTP(0x01E, otpXtal);
    Serial.print(F("OTP XTAL: 0x"));
    Serial.println(otpXtal[0], HEX);

    byte sysCfg[4];
    DW1000.readBytes(SYS_CFG_REG, 0x00, sysCfg, 4);
    uint32_t cfg = (uint32_t)sysCfg[0] | ((uint32_t)sysCfg[1] << 8) |
                   ((uint32_t)sysCfg[2] << 16) | ((uint32_t)sysCfg[3] << 24);
    Serial.print(F("SYS_CFG: 0x"));
    Serial.println(cfg, HEX);

    DW1000.getPrintableDeviceMode(msg);
    Serial.print(F("Mode: "));
    Serial.println(msg);

    Serial.println(F("Starting RX...\n"));
}

void loop() {
    static uint32_t lastReport = 0;
    cycles++;

    if (deadCycles >= 50) {
        watchdogResets++;
        Serial.print(F("[WD #"));
        Serial.print(watchdogResets);
        Serial.println(F("]"));
        fullInit();
        deadCycles = 0;
        return;
    }

    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delayMicroseconds(50);

    DW1000.newReceive();
    DW1000.receivePermanently(false);
    DW1000.startReceive();

    delay(200);

    if (!forceIdleVerified()) {
        deadCycles++;
        DW1000.idle();
        delay(5);
        return;
    }

    uint32_t s = readStatus();

    if (s == 0xFFFFFFFF || s == 0x00000000) {
        deadCycles++;
        DW1000.idle();
        return;
    }

    // Corruption guard
    uint8_t bitCount = 0;
    uint32_t tmp = s;
    while (tmp) { bitCount += tmp & 1; tmp >>= 1; }
    if (bitCount > 20) { deadCycles++; DW1000.idle(); return; }

    // With frame check suppressed, RXFCG means frame received (no CRC check)
    bool preamble  = s & (1UL << 8);
    bool headerErr = s & (1UL << 12);
    bool frameRx   = s & (1UL << 13);
    bool frameGood = s & (1UL << 14);
    bool crcErr    = s & (1UL << 15);

    if (frameGood && frameRx) {
        rxGood++;
        deadCycles = 0;
        uint16_t rawLen = readRawFrameLength();

        Serial.print(F("RX #"));
        Serial.print(rxGood);
        Serial.print(F(" raw="));
        Serial.print(rawLen);

        if (rawLen > 0 && rawLen < 128) {
            byte data[128];
            readRawData(data, rawLen);
            Serial.print(F(" \""));
            for (uint16_t i = 0; i < rawLen && i < 32; i++) {
                if (data[i] >= 32 && data[i] < 127) Serial.print((char)data[i]);
                else Serial.print('.');
            }
            Serial.print(F("\""));
        } else {
            byte peek[16];
            readRawData(peek, 16);
            Serial.print(F(" pk["));
            for (int i = 0; i < 12; i++) {
                if (i > 0) Serial.print(' ');
                if (peek[i] < 0x10) Serial.print('0');
                Serial.print(peek[i], HEX);
            }
            Serial.print(F("]"));
        }

        Serial.print(F(" S:0x"));
        Serial.println(s, HEX);

    } else if (frameRx) {
        // Frame received but not "good" — might have CRC error
        rxCRC++;
        deadCycles = 0;
        uint16_t rawLen = readRawFrameLength();
        byte peek[16];
        readRawData(peek, 16);

        Serial.print(F("[FRM #"));
        Serial.print(rxCRC);
        Serial.print(F(" raw="));
        Serial.print(rawLen);
        Serial.print(F(" pk["));
        for (int i = 0; i < 12; i++) {
            if (i > 0) Serial.print(' ');
            if (peek[i] < 0x10) Serial.print('0');
            Serial.print(peek[i], HEX);
        }
        Serial.print(F("] S:0x"));
        Serial.print(s, HEX);
        Serial.println(F("]"));

    } else if (headerErr || preamble) {
        rxHDR++;
        deadCycles = 0;
        uint16_t rawLen = readRawFrameLength();
        Serial.print(F("[HDR #"));
        Serial.print(rxHDR);
        Serial.print(F(" fl="));
        Serial.print(rawLen);
        Serial.print(F(" S:0x"));
        Serial.print(s, HEX);
        Serial.println(F("]"));
    } else {
        deadCycles++;
    }

    DW1000.idle();

    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] G:"));
        Serial.print(rxGood);
        Serial.print(F(" F:"));
        Serial.print(rxCRC);
        Serial.print(F(" HDR:"));
        Serial.print(rxHDR);
        Serial.print(F(" wd:"));
        Serial.println(watchdogResets);
    }
}
