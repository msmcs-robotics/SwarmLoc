/**
 * RX Test v3 - Comprehensive debugging
 *
 * Shows DW1000 internal state and tries both polling methods
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
#define SYS_STATE_REG   0x19   // System state register
#define RX_FINFO_REG    0x10   // RX frame info
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Stats
uint32_t pollCount = 0;
uint32_t rxGood = 0;
uint32_t rxBad = 0;

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);
    Serial.print(F("OTP LDO: 0x"));
    Serial.println(ldoTune[0], HEX);

    if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        aonCtrl[0] |= 0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        delay(1);
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        Serial.println(F("LDO applied"));
    }
}

void printHex(byte* data, int len) {
    for (int i = 0; i < len; i++) {
        if (data[i] < 16) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
}

void printStatus(uint32_t s) {
    if (s & (1UL << 0)) Serial.print(F(" IRQS"));
    if (s & (1UL << 1)) Serial.print(F(" CPLOCK"));
    if (s & (1UL << 7)) Serial.print(F(" TXFRS"));
    if (s & (1UL << 10)) Serial.print(F(" RXPHE"));
    if (s & (1UL << 12)) Serial.print(F(" LDEERR"));
    if (s & (1UL << 13)) Serial.print(F(" RXDFR"));
    if (s & (1UL << 14)) Serial.print(F(" RXFCG"));
    if (s & (1UL << 15)) Serial.print(F(" RXFCE"));
    if (s & (1UL << 17)) Serial.print(F(" RXRFTO"));
    if (s & (1UL << 21)) Serial.print(F(" RXOVRR"));
}

uint32_t readStatus() {
    byte status[5];
    DW1000.readBytes(SYS_STATUS_REG, 0, status, 5);

    // Check for obvious corruption
    if (status[0] == 0xFF && status[1] == 0xFF && status[2] == 0xFF) {
        return 0xFFFFFFFF;  // Signal corruption
    }

    return status[0] | ((uint32_t)status[1] << 8) |
           ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);
}

void clearStatusAndRestart() {
    byte clear[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0, clear, 5);
    DW1000.newReceive();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Test v3 ==="));

    // Initialize without library IRQ
    DW1000.begin(0xFF, PIN_RST);
    DW1000.select(PIN_SS);

    // Device info
    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    // Apply LDO
    applyLDOTuning();

    // Configure exactly like TX
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_SHORTDATA_FAST_LOWPOWER);
    DW1000.setFrameFilter(false);
    DW1000.suppressFrameCheck(false);  // Enable CRC check
    DW1000.commitConfiguration();

    // Re-apply LDO
    applyLDOTuning();

    // Read channel config
    DW1000.getPrintableDeviceMode(msg);
    Serial.print(F("Mode: "));
    Serial.println(msg);

    // Setup IRQ pin
    pinMode(PIN_IRQ, INPUT);

    // Start receiver
    clearStatusAndRestart();

    Serial.print(F("Initial IRQ pin: "));
    Serial.println(digitalRead(PIN_IRQ));

    uint32_t s = readStatus();
    Serial.print(F("Initial status: 0x"));
    Serial.println(s, HEX);

    Serial.println(F("\nPolling for frames (TX should send PING every 2s)...\n"));
}

void loop() {
    static uint32_t lastReport = 0;
    static uint32_t lastStatusPrint = 0;

    pollCount++;

    // Check IRQ pin
    bool irqHigh = digitalRead(PIN_IRQ);

    // Read status
    uint32_t status = readStatus();

    // Only process if we have real data and status isn't corrupt
    if (status != 0xFFFFFFFF && status != 0) {
        // Check for any RX event
        bool hasRxEvent = (status & ((1UL << 13) | (1UL << 14) | (1UL << 15) |
                                     (1UL << 10) | (1UL << 12) | (1UL << 17)));

        if (hasRxEvent) {
            Serial.print(F("RX event: 0x"));
            Serial.print(status, HEX);
            printStatus(status);
            Serial.println();

            if (status & (1UL << 14)) {  // RXFCG - good frame
                rxGood++;
                uint16_t len = DW1000.getDataLength();
                Serial.print(F("  *** GOOD FRAME *** len="));
                Serial.print(len);

                if (len > 0 && len < 128) {
                    byte data[128];
                    DW1000.getData(data, len);
                    Serial.print(F(" data=\""));
                    for (int i = 0; i < min((int)len, 32); i++) {
                        Serial.write(data[i] >= 32 && data[i] < 127 ? data[i] : '.');
                    }
                    Serial.print(F("\""));
                }
                Serial.println();
            } else {
                rxBad++;
            }

            // Clear and restart
            clearStatusAndRestart();
        }
    }

    // Periodic status print
    if (millis() - lastStatusPrint >= 2000) {
        lastStatusPrint = millis();

        uint32_t s = readStatus();
        Serial.print(F("[t="));
        Serial.print(millis() / 1000);
        Serial.print(F("s] IRQ="));
        Serial.print(digitalRead(PIN_IRQ));
        Serial.print(F(" S=0x"));
        Serial.print(s, HEX);
        Serial.print(F(" Good:"));
        Serial.print(rxGood);
        Serial.print(F(" Bad:"));
        Serial.println(rxBad);
    }

    // Small delay
    delay(5);
}
