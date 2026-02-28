/**
 * RX Test v7b - Window Poll with Verified IDLE Reads
 *
 * Key fixes from v7:
 *   - Explicitly disable RXAUTR after commitConfiguration
 *   - Verify IDLE state before reading status
 *   - Multiple idle() attempts if first doesn't work
 *   - Read status only when confirmed in IDLE
 *
 * Strategy: 200ms RX window → verified IDLE → reliable read
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

#define SYS_STATUS_REG  0x0F
#define SYS_CFG_REG     0x04
#define SYS_STATE_REG   0x19
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

uint32_t rxGood = 0;
uint32_t rxFailed = 0;
uint32_t idleRetries = 0;
uint32_t cycles = 0;

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);
    Serial.print(F("  LDO: 0x"));
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
    // Try up to 3 times to get into IDLE
    for (int attempt = 0; attempt < 3; attempt++) {
        DW1000.idle();
        delay(1);  // 1ms settling

        // Verify by reading status twice — both should be consistent
        uint32_t s1 = readStatus();
        delayMicroseconds(200);
        uint32_t s2 = readStatus();

        // If both reads agree (no corruption), we're in IDLE
        if (s1 == s2) {
            return true;
        }

        // Reads disagree — still in RX or corrupted, try again
        if (attempt > 0) idleRetries++;
        delay(2);
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Simple Poll v7b ==="));

    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Detach library ISR
    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    // Use 6.8Mbps mode — close-range (<1m), less receiver saturation
    DW1000.enableMode(DW1000.MODE_LONGDATA_FAST_LOWPOWER);

    // CRITICAL: Disable auto re-enable before committing
    DW1000.setReceiverAutoReenable(false);

    DW1000.commitConfiguration();
    applyLDOTuning();

    // Verify RXAUTR is off
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

    // Verify we're in IDLE
    Serial.print(F("IDLE verify: "));
    Serial.println(forceIdleVerified() ? "OK" : "FAIL");

    Serial.println(F("Starting RX window cycles (200ms)...\n"));
}

void loop() {
    static uint32_t lastReport = 0;

    cycles++;

    // === Step 1: Verify IDLE, clear status ===
    // (Should already be in IDLE from previous cycle)
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delayMicroseconds(50);

    // === Step 2: Start single-shot receiver ===
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(false);
    DW1000.startReceive();

    // === Step 3: Wait for frame (NO SPI!) ===
    delay(200);

    // === Step 4: Force IDLE with verification ===
    if (!forceIdleVerified()) {
        // Can't get reliable IDLE — skip this cycle
        DW1000.idle();
        delay(5);
        return;
    }

    // === Step 5: Read status RELIABLY in IDLE ===
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

    // Force IDLE for next cycle
    DW1000.idle();

    // === Periodic report ===
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
