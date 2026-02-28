/**
 * RX Test v8 - Correct Reset Pin + Clean Loop
 *
 * Key changes from v7b:
 *   - PIN_RST = 7 (correct for DWS1000 shield, was 9 = wrong pin!)
 *   - Removed setDefaults() from loop (was no-op but misleading)
 *   - 110kbps mode (MODE_LONGDATA_RANGE_LOWPOWER) to match TX default
 *   - RXAUTR disabled, verified IDLE reads
 *
 * Hypothesis: Proper hardware reset fixes RXPHE (PHY header errors)
 * that occur on every frame despite 57% preamble detection.
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

// DWS1000 shield correct pin configuration
const uint8_t PIN_RST = 7;   // DWS1000 routes RST to D7 (was incorrectly 9)
const uint8_t PIN_IRQ = 2;   // IRQ jumpered from D8 to D2
const uint8_t PIN_SS = SS;   // SS/CS = D10

#define SYS_STATUS_REG  0x0F
#define SYS_CFG_REG     0x04
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

uint32_t rxGood = 0;
uint32_t rxFailed = 0;
uint32_t idleRetries = 0;
uint32_t cycles = 0;

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

        if (s1 == s2) return true;

        if (attempt > 0) idleRetries++;
        delay(2);
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX v8 - Reset Pin Fix ==="));

    // Initialize with CORRECT reset pin for DWS1000 shield
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Detach library ISR (we use SPI polling)
    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    // Configure — 110kbps to match TX
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setReceiverAutoReenable(false);
    DW1000.commitConfiguration();

    applyLDOTuning();

    // Verify RXAUTR off
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

    Serial.print(F("IDLE: "));
    Serial.println(forceIdleVerified() ? "OK" : "FAIL");

    Serial.println(F("Starting RX (200ms windows)...\n"));
}

void loop() {
    static uint32_t lastReport = 0;
    cycles++;

    // Clear all status bits
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delayMicroseconds(50);

    // Start single-shot receive (no setDefaults — config is set in setup)
    DW1000.newReceive();
    DW1000.receivePermanently(false);
    DW1000.startReceive();

    // Wait for frame
    delay(200);

    // Force IDLE with verification
    if (!forceIdleVerified()) {
        DW1000.idle();
        delay(5);
        return;
    }

    // Read status reliably in IDLE
    uint32_t s = readStatus();

    bool frameGood = (s & (1UL << 14)) && (s & (1UL << 13));  // RXFCG + RXDFR
    bool crcErr    = s & (1UL << 15);
    bool headerErr = s & (1UL << 12);

    if (frameGood) {
        rxGood++;

        byte data[128];
        int dataLen = DW1000.getDataLength();
        if (dataLen > 127) dataLen = 127;
        if (dataLen > 0) DW1000.getData(data, dataLen);

        Serial.print(F("RX #"));
        Serial.print(rxGood);
        Serial.print(F(" len="));
        Serial.print(dataLen);
        Serial.print(F(" \""));
        for (int i = 0; i < dataLen && i < 32; i++) {
            if (data[i] >= 32 && data[i] < 127) Serial.print((char)data[i]);
            else Serial.print('.');
        }
        Serial.print(F("\" S:0x"));
        Serial.println(s, HEX);

    } else if (crcErr || headerErr) {
        rxFailed++;
        Serial.print(F("[ERR #"));
        Serial.print(rxFailed);
        if (crcErr) Serial.print(F(" CRC"));
        if (headerErr) Serial.print(F(" HDR"));
        Serial.print(F(" S:0x"));
        Serial.print(s, HEX);
        Serial.println(F("]"));
    }

    DW1000.idle();

    // Periodic report
    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] G:"));
        Serial.print(rxGood);
        Serial.print(F(" F:"));
        Serial.print(rxFailed);
        Serial.print(F(" cyc:"));
        Serial.print(cycles);
        Serial.print(F(" retry:"));
        Serial.println(idleRetries);
    }
}
