/**
 * RX Test using DW1000-ng library
 *
 * DW1000-ng has better initialization (PLLLDT, slow SPI, proper clock
 * sequencing) which previously yielded 63+ frame detections.
 *
 * Uses polling mode (no IRQ) with DW1000-ng status checking.
 * Also reads raw SYS_STATUS for CLKPLL_LL monitoring.
 *
 * 110kbps, 16MHz PRF, Ch5, Preamble 2048, Code 4
 * PIN_RST = 7
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000Ng.hpp>
#include <DW1000NgConstants.hpp>
#include <DW1000NgConfiguration.hpp>
#include <DW1000NgRegisters.hpp>

const uint8_t PIN_RST = 7;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

uint32_t rxGood = 0;
uint32_t rxCRC = 0;
uint32_t rxPHE = 0;
uint32_t rxRFSL = 0;
uint32_t clkPllCnt = 0;
uint32_t rfPllCnt = 0;
uint32_t spiCorrupt = 0;
uint32_t cycles = 0;
uint32_t deadCycles = 0;
uint32_t watchdogResets = 0;

device_configuration_t DEFAULT_CONFIG = {
    false,                       // extendedFrameLength
    false,                       // receiverAutoReenable
    true,                        // smartPower
    true,                        // frameCheck
    false,                       // nlos
    SFDMode::STANDARD_SFD,       // sfd
    Channel::CHANNEL_5,          // channel
    DataRate::RATE_110KBPS,      // dataRate
    PulseFrequency::FREQ_16MHZ,  // pulseFreq
    PreambleLength::LEN_2048,    // preambleLen
    PreambleCode::CODE_4         // preaCode
};

// Read raw SYS_STATUS register (low-level via DW1000-ng internal access)
uint32_t readRawStatus() {
    byte status[5];
    DW1000Ng::readBytes(SYS_STATUS, NO_SUB, status, 5);
    return (uint32_t)status[0] | ((uint32_t)status[1] << 8) |
           ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);
}

void clearAllStatus() {
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000Ng::writeBytes(SYS_STATUS, NO_SUB, clearAll, 5);
}

uint16_t readRawFrameLength() {
    byte rxInfo[4];
    DW1000Ng::readBytes(RX_FINFO, NO_SUB, rxInfo, 4);
    return ((uint16_t)rxInfo[1] << 8 | (uint16_t)rxInfo[0]) & 0x03FF;
}

void readRawData(byte* buf, uint16_t len) {
    if (len > 127) len = 127;
    DW1000Ng::readBytes(RX_BUFFER, NO_SUB, buf, len);
}

void printStatusDecode(uint32_t s) {
    if (s & (1UL << 1))  Serial.print(F("CPL "));
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

void hardwareReset() {
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(2);
    pinMode(PIN_RST, INPUT);
    delay(10);
}

void fullInit() {
    hardwareReset();
    DW1000Ng::initialize(PIN_SS, PIN_IRQ, PIN_RST);
    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
    DW1000Ng::setDeviceAddress(2);
    DW1000Ng::setNetworkId(10);

    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    // Check CPLOCK after init
    uint32_t s = readRawStatus();
    Serial.print(F("Init status: 0x"));
    Serial.print(s, HEX);
    Serial.print(F(" CPL="));
    Serial.print((s & (1UL << 1)) ? "Y" : "N");
    Serial.print(F(" clk="));
    Serial.println((s & (1UL << 25)) ? "Y" : "N");

    clearAllStatus();
    delay(5);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX (DW1000-ng) ==="));

    fullInit();

    char devId[128];
    DW1000Ng::getPrintableDeviceIdentifier(devId);
    Serial.print(F("Device: "));
    Serial.println(devId);

    char msg[128];
    DW1000Ng::getPrintableDeviceMode(msg);
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

    clearAllStatus();
    delayMicroseconds(50);

    DW1000Ng::startReceive();

    delay(200);

    DW1000Ng::forceTRxOff();
    delay(1);

    uint32_t s = readRawStatus();

    if (s == 0xFFFFFFFF || s == 0x00000000) {
        deadCycles++;
        return;
    }

    uint8_t bitCount = 0;
    uint32_t tmp = s;
    while (tmp) { bitCount += tmp & 1; tmp >>= 1; }
    if (bitCount > 20) { deadCycles++; spiCorrupt++; return; }

    if (isStatusContradictory(s)) {
        spiCorrupt++;
        deadCycles++;
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

        // Try DW1000-ng API first
        uint16_t ngLen = DW1000Ng::getReceivedDataLength();
        uint16_t rawLen = readRawFrameLength();

        Serial.print(F("*** RX #"));
        Serial.print(rxGood);
        Serial.print(F(" ngL="));
        Serial.print(ngLen);
        Serial.print(F(" rawL="));
        Serial.print(rawLen);

        uint16_t useLen = (ngLen > 0) ? ngLen : rawLen;
        if (useLen > 0 && useLen < 128) {
            byte data[128];
            DW1000Ng::getReceivedData(data, useLen);
            Serial.print(F(" \""));
            for (uint16_t i = 0; i < useLen && i < 32; i++) {
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
        Serial.print(F(" spi:"));
        Serial.print(spiCorrupt);
        Serial.print(F(" clk:"));
        Serial.print(clkPllCnt);
        Serial.print(F("/"));
        Serial.print(cycles);
        Serial.print(F(" rf:"));
        Serial.print(rfPllCnt);
        Serial.print(F(" wd:"));
        Serial.println(watchdogResets);
    }
}
