/**
 * RX Test v8c - Hardware Reset Watchdog + SPI Corruption Guard
 *
 * Changes from v8b:
 *   - 0xFFFFFFFF status = SPI corruption, NOT a valid frame
 *   - Removed PLL clock-toggle recovery (broke SPI in v8b)
 *   - Watchdog uses hardware reset (PIN_RST=7) for clean recovery
 *   - Reads data on CRC errors to check payload
 *   - Tracks consecutive empty/corrupt cycles for watchdog
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
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

uint32_t rxGood = 0;
uint32_t rxCRC = 0;
uint32_t rxHDR = 0;
uint32_t spiCorrupt = 0;
uint32_t cycles = 0;
uint32_t deadCycles = 0;   // consecutive cycles with no real event
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

bool forceIdleVerified() {
    for (int attempt = 0; attempt < 3; attempt++) {
        DW1000.idle();
        delay(1);
        uint32_t s1 = readStatus();
        delayMicroseconds(200);
        uint32_t s2 = readStatus();
        // Must match AND not be all-ones (SPI corruption)
        if (s1 == s2 && s1 != 0xFFFFFFFF) return true;
        if (attempt > 0) delay(2);
    }
    return false;
}

void hardwareReset() {
    // Toggle the actual RST pin for a clean DW1000 reset
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(2);
    pinMode(PIN_RST, INPUT);  // Let it float high
    delay(10);
}

void fullInit() {
    hardwareReset();

    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);
    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    applyLDOTuning();

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

void printData(int dataLen) {
    byte data[128];
    if (dataLen > 127) dataLen = 127;
    if (dataLen > 0) DW1000.getData(data, dataLen);

    Serial.print(F(" len="));
    Serial.print(dataLen);
    Serial.print(F(" \""));
    for (int i = 0; i < dataLen && i < 32; i++) {
        if (data[i] >= 32 && data[i] < 127) Serial.print((char)data[i]);
        else Serial.print('.');
    }
    Serial.print(F("\""));
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX v8c - HW Reset Watchdog ==="));

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

    Serial.println(F("Starting RX (200ms windows)...\n"));
}

void loop() {
    static uint32_t lastReport = 0;
    cycles++;

    // Watchdog: hardware reset after 50 dead cycles (~10s of nothing)
    if (deadCycles >= 50) {
        watchdogResets++;
        Serial.print(F("[WD RESET #"));
        Serial.print(watchdogResets);
        Serial.println(F("]"));
        fullInit();
        deadCycles = 0;
        return;
    }

    // Clear all status bits
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delayMicroseconds(50);

    // Start single-shot receive
    DW1000.newReceive();
    DW1000.receivePermanently(false);
    DW1000.startReceive();

    // Wait for frame
    delay(200);

    // Force IDLE with verification (rejects 0xFFFFFFFF)
    if (!forceIdleVerified()) {
        // Can't get clean IDLE — count as dead cycle
        spiCorrupt++;
        deadCycles++;
        DW1000.idle();
        delay(5);
        return;
    }

    // Read status (verified non-corrupt by forceIdleVerified)
    uint32_t s = readStatus();

    // Double-check for corruption
    if (s == 0xFFFFFFFF || s == 0x00000000) {
        deadCycles++;
        DW1000.idle();
        return;
    }

    bool preamble  = s & (1UL << 8);   // RXPRD
    bool headerErr = s & (1UL << 12);  // RXPHE
    bool frameRx   = s & (1UL << 13);  // RXDFR
    bool frameGood = s & (1UL << 14);  // RXFCG
    bool crcErr    = s & (1UL << 15);  // RXFCE

    if (frameGood && frameRx) {
        rxGood++;
        deadCycles = 0;
        int dataLen = DW1000.getDataLength();

        Serial.print(F("RX #"));
        Serial.print(rxGood);
        printData(dataLen);
        Serial.print(F(" S:0x"));
        Serial.println(s, HEX);

    } else if (crcErr && frameRx) {
        rxCRC++;
        deadCycles = 0;
        int dataLen = DW1000.getDataLength();

        Serial.print(F("[CRC #"));
        Serial.print(rxCRC);
        printData(dataLen);
        Serial.print(F(" S:0x"));
        Serial.print(s, HEX);
        Serial.println(F("]"));

    } else if (headerErr || preamble) {
        rxHDR++;
        deadCycles = 0;

        Serial.print(F("[HDR #"));
        Serial.print(rxHDR);
        Serial.print(F(" S:0x"));
        Serial.print(s, HEX);
        Serial.println(F("]"));

    } else {
        // Status has bits set but no RX event — just count as idle
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
        Serial.print(F(" spi:"));
        Serial.print(spiCorrupt);
        Serial.print(F(" wd:"));
        Serial.println(watchdogResets);
    }
}
