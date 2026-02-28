/**
 * RX Test v8d - Raw Register Reads (Bypass Library Device Mode)
 *
 * KEY FIX: Library's getDataLength()/getData() return 0 in IDLE_MODE
 * because they check _deviceMode == RX_MODE. Since we force IDLE before
 * reading (for SPI reliability), we must read RX_FINFO and RX_BUFFER
 * directly via raw SPI reads.
 *
 * PIN_RST = 7 (correct for DWS1000 shield)
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

// Read RX frame length directly from RX_FINFO register (bypasses _deviceMode check)
uint16_t readRawFrameLength() {
    byte rxInfo[4];
    DW1000.readBytes(RX_FINFO_REG, 0x00, rxInfo, 4);
    uint16_t len = ((uint16_t)rxInfo[1] << 8 | (uint16_t)rxInfo[0]) & 0x03FF;
    return len;
}

// Read RX buffer directly (bypasses _deviceMode check)
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
    DW1000.commitConfiguration();
    applyLDOTuning();

    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
}

void printFrame(uint16_t rawLen, uint32_t status) {
    // Subtract 2 for CRC if frame check enabled
    uint16_t dataLen = (rawLen > 2) ? rawLen - 2 : 0;

    Serial.print(F(" raw="));
    Serial.print(rawLen);
    Serial.print(F(" data="));
    Serial.print(dataLen);

    if (dataLen > 0) {
        byte data[128];
        uint16_t readLen = (dataLen > 127) ? 127 : dataLen;
        readRawData(data, readLen);

        Serial.print(F(" \""));
        for (uint16_t i = 0; i < readLen && i < 32; i++) {
            if (data[i] >= 32 && data[i] < 127) Serial.print((char)data[i]);
            else Serial.print('.');
        }
        Serial.print(F("\""));
    } else {
        // Even if len=0, try reading a few bytes to see what's there
        byte peek[16];
        readRawData(peek, 16);
        Serial.print(F(" peek["));
        for (int i = 0; i < 16; i++) {
            if (i > 0) Serial.print(' ');
            if (peek[i] < 0x10) Serial.print('0');
            Serial.print(peek[i], HEX);
        }
        Serial.print(F("]"));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX v8d - Raw Register Reads ==="));

    fullInit();

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    byte sysCfg[4];
    DW1000.readBytes(SYS_CFG_REG, 0x00, sysCfg, 4);
    uint32_t cfg = (uint32_t)sysCfg[0] | ((uint32_t)sysCfg[1] << 8) |
                   ((uint32_t)sysCfg[2] << 16) | ((uint32_t)sysCfg[3] << 24);
    Serial.print(F("SYS_CFG: 0x"));
    Serial.print(cfg, HEX);
    Serial.print(F(" RXAUTR="));
    Serial.println((cfg & (1UL << 29)) ? "ON" : "OFF");

    DW1000.getPrintableDeviceMode(msg);
    Serial.print(F("Mode: "));
    Serial.println(msg);

    Serial.println(F("Starting RX...\n"));
}

void loop() {
    static uint32_t lastReport = 0;
    cycles++;

    // Watchdog: hardware reset after 50 dead cycles
    if (deadCycles >= 50) {
        watchdogResets++;
        Serial.print(F("[WD #"));
        Serial.print(watchdogResets);
        Serial.println(F("]"));
        fullInit();
        deadCycles = 0;
        return;
    }

    // Clear all status
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delayMicroseconds(50);

    // Start single-shot receive
    DW1000.newReceive();
    DW1000.receivePermanently(false);
    DW1000.startReceive();

    // Wait for frame
    delay(200);

    // Force IDLE with verification
    if (!forceIdleVerified()) {
        deadCycles++;
        DW1000.idle();
        delay(5);
        return;
    }

    uint32_t s = readStatus();

    // Guard against SPI corruption
    if (s == 0xFFFFFFFF || s == 0x00000000) {
        deadCycles++;
        DW1000.idle();
        return;
    }

    bool preamble  = s & (1UL << 8);
    bool headerErr = s & (1UL << 12);
    bool frameRx   = s & (1UL << 13);
    bool frameGood = s & (1UL << 14);
    bool crcErr    = s & (1UL << 15);

    // Also check for near-miss corruption (too many bits set)
    uint8_t bitCount = 0;
    uint32_t tmp = s;
    while (tmp) { bitCount += tmp & 1; tmp >>= 1; }
    if (bitCount > 20) {
        // Likely SPI corruption — too many bits set
        deadCycles++;
        DW1000.idle();
        return;
    }

    if (frameGood && frameRx) {
        rxGood++;
        deadCycles = 0;
        uint16_t rawLen = readRawFrameLength();

        Serial.print(F("RX #"));
        Serial.print(rxGood);
        printFrame(rawLen, s);
        Serial.print(F(" S:0x"));
        Serial.println(s, HEX);

    } else if (crcErr && frameRx) {
        rxCRC++;
        deadCycles = 0;
        uint16_t rawLen = readRawFrameLength();

        Serial.print(F("[CRC #"));
        Serial.print(rxCRC);
        printFrame(rawLen, s);
        Serial.print(F(" S:0x"));
        Serial.print(s, HEX);
        Serial.println(F("]"));

    } else if (headerErr || preamble) {
        rxHDR++;
        deadCycles = 0;

        // On header error, also peek at RX_FINFO to see what was decoded
        uint16_t rawLen = readRawFrameLength();
        Serial.print(F("[HDR #"));
        Serial.print(rxHDR);
        Serial.print(F(" finfo_len="));
        Serial.print(rawLen);
        Serial.print(F(" S:0x"));
        Serial.print(s, HEX);
        Serial.println(F("]"));

    } else {
        deadCycles++;
    }

    DW1000.idle();

    // Periodic report
    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] G:"));
        Serial.print(rxGood);
        Serial.print(F(" CRC:"));
        Serial.print(rxCRC);
        Serial.print(F(" HDR:"));
        Serial.print(rxHDR);
        Serial.print(F(" wd:"));
        Serial.println(watchdogResets);
    }
}
