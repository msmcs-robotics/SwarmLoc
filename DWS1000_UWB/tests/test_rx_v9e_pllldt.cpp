/**
 * RX Test v9e - PLLLDT Fix (Official Driver Init Steps)
 *
 * Adds initialization steps from the official Decawave driver (decadriver)
 * that are MISSING from the Arduino library:
 *
 * 1. PLLLDT (PLL Lock Detect Tune) - EXT_SYNC:EC_CTRL bit 2
 *    The official driver sets this during init. Without it, PLL lock
 *    detection may be unreliable, potentially causing CLKPLL_LL errors.
 *
 * 2. AON_CFG1 clear - AON:0x0A = 0x00
 *    Official driver clears this to ensure proper operation.
 *
 * Also checks CPLOCK (bit 1 of SYS_STATUS) after init to verify PLL locked.
 *
 * 110kbps, 16MHz PRF, Ch5, PIN_RST = 7
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
#define AON_CFG1_SUB    0x0A
#define EXT_SYNC_REG    0x24
#define EC_CTRL_SUB     0x00  // EC_CTRL is at offset 0x00 within EXT_SYNC (not 0x04!)

uint32_t rxGood = 0;
uint32_t rxCRC = 0;
uint32_t rxPHE = 0;
uint32_t rxRFSL = 0;
uint32_t rxPreamOnly = 0;
uint32_t clkPllCnt = 0;
uint32_t rfPllCnt = 0;
uint32_t spiCorrupt = 0;
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

void applyDecadriverInit() {
    // Step 1: Set PLLLDT (PLL Lock Detect Tune) - from official Decawave driver
    // EXT_SYNC register (0x24), EC_CTRL sub-register at offset 0x00, bit 2 = PLLLDT
    // Note: Previous attempt used offset 0x04 (wrong!) - lab11 driver confirms offset 0x00
    byte ecCtrl[4];
    DW1000.readBytes(EXT_SYNC_REG, EC_CTRL_SUB, ecCtrl, 4);
    Serial.print(F("EC_CTRL before: 0x"));
    Serial.println(ecCtrl[0], HEX);
    ecCtrl[0] |= 0x04;  // Set bit 2 = PLLLDT (read-modify-write to preserve other bits)
    DW1000.writeBytes(EXT_SYNC_REG, EC_CTRL_SUB, ecCtrl, 4);

    // Step 2: Clear AON_CFG1 - from official Decawave driver
    // "The 3 bits in AON CFG1 register must be cleared"
    byte aonCfg1[1] = {0x00};
    DW1000.writeBytes(AON_REG, AON_CFG1_SUB, aonCfg1, 1);

    // Verify PLLLDT was written
    byte ecCtrlRead[1];
    DW1000.readBytes(EXT_SYNC_REG, EC_CTRL_SUB, ecCtrlRead, 1);
    Serial.print(F("EC_CTRL: 0x"));
    Serial.print(ecCtrlRead[0], HEX);
    Serial.print(F(" PLLLDT="));
    Serial.println((ecCtrlRead[0] & 0x04) ? "ON" : "OFF");
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
    DW1000.commitConfiguration();
    applyLDOTuning();

    // Apply official Decawave driver initialization steps
    applyDecadriverInit();

    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    // Clear all status bits
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delay(5);

    // Check CPLOCK after clear
    uint32_t s = readStatus();
    Serial.print(F("Post-init status: 0x"));
    Serial.print(s, HEX);
    Serial.print(F(" CPLOCK="));
    Serial.print((s & (1UL << 1)) ? "YES" : "NO");
    Serial.print(F(" CLKPLL_LL="));
    Serial.println((s & (1UL << 25)) ? "YES" : "NO");
}

void printStatusDecode(uint32_t s) {
    if (s & (1UL << 8))  Serial.print(F("PRD "));
    if (s & (1UL << 9))  Serial.print(F("SFD "));
    if (s & (1UL << 10)) Serial.print(F("LDE "));
    if (s & (1UL << 11)) Serial.print(F("PHD "));
    if (s & (1UL << 12)) Serial.print(F("PHE! "));
    if (s & (1UL << 13)) Serial.print(F("DFR "));
    if (s & (1UL << 14)) Serial.print(F("FCG "));
    if (s & (1UL << 15)) Serial.print(F("FCE! "));
    if (s & (1UL << 16)) Serial.print(F("RFSL! "));
    if (s & (1UL << 18)) Serial.print(F("LDERR! "));
    if (s & (1UL << 24)) Serial.print(F("rfPLL! "));
    if (s & (1UL << 25)) Serial.print(F("clkPLL! "));
}

bool isStatusContradictory(uint32_t s) {
    bool fcg = s & (1UL << 14);
    bool fce = s & (1UL << 15);
    bool rfsl = s & (1UL << 16);
    bool sfdto = s & (1UL << 26);
    bool prd = s & (1UL << 8);
    bool sfd = s & (1UL << 9);
    bool phe = s & (1UL << 12);
    if (fcg && fce) return true;
    if (fcg && rfsl) return true;
    if (fcg && sfdto) return true;
    if (fcg && phe) return true;
    if (fcg && !prd && !sfd) return true;
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX v9e - PLLLDT Fix ==="));

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

    uint8_t bitCount = 0;
    uint32_t tmp = s;
    while (tmp) { bitCount += tmp & 1; tmp >>= 1; }
    if (bitCount > 20) { deadCycles++; spiCorrupt++; DW1000.idle(); return; }

    if (isStatusContradictory(s)) {
        spiCorrupt++;
        deadCycles++;
        DW1000.idle();
        return;
    }

    bool preamble   = s & (1UL << 8);
    bool sfdDet     = s & (1UL << 9);
    bool phrDet     = s & (1UL << 11);
    bool headerErr  = s & (1UL << 12);
    bool frameRx    = s & (1UL << 13);
    bool frameGood  = s & (1UL << 14);
    bool crcErr     = s & (1UL << 15);
    bool rfSyncLoss = s & (1UL << 16);
    bool clkPllLL   = s & (1UL << 25);
    bool rfPllLL    = s & (1UL << 24);

    if (clkPllLL) clkPllCnt++;
    if (rfPllLL) rfPllCnt++;

    if (frameGood && frameRx) {
        rxGood++;
        deadCycles = 0;
        uint16_t rawLen = readRawFrameLength();

        Serial.print(F("*** RX #"));
        Serial.print(rxGood);
        Serial.print(F(" len="));
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
            for (int i = 0; i < 8; i++) {
                if (i > 0) Serial.print(' ');
                if (peek[i] < 0x10) Serial.print('0');
                Serial.print(peek[i], HEX);
            }
            Serial.print(F("]"));
        }

        Serial.print(F(" ["));
        printStatusDecode(s);
        Serial.println(F("] ***"));

    } else if (frameRx && crcErr) {
        rxCRC++;
        deadCycles = 0;
        uint16_t rawLen = readRawFrameLength();
        byte peek[16];
        readRawData(peek, 16);

        Serial.print(F("[CRC #"));
        Serial.print(rxCRC);
        Serial.print(F(" len="));
        Serial.print(rawLen);
        Serial.print(F(" pk["));
        for (int i = 0; i < 12; i++) {
            if (i > 0) Serial.print(' ');
            if (peek[i] < 0x10) Serial.print('0');
            Serial.print(peek[i], HEX);
        }
        Serial.print(F("] "));
        printStatusDecode(s);
        Serial.println(F("]"));

    } else if (headerErr) {
        rxPHE++;
        deadCycles = 0;
        uint16_t rawLen = readRawFrameLength();
        Serial.print(F("[PHE #"));
        Serial.print(rxPHE);
        Serial.print(F(" fl="));
        Serial.print(rawLen);
        Serial.print(F(" "));
        printStatusDecode(s);
        Serial.println(F("]"));

    } else if (rfSyncLoss && (preamble || sfdDet || phrDet)) {
        rxRFSL++;
        deadCycles = 0;
        uint16_t rawLen = readRawFrameLength();
        Serial.print(F("[RFSL #"));
        Serial.print(rxRFSL);
        Serial.print(F(" fl="));
        Serial.print(rawLen);
        Serial.print(F(" "));
        printStatusDecode(s);
        Serial.println(F("]"));

    } else if (preamble || sfdDet) {
        rxPreamOnly++;
        deadCycles = 0;
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
        Serial.print(F(" CRC:"));
        Serial.print(rxCRC);
        Serial.print(F(" PHE:"));
        Serial.print(rxPHE);
        Serial.print(F(" RFSL:"));
        Serial.print(rxRFSL);
        Serial.print(F(" spi:"));
        Serial.print(spiCorrupt);
        Serial.print(F(" clk:"));
        Serial.print(clkPllCnt);
        Serial.print(F(" rf:"));
        Serial.print(rfPllCnt);
        Serial.print(F(" wd:"));
        Serial.println(watchdogResets);
    }
}
