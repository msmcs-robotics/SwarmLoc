/**
 * RX Test - Callback Only (No Polling)
 *
 * Uses the library's interrupt handler exclusively.
 * Never polls status register during RX to avoid SPI corruption.
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Register addresses
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Stats
volatile uint32_t rxGood = 0;
volatile uint32_t rxFailed = 0;
volatile uint32_t rxTimeout = 0;
volatile uint32_t rxError = 0;
volatile bool newFrame = false;

// Data buffer
#define RX_BUF_SIZE 128
volatile byte rxBuffer[RX_BUF_SIZE];
volatile uint16_t rxLength = 0;

// Callbacks - called from ISR
void handleReceived() {
    rxLength = DW1000.getDataLength();
    if (rxLength > 0 && rxLength <= RX_BUF_SIZE) {
        DW1000.getData((byte*)rxBuffer, rxLength);
        rxGood++;
        newFrame = true;
    }
}

void handleReceiveFailed() {
    rxFailed++;
    // Try to get the length and data of the failed frame
    uint16_t len = DW1000.getDataLength();
    Serial.print(F("[RX FAILED] len="));
    Serial.print(len);

    if (len > 0 && len <= RX_BUF_SIZE) {
        byte buf[RX_BUF_SIZE];
        DW1000.getData(buf, len);
        Serial.print(F(" data="));
        for (int i = 0; i < min((int)len, 20); i++) {
            if (buf[i] < 16) Serial.print("0");
            Serial.print(buf[i], HEX);
            Serial.print(" ");
        }
    }
    Serial.println();
}

void handleReceiveTimeout() {
    rxTimeout++;
}

void handleError() {
    rxError++;
    Serial.println(F("[RX ERROR] Clock/PLL issue"));
}

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

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Callback Only Test ==="));
    Serial.println(F("No status polling - IRQ handler only"));

    // Initialize WITH library IRQ handler
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Device info
    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    // Configure
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    // Use long range mode - 110kbps, 2048 preamble, more robust
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setFrameFilter(false);
    DW1000.suppressFrameCheck(false);  // Keep CRC check enabled
    DW1000.commitConfiguration();

    applyLDOTuning();

    // Print mode
    DW1000.getPrintableDeviceMode(msg);
    Serial.print(F("Mode: "));
    Serial.println(msg);

    // Attach callbacks
    DW1000.attachReceivedHandler(handleReceived);
    DW1000.attachReceiveFailedHandler(handleReceiveFailed);
    DW1000.attachReceiveTimeoutHandler(handleReceiveTimeout);
    DW1000.attachErrorHandler(handleError);

    // Start receiver in permanent mode
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();

    Serial.println(F("Receiver started"));
    Serial.println(F("Waiting for frames from TX..."));
    Serial.println();
}

void loop() {
    // Check for new frame (set by ISR)
    if (newFrame) {
        newFrame = false;

        Serial.print(F("[RX #"));
        Serial.print(rxGood);
        Serial.print(F("] len="));
        Serial.print(rxLength);
        Serial.print(F(" \""));

        // Print data
        for (uint16_t i = 0; i < min(rxLength, (uint16_t)32); i++) {
            char c = rxBuffer[i];
            Serial.write((c >= 32 && c < 127) ? c : '.');
        }
        Serial.println(F("\""));
    }

    // Periodic stats (no status polling!)
    static uint32_t lastStats = 0;
    if (millis() - lastStats >= 5000) {
        lastStats = millis();

        Serial.print(F("[Stats t="));
        Serial.print(millis() / 1000);
        Serial.print(F("s] Good:"));
        Serial.print(rxGood);
        Serial.print(F(" Failed:"));
        Serial.print(rxFailed);
        Serial.print(F(" Timeout:"));
        Serial.print(rxTimeout);
        Serial.print(F(" Error:"));
        Serial.println(rxError);
    }

    delay(10);
}
