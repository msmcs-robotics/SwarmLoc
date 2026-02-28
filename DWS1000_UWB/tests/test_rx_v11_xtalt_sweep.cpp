/**
 * RX Test v11 - XTAL Trim Sweep
 *
 * Sweeps through crystal trim values (0-31) to find optimal PLL stability.
 * At each trim value, runs N RX cycles and counts CLKPLL_LL events.
 *
 * The XTAL trim adjusts the 38.4MHz crystal frequency which drives the PLL.
 * A better-tuned crystal means less PLL correction needed, potentially
 * reducing CLKPLL_LL events even with noisy power supply.
 *
 * Current OTP has no factory trim (defaults to 16/midrange).
 * Upper bits of FS_XTALT byte (bits 7:5) must be 0x60 for proper cap bank.
 *
 * After sweep, runs continuous RX with the best trim value found.
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
#define EC_CTRL_SUB     0x00
#define FS_CTRL_REG     0x2B
#define FS_XTALT_SUB    0x0E

uint32_t rxGood = 0;
uint32_t rxCRC = 0;
uint32_t rxPHE = 0;
uint32_t rxRFSL = 0;
uint32_t clkPllCnt = 0;
uint32_t spiCorrupt = 0;
uint32_t cycles = 0;
uint32_t deadCycles = 0;
uint32_t watchdogResets = 0;

// Sweep results
uint8_t bestTrim = 16;
uint16_t bestPllRate = 0xFFFF;  // lower is better
bool sweepDone = false;

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);
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
    byte ecCtrl[4];
    DW1000.readBytes(EXT_SYNC_REG, EC_CTRL_SUB, ecCtrl, 4);
    ecCtrl[0] |= 0x04;
    DW1000.writeBytes(EXT_SYNC_REG, EC_CTRL_SUB, ecCtrl, 4);
    byte aonCfg1[1] = {0x00};
    DW1000.writeBytes(AON_REG, AON_CFG1_SUB, aonCfg1, 1);
}

void setXtalTrim(uint8_t trim) {
    // Trim is 5 bits (0-31), upper bits must be 0x60 for proper cap bank
    byte val = (trim & 0x1F) | 0x60;
    DW1000.writeBytes(FS_CTRL_REG, FS_XTALT_SUB, &val, 1);
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

void fullInit(uint8_t trim) {
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
    applyDecadriverInit();

    // Apply specific XTAL trim
    setXtalTrim(trim);

    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delay(5);
}

// Run N cycles and return PLL error rate (clkPLL events per cycle * 100)
// Also returns rx activity counts via pointers
uint16_t measurePllRate(uint8_t trim, uint8_t numCycles,
                        uint8_t* outGood, uint8_t* outCRC, uint8_t* outPHE) {
    fullInit(trim);

    uint16_t pllCount = 0;
    uint8_t good = 0, crc = 0, phe = 0;
    uint8_t dead = 0;

    for (uint8_t i = 0; i < numCycles && dead < 20; i++) {
        byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
        delayMicroseconds(50);

        DW1000.newReceive();
        DW1000.receivePermanently(false);
        DW1000.startReceive();

        delay(200);

        DW1000.idle();
        delay(1);

        uint32_t s = readStatus();

        if (s == 0xFFFFFFFF || s == 0x00000000) {
            dead++;
            continue;
        }

        uint8_t bitCount = 0;
        uint32_t tmp = s;
        while (tmp) { bitCount += tmp & 1; tmp >>= 1; }
        if (bitCount > 20) { dead++; continue; }

        if (isStatusContradictory(s)) { dead++; continue; }

        if (s & (1UL << 25)) pllCount++;  // CLKPLL_LL

        bool frameGood = s & (1UL << 14);
        bool frameRx   = s & (1UL << 13);
        bool crcErr    = s & (1UL << 15);
        bool headerErr = s & (1UL << 12);
        bool preamble  = s & (1UL << 8);

        if (frameGood && frameRx) { good++; dead = 0; }
        else if (frameRx && crcErr) { crc++; dead = 0; }
        else if (headerErr) { phe++; dead = 0; }
        else if (preamble) dead = 0;
        else dead++;
    }

    *outGood = good;
    *outCRC = crc;
    *outPHE = phe;

    // Return PLL count (lower is better)
    return pllCount;
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

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX v11 - XTAL Trim Sweep ==="));

    // Quick init to read device info
    hardwareReset();
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    byte otpXtal[4];
    DW1000.readBytesOTP(0x01E, otpXtal);
    Serial.print(F("OTP XTAL: 0x"));
    Serial.print(otpXtal[0], HEX);
    Serial.print(F(" trim="));
    Serial.println(otpXtal[0] & 0x1F);

    Serial.println(F("\n--- XTAL Trim Sweep (20 cycles each) ---"));
    Serial.println(F("Trim | PLL_LL | Good | CRC | PHE"));
    Serial.println(F("-----|--------|------|-----|----"));

    // Sweep trim values: test key values across the range
    // Full range is 0-31. Test every 2 to cover range in reasonable time.
    for (uint8_t trim = 0; trim <= 31; trim += 2) {
        uint8_t good, crc, phe;
        uint16_t pllRate = measurePllRate(trim, 20, &good, &crc, &phe);

        Serial.print(F("  "));
        if (trim < 10) Serial.print(' ');
        Serial.print(trim);
        Serial.print(F("  |   "));
        if (pllRate < 10) Serial.print(' ');
        Serial.print(pllRate);
        Serial.print(F("   |  "));
        Serial.print(good);
        Serial.print(F("   |  "));
        Serial.print(crc);
        Serial.print(F("  |  "));
        Serial.println(phe);

        if (pllRate < bestPllRate ||
            (pllRate == bestPllRate && (good + crc + phe) > 0)) {
            bestPllRate = pllRate;
            bestTrim = trim;
        }
    }

    Serial.print(F("\nBest trim: "));
    Serial.print(bestTrim);
    Serial.print(F(" (PLL_LL="));
    Serial.print(bestPllRate);
    Serial.println(F(")"));

    // Also test neighbors of best for fine-tuning
    Serial.println(F("\n--- Fine-tuning around best ---"));
    uint8_t fineStart = (bestTrim > 1) ? bestTrim - 1 : 0;
    uint8_t fineEnd = (bestTrim < 30) ? bestTrim + 1 : 31;
    for (uint8_t trim = fineStart; trim <= fineEnd; trim++) {
        uint8_t good, crc, phe;
        uint16_t pllRate = measurePllRate(trim, 25, &good, &crc, &phe);

        Serial.print(F("  "));
        if (trim < 10) Serial.print(' ');
        Serial.print(trim);
        Serial.print(F("  |   "));
        if (pllRate < 10) Serial.print(' ');
        Serial.print(pllRate);
        Serial.print(F("   |  "));
        Serial.print(good);
        Serial.print(F("   |  "));
        Serial.print(crc);
        Serial.print(F("  |  "));
        Serial.println(phe);

        if (pllRate < bestPllRate ||
            (pllRate == bestPllRate && (good + crc + phe) > 0)) {
            bestPllRate = pllRate;
            bestTrim = trim;
        }
    }

    Serial.print(F("\nFinal best trim: "));
    Serial.print(bestTrim);
    Serial.print(F(" (PLL_LL="));
    Serial.print(bestPllRate);
    Serial.println(F(")"));

    sweepDone = true;

    // Initialize for continuous RX with best trim
    Serial.println(F("\n--- Continuous RX with best trim ---"));
    fullInit(bestTrim);

    byte xtalt[1];
    DW1000.readBytes(FS_CTRL_REG, FS_XTALT_SUB, xtalt, 1);
    Serial.print(F("Active XTALT: 0x"));
    Serial.print(xtalt[0], HEX);
    Serial.print(F(" trim="));
    Serial.println(xtalt[0] & 0x1F);

    Serial.println(F("Starting RX...\n"));
}

void loop() {
    static uint32_t lastReport = 0;

    if (!sweepDone) return;

    cycles++;

    if (deadCycles >= 50) {
        watchdogResets++;
        Serial.print(F("[WD #"));
        Serial.print(watchdogResets);
        Serial.println(F("]"));
        fullInit(bestTrim);
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

    if (clkPllLL) clkPllCnt++;

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
        for (int i = 0; i < 8; i++) {
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

    } else if (rfSyncLoss && (preamble || sfdDet || phrDet)) {
        rxRFSL++;
        deadCycles = 0;

    } else if (preamble || sfdDet) {
        deadCycles = 0;
    } else {
        deadCycles++;
    }

    DW1000.idle();

    if (millis() - lastReport >= 10000) {
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
        Serial.print(F(" clk:"));
        Serial.print(clkPllCnt);
        Serial.print(F("/"));
        Serial.print(cycles);
        Serial.print(F(" wd:"));
        Serial.println(watchdogResets);
    }
}
