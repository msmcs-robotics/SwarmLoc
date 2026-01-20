/**
 * RX Debug Test
 *
 * Debug version to understand why RX IRQ is not firing.
 * Polls status register AND checks IRQ pin manually.
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
#define SYS_CFG_REG     0x04
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Stats
volatile uint32_t irqCount = 0;
volatile bool irqFired = false;

// Simple ISR just to count interrupts
void simpleIRQ() {
    irqCount++;
    irqFired = true;
}

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

void printHex(byte* data, int len) {
    for (int i = 0; i < len; i++) {
        if (data[i] < 16) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Debug Test ==="));

    // Initialize WITHOUT library IRQ (we'll handle it ourselves)
    DW1000.begin(0xFF, PIN_RST);  // No IRQ
    DW1000.select(PIN_SS);

    // Device info
    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    // Apply LDO tuning
    Serial.println(F("Applying LDO..."));
    applyLDOTuning();

    // Read SYS_CFG
    byte sysCfg[4];
    DW1000.readBytes(SYS_CFG_REG, 0, sysCfg, 4);
    Serial.print(F("SYS_CFG: "));
    printHex(sysCfg, 4);
    Serial.println();

    // Configure
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_SHORTDATA_FAST_LOWPOWER);
    // Disable frame filtering (TX doesn't send proper 802.15.4 headers)
    DW1000.setFrameFilter(false);
    // But DO check CRC - this will help reject noise
    DW1000.suppressFrameCheck(false);
    DW1000.interruptOnReceived(true);  // Explicitly enable RX interrupt
    DW1000.interruptOnReceiveFailed(true);
    DW1000.commitConfiguration();

    // Re-apply LDO after config
    applyLDOTuning();

    // Read SYS_MASK to verify interrupt settings
    byte sysMask[4];
    DW1000.readBytes(SYS_MASK_REG, 0, sysMask, 4);
    Serial.print(F("SYS_MASK: "));
    printHex(sysMask, 4);
    Serial.println();

    // Read SYS_CFG again
    DW1000.readBytes(SYS_CFG_REG, 0, sysCfg, 4);
    Serial.print(F("SYS_CFG: "));
    printHex(sysCfg, 4);
    Serial.println();

    // Set up our own simple interrupt handler
    pinMode(PIN_IRQ, INPUT);
    Serial.print(F("IRQ pin (D2) state: "));
    Serial.println(digitalRead(PIN_IRQ));
    attachInterrupt(digitalPinToInterrupt(PIN_IRQ), simpleIRQ, RISING);

    // Clear all status
    byte clearStatus[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0, clearStatus, 5);

    // Start receiver
    Serial.println(F("\nStarting receiver..."));
    DW1000.newReceive();
    DW1000.receivePermanently(true);
    DW1000.startReceive();

    // Read status after starting RX
    byte status[5];
    DW1000.readBytes(SYS_STATUS_REG, 0, status, 5);
    Serial.print(F("Initial STATUS: "));
    printHex(status, 5);
    Serial.println();

    Serial.print(F("IRQ pin after RX start: "));
    Serial.println(digitalRead(PIN_IRQ));

    Serial.println(F("\nWaiting... (will poll status and check IRQ)"));
}

void loop() {
    static uint32_t lastPrint = 0;
    static uint32_t loopCount = 0;
    static uint32_t lastIrqCount = 0;

    loopCount++;

    // Check if IRQ fired
    if (irqFired) {
        irqFired = false;
        Serial.print(F("*** IRQ #"));
        Serial.print(irqCount);
        Serial.println(F(" fired! ***"));

        // Read status
        byte status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0, status, 5);
        Serial.print(F("  STATUS: "));
        printHex(status, 5);

        // Decode some bits
        uint32_t s = status[0] | ((uint32_t)status[1] << 8) |
                     ((uint32_t)status[2] << 16) | ((uint32_t)status[3] << 24);

        if (s & (1UL << 13)) Serial.print(F(" RXDFR"));
        if (s & (1UL << 14)) Serial.print(F(" RXFCG"));
        if (s & (1UL << 15)) Serial.print(F(" RXFCE"));
        if (s & (1UL << 16)) Serial.print(F(" RXRFSL"));
        if (s & (1UL << 17)) Serial.print(F(" RXRFTO"));
        if (s & (1UL << 10)) Serial.print(F(" RXPHE"));
        if (s & (1UL << 12)) Serial.print(F(" LDEERR"));
        Serial.println();

        // Check for valid receive
        if (s & (1UL << 14)) {  // RXFCG - Frame Check Good
            uint16_t len = DW1000.getDataLength();
            Serial.print(F("  Valid frame! len="));
            Serial.println(len);

            if (len > 0 && len < 128) {
                byte data[128];
                DW1000.getData(data, len);
                Serial.print(F("  Data: "));
                for (int i = 0; i < min((int)len, 32); i++) {
                    Serial.write(data[i] >= 32 && data[i] < 127 ? data[i] : '.');
                }
                Serial.println();
            }
        }

        // Clear status and restart receive
        byte clearStatus[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0, clearStatus, 5);
        DW1000.newReceive();
        DW1000.receivePermanently(true);
        DW1000.startReceive();
    }

    // Print status periodically
    if (millis() - lastPrint >= 3000) {
        lastPrint = millis();

        // Read status (but beware of corruption in RX mode!)
        byte status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0, status, 5);

        Serial.print(F("[t="));
        Serial.print(millis() / 1000);
        Serial.print(F("s] IRQ pin="));
        Serial.print(digitalRead(PIN_IRQ));
        Serial.print(F(" IRQ count="));
        Serial.print(irqCount);
        Serial.print(F(" STATUS: "));
        printHex(status, 5);
        Serial.println();
    }

    delay(10);
}
