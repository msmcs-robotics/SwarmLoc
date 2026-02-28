/**
 * RX Test v4 - IRQ Pin Polling (Zero SPI During RX)
 *
 * KEY INSIGHT: Instead of polling SYS_STATUS via SPI (75-90% reliable
 * during RX mode), we poll the IRQ *pin* as a digital input (100% reliable).
 *
 * When DW1000 has an event (frame received, error), it:
 *   1. Asserts the IRQ pin HIGH (hardware signal, not SPI)
 *   2. Returns to IDLE (when RXAUTR is off)
 *   3. SPI becomes 100% reliable in IDLE
 *
 * Flow:
 *   Start RX → poll digitalRead(IRQ) → event! → read status via SPI (IDLE=reliable)
 *   → process frame → clear status → restart RX
 *
 * This eliminates ALL SPI transactions during active receive.
 *
 * Upload to: receiver device while TX runs on other device
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Register addresses
#define SYS_STATUS_REG  0x0F
#define SYS_MASK_REG    0x0E
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Stats
uint32_t rxGood = 0;
uint32_t rxFailed = 0;
uint32_t rxTimeout = 0;
uint32_t irqEvents = 0;
uint32_t watchdogRestarts = 0;

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);

    Serial.print(F("  OTP LDO: 0x"));
    Serial.println(ldoTune[0], HEX);

    if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        aonCtrl[0] |= 0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        delay(1);
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        Serial.println(F("  LDO APPLIED"));
    }
}

void startReceiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(false);  // Return to IDLE after receive
    DW1000.startReceive();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX IRQ Pin Polling Test v4 ==="));

    // Init WITHOUT library IRQ handling (we poll the pin ourselves)
    DW1000.begin(0xFF, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    // Configure - match TX settings
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

    // === KEY: Set SYS_MASK to enable IRQ pin for receive events ===
    // SYS_MASK register (0x0E) is 4 bytes, uses same bit defs as SYS_STATUS
    //   Bit 12: RXPHE   - PHY header error
    //   Bit 13: RXDFR   - Data frame received
    //   Bit 14: RXFCG   - Frame check good (CRC passed)
    //   Bit 15: RXFCE   - Frame check error (CRC fail)
    //   Bit 16: RXRFSL  - Reed Solomon error
    //   Bit 17: RXRFTO  - Receive frame wait timeout
    //   Bit 18: LDEERR  - LDE processing error
    byte mask[4] = {0x00, 0x00, 0x00, 0x00};
    mask[1] = (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);  // bits 12-15
    mask[2] = (1 << 0) | (1 << 1) | (1 << 2);              // bits 16-18
    DW1000.writeBytes(SYS_MASK_REG, 0x00, mask, 4);

    // Verify mask was written
    byte maskRead[4];
    DW1000.readBytes(SYS_MASK_REG, 0x00, maskRead, 4);
    Serial.print(F("SYS_MASK: 0x"));
    for (int i = 3; i >= 0; i--) {
        if (maskRead[i] < 16) Serial.print("0");
        Serial.print(maskRead[i], HEX);
    }
    Serial.println();

    // Set IRQ pin as digital input (NOT using attachInterrupt)
    pinMode(PIN_IRQ, INPUT);

    // Clear all status bits
    byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
    delay(10);

    // Verify IRQ pin is LOW before starting
    Serial.print(F("IRQ pin before RX: "));
    Serial.println(digitalRead(PIN_IRQ) ? "HIGH" : "LOW");

    // Start receiver
    startReceiver();
    delay(10);

    // Clear any startup transients from IRQ
    if (digitalRead(PIN_IRQ)) {
        byte clearAll2[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll2, 5);
        delay(5);
        startReceiver();
        delay(5);
    }

    Serial.print(F("IRQ pin after RX start: "));
    Serial.println(digitalRead(PIN_IRQ) ? "HIGH" : "LOW");

    Serial.println(F("Waiting for frames via IRQ pin...\n"));
}

void loop() {
    static uint32_t lastReport = 0;
    static uint32_t lastEventTime = 0;

    // === PRIMARY: Check IRQ pin (digital read, no SPI!) ===
    if (digitalRead(PIN_IRQ)) {
        // DW1000 has an event!
        // With receivePermanently(false), chip is now in IDLE
        // SPI is 100% reliable in IDLE mode
        irqEvents++;

        // Small settling delay
        delayMicroseconds(50);

        // Read status register - this is in IDLE mode, so SPI is reliable
        byte status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);
        uint32_t s = (uint32_t)status[0] | ((uint32_t)status[1] << 8) |
                     ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);

        bool frameGood = s & (1UL << 14);   // RXFCG
        bool frameDone = s & (1UL << 13);   // RXDFR
        bool crcErr    = s & (1UL << 15);   // RXFCE
        bool headerErr = s & (1UL << 12);   // RXPHE
        bool rsErr     = s & (1UL << 16);   // RXRFSL
        bool ldeErr    = s & (1UL << 18);   // LDEERR

        if (frameGood) {
            // === GOOD FRAME ===
            rxGood++;
            lastEventTime = millis();

            // Read data - still in IDLE, SPI reliable
            byte data[128];
            int dataLen = DW1000.getDataLength();
            if (dataLen > 127) dataLen = 127;
            if (dataLen > 0) {
                DW1000.getData(data, dataLen);
            }

            Serial.print(F("RX #"));
            Serial.print(rxGood);
            Serial.print(F(" len="));
            Serial.print(dataLen);
            Serial.print(F(" \""));
            for (int i = 0; i < dataLen && i < 32; i++) {
                if (data[i] >= 32 && data[i] < 127) {
                    Serial.print((char)data[i]);
                } else {
                    Serial.print('.');
                }
            }
            Serial.print(F("\" S:0x"));
            Serial.print(s, HEX);
            Serial.println(F(" OK"));

        } else if (frameDone || crcErr || headerErr || rsErr || ldeErr) {
            // === ERROR ===
            rxFailed++;
            lastEventTime = millis();

            Serial.print(F("[ERR #"));
            Serial.print(rxFailed);
            if (crcErr) Serial.print(F(" CRC"));
            if (headerErr) Serial.print(F(" HDR"));
            if (rsErr) Serial.print(F(" RS"));
            if (ldeErr) Serial.print(F(" LDE"));
            if (frameDone && !frameGood) Serial.print(F(" NOCRC"));
            Serial.print(F(" S:0x"));
            Serial.print(s, HEX);
            Serial.println(F("]"));
        } else {
            // Unknown event (maybe PLL or TX related)
            Serial.print(F("[UNK S:0x"));
            Serial.print(s, HEX);
            Serial.println(F("]"));
        }

        // Clear ALL status bits
        byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);

        // Restart receiver
        delay(1);
        startReceiver();
    }

    // === Periodic status report (every 5s) ===
    if (millis() - lastReport >= 5000) {
        lastReport = millis();

        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] G:"));
        Serial.print(rxGood);
        Serial.print(F(" F:"));
        Serial.print(rxFailed);
        Serial.print(F(" IRQ:"));
        Serial.print(irqEvents);
        Serial.print(F(" WD:"));
        Serial.print(watchdogRestarts);
        Serial.print(F(" pin="));
        Serial.println(digitalRead(PIN_IRQ) ? "HIGH" : "LOW");

        // === Watchdog: restart if no events for 10 seconds ===
        if (millis() > 15000 && millis() - lastEventTime > 10000) {
            watchdogRestarts++;
            Serial.println(F("  >> WATCHDOG: Restarting RX"));

            DW1000.idle();
            delay(5);

            byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);
            delay(5);

            startReceiver();
            lastEventTime = millis();
        }
    }

    // Very fast polling - no SPI involved, just a digitalRead
    delayMicroseconds(100);
}
