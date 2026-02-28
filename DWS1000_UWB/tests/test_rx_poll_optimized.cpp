/**
 * RX Test v6 - Optimized SPI Polling with Double-Read Verification
 *
 * After proving IRQ pin doesn't assert for RX events on DWS1000 shield,
 * this test optimizes the SPI polling approach:
 *
 *   1. Fast continuous polling of SYS_STATUS via SPI
 *   2. Double-read verification: on event detection, read again to confirm
 *      (eliminates false positives from SPI corruption during RX mode)
 *   3. Read frame data AFTER forcing IDLE (where SPI is 100% reliable)
 *   4. Watchdog restart if no events for 10 seconds
 *
 * SPI reliability: IDLE=100%, TX=100%, RX=75-90%
 * Expected: ~33% raw detection → better with double-read filtering
 *
 * Upload to: receiver device while TX runs on other device
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

#define SYS_STATUS_REG  0x0F
#define SYS_MASK_REG    0x0E
#define SYS_CTRL_REG    0x0D
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// RX event bits in SYS_STATUS
#define RX_GOOD_MASK    ((1UL << 14) | (1UL << 13))  // RXFCG | RXDFR
#define RX_ERROR_MASK   ((1UL << 15) | (1UL << 12) | (1UL << 16) | (1UL << 18))  // RXFCE|RXPHE|RXRFSL|LDEERR
#define RX_ANY_MASK     (RX_GOOD_MASK | RX_ERROR_MASK)

// Stats
uint32_t rxGood = 0;
uint32_t rxFailed = 0;
uint32_t rxFalsePositive = 0;
uint32_t pollCount = 0;
uint32_t watchdogRestarts = 0;

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

void clearStatusAndRestart() {
    // Force IDLE first (makes SPI 100% reliable)
    DW1000.idle();
    delayMicroseconds(50);

    // Clear all status in IDLE mode
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);

    // Restart receiver
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(false);
    DW1000.startReceive();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Poll Optimized v6 ==="));

    // Init without IRQ handling (not used for RX on this hardware)
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    // Configure - match TX
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

    // Clear status
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delay(10);

    // Start receiving (single shot — return to IDLE after event)
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(false);
    DW1000.startReceive();

    Serial.println(F("Polling SYS_STATUS for frames...\n"));
}

void loop() {
    static uint32_t lastReport = 0;
    static uint32_t lastEventTime = 0;

    pollCount++;

    // === Fast SPI poll of SYS_STATUS ===
    uint32_t s1 = readStatus();

    if (s1 & RX_ANY_MASK) {
        // Possible event detected — verify with second read
        delayMicroseconds(50);
        uint32_t s2 = readStatus();

        // Both reads must agree on which RX bits are set
        uint32_t confirmed = (s1 & RX_ANY_MASK) & (s2 & RX_ANY_MASK);

        if (confirmed == 0) {
            // False positive: first read had event bits but second didn't
            rxFalsePositive++;

            // If status is 0xFFFFFFFF, it's pure SPI corruption
            if (s1 == 0xFFFFFFFF || s2 == 0xFFFFFFFF) {
                // Don't restart — receiver is still running
                return;
            }

            // Ambiguous — force IDLE and restart to be safe
            clearStatusAndRestart();
            lastEventTime = millis();
            return;
        }

        // === CONFIRMED event ===
        // Force IDLE for reliable SPI reads
        DW1000.idle();
        delayMicroseconds(50);

        // Read status one more time in IDLE (100% reliable)
        uint32_t s = readStatus();

        bool frameGood = s & (1UL << 14);   // RXFCG
        bool frameDone = s & (1UL << 13);   // RXDFR
        bool crcErr    = s & (1UL << 15);   // RXFCE
        bool headerErr = s & (1UL << 12);   // RXPHE

        if (frameGood && frameDone) {
            // === GOOD FRAME ===
            rxGood++;
            lastEventTime = millis();

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
            Serial.println(F("\""));

        } else if (crcErr || headerErr || frameDone) {
            // === ERROR frame ===
            rxFailed++;
            lastEventTime = millis();

            Serial.print(F("[ERR #"));
            Serial.print(rxFailed);
            if (crcErr) Serial.print(F(" CRC"));
            if (headerErr) Serial.print(F(" HDR"));
            if (frameDone && !frameGood) Serial.print(F(" NOCRC"));
            Serial.println(F("]"));
        } else {
            // No real RX bits in IDLE read — was a false positive
            rxFalsePositive++;
        }

        // Clear and restart
        clearStatusAndRestart();
    }

    // === Periodic report every 5s ===
    if (millis() - lastReport >= 5000) {
        lastReport = millis();

        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] G:"));
        Serial.print(rxGood);
        Serial.print(F(" F:"));
        Serial.print(rxFailed);
        Serial.print(F(" FP:"));
        Serial.print(rxFalsePositive);
        Serial.print(F(" WD:"));
        Serial.print(watchdogRestarts);
        Serial.print(F(" polls:"));
        Serial.println(pollCount);

        // Watchdog: restart if no events for 10s
        if (millis() > 15000 && millis() - lastEventTime > 10000) {
            watchdogRestarts++;
            Serial.println(F("  >> WATCHDOG: Restarting RX"));
            clearStatusAndRestart();
            lastEventTime = millis();
        }
    }

    // Delay between polls — continuous SPI traffic may interfere with receiver
    // At 110kbps + 2048 preamble, a frame takes ~19ms to receive
    // Poll every 25ms to give receiver uninterrupted time
    delay(25);
}
