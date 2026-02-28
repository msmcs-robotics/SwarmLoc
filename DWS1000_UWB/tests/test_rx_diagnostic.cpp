/**
 * RX Diagnostic Test - IRQ Pin + SPI Status Combined
 *
 * Polls BOTH the IRQ pin AND reads SYS_STATUS via SPI to determine:
 *   1. Is the DW1000 receiver detecting frames at all? (SYS_STATUS check)
 *   2. Is the IRQ pin asserting correctly? (digitalRead check)
 *   3. Is the SYS_MASK set correctly? (periodic readback)
 *
 * This separates "hardware not receiving" from "IRQ pin not routing".
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

#define SYS_STATUS_REG  0x0F
#define SYS_MASK_REG    0x0E
#define SYS_CFG_REG     0x04
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

uint32_t rxGood = 0;
uint32_t rxFailed = 0;
uint32_t irqPinEvents = 0;
uint32_t spiStatusEvents = 0;
uint32_t watchdogRestarts = 0;

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

void startReceiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(false);
    DW1000.startReceive();
}

void processEvent(uint32_t s) {
    bool frameGood = s & (1UL << 14);   // RXFCG
    bool frameDone = s & (1UL << 13);   // RXDFR
    bool crcErr    = s & (1UL << 15);   // RXFCE
    bool headerErr = s & (1UL << 12);   // RXPHE
    bool rsErr     = s & (1UL << 16);   // RXRFSL
    bool ldeErr    = s & (1UL << 18);   // LDEERR

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
    } else if (frameDone || crcErr || headerErr || rsErr || ldeErr) {
        rxFailed++;
        Serial.print(F("[ERR"));
        if (crcErr) Serial.print(F(" CRC"));
        if (headerErr) Serial.print(F(" HDR"));
        if (rsErr) Serial.print(F(" RS"));
        if (ldeErr) Serial.print(F(" LDE"));
        Serial.print(F(" S:0x"));
        Serial.print(s, HEX);
        Serial.println(F("]"));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Diagnostic (IRQ + SPI) ==="));

    // Initialize with REAL IRQ pin so library sets up properly
    // But we'll also poll the pin ourselves
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

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

    // Detach the library's ISR so it doesn't interfere with our polling
    detachInterrupt(digitalPinToInterrupt(PIN_IRQ));

    // Write SYS_MASK for receive events
    byte mask[4] = {0x00, 0x00, 0x00, 0x00};
    mask[1] = (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);  // bits 12-15
    mask[2] = (1 << 0) | (1 << 1) | (1 << 2);              // bits 16-18
    DW1000.writeBytes(SYS_MASK_REG, 0x00, mask, 4);

    // Verify mask
    byte maskRead[4];
    DW1000.readBytes(SYS_MASK_REG, 0x00, maskRead, 4);
    Serial.print(F("SYS_MASK: 0x"));
    for (int i = 3; i >= 0; i--) {
        if (maskRead[i] < 16) Serial.print("0");
        Serial.print(maskRead[i], HEX);
    }
    Serial.println();

    // Read SYS_CFG to verify interrupt polarity
    byte sysCfg[4];
    DW1000.readBytes(SYS_CFG_REG, 0x00, sysCfg, 4);
    Serial.print(F("SYS_CFG: 0x"));
    for (int i = 3; i >= 0; i--) {
        if (sysCfg[i] < 16) Serial.print("0");
        Serial.print(sysCfg[i], HEX);
    }
    Serial.print(F(" HIRQ_POL="));
    Serial.println((sysCfg[1] & 0x02) ? "ACTIVE_HIGH" : "ACTIVE_LOW");

    // Test pin D8 (shield IRQ output) vs D2 (where we read)
    pinMode(8, INPUT);
    pinMode(PIN_IRQ, INPUT);
    Serial.print(F("Pin D2: "));
    Serial.print(digitalRead(PIN_IRQ) ? "HIGH" : "LOW");
    Serial.print(F("  Pin D8: "));
    Serial.println(digitalRead(8) ? "HIGH" : "LOW");

    // Clear all status
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delay(10);

    // Start receiver
    startReceiver();
    delay(10);

    // Clear startup transients
    if (digitalRead(PIN_IRQ)) {
        Serial.println(F("Clearing startup IRQ transient"));
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
        delay(5);
        startReceiver();
        delay(5);
    }

    Serial.print(F("Pin D2 after RX: "));
    Serial.print(digitalRead(PIN_IRQ) ? "HIGH" : "LOW");
    Serial.print(F("  Pin D8 after RX: "));
    Serial.println(digitalRead(8) ? "HIGH" : "LOW");

    Serial.println(F("Waiting... (dual IRQ+SPI monitoring)\n"));
}

void loop() {
    static uint32_t lastReport = 0;
    static uint32_t lastEventTime = 0;
    static bool hadEvent = false;

    // === METHOD 1: Check IRQ pin (no SPI) ===
    if (digitalRead(PIN_IRQ)) {
        irqPinEvents++;
        delayMicroseconds(50);

        byte status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);
        uint32_t s = (uint32_t)status[0] | ((uint32_t)status[1] << 8) |
                     ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);

        Serial.print(F("[IRQ] "));
        processEvent(s);

        byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
        delay(1);
        startReceiver();
        lastEventTime = millis();
        hadEvent = true;
    }

    // === METHOD 2: Periodic SPI status check (every 500ms) ===
    static uint32_t lastSpiCheck = 0;
    if (millis() - lastSpiCheck >= 500) {
        lastSpiCheck = millis();

        byte status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);
        uint32_t s = (uint32_t)status[0] | ((uint32_t)status[1] << 8) |
                     ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);

        // Check if any receive event bits are set
        uint32_t rxBits = s & 0x0007F000UL;  // bits 12-18
        if (rxBits) {
            spiStatusEvents++;
            Serial.print(F("[SPI] "));
            processEvent(s);

            // If we got here by SPI but IRQ pin didn't fire, note it
            if (!hadEvent) {
                Serial.print(F("  >> IRQ pin was LOW! pin="));
                Serial.println(digitalRead(PIN_IRQ) ? "HIGH" : "LOW");
            }

            byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
            delay(1);
            startReceiver();
            lastEventTime = millis();
        }
        hadEvent = false;
    }

    // === Periodic report (every 5s) ===
    if (millis() - lastReport >= 5000) {
        lastReport = millis();

        // Read current status
        byte status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);
        uint32_t s = (uint32_t)status[0] | ((uint32_t)status[1] << 8) |
                     ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);

        // Read current mask
        byte maskNow[4];
        DW1000.readBytes(SYS_MASK_REG, 0x00, maskNow, 4);
        uint32_t m = (uint32_t)maskNow[0] | ((uint32_t)maskNow[1] << 8) |
                     ((uint32_t)maskNow[2] << 16) | ((uint32_t)maskNow[3] << 24);

        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] G:"));
        Serial.print(rxGood);
        Serial.print(F(" F:"));
        Serial.print(rxFailed);
        Serial.print(F(" IRQ:"));
        Serial.print(irqPinEvents);
        Serial.print(F(" SPI:"));
        Serial.print(spiStatusEvents);
        Serial.print(F(" WD:"));
        Serial.print(watchdogRestarts);

        Serial.print(F(" d2="));
        Serial.print(digitalRead(PIN_IRQ) ? "H" : "L");
        Serial.print(F(" d8="));
        Serial.print(digitalRead(8) ? "H" : "L");

        Serial.print(F(" S:0x"));
        Serial.print(s, HEX);
        Serial.print(F(" M:0x"));
        Serial.println(m, HEX);

        // Watchdog
        if (millis() > 15000 && millis() - lastEventTime > 10000) {
            watchdogRestarts++;
            Serial.println(F("  >> WATCHDOG: Restarting RX"));

            DW1000.idle();
            delay(5);

            byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
            delay(5);

            // Re-verify and re-write SYS_MASK
            byte mask[4] = {0x00, 0x00, 0x00, 0x00};
            mask[1] = (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);
            mask[2] = (1 << 0) | (1 << 1) | (1 << 2);
            DW1000.writeBytes(SYS_MASK_REG, 0x00, mask, 4);

            startReceiver();
            lastEventTime = millis();
        }
    }

    delayMicroseconds(100);
}
