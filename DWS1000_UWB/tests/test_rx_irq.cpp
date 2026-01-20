/**
 * RX Test with IRQ-based Reception
 *
 * Uses the DW1000 library's interrupt handler and callbacks
 * instead of polling, to avoid SPI corruption during RX mode.
 *
 * Key insight: SPI reads are corrupted when polling during RX,
 * but the IRQ handler reads status correctly because it only
 * triggers when the DW1000 has finished a receive operation.
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
volatile uint32_t rxCount = 0;
volatile uint32_t rxGood = 0;
volatile uint32_t rxFailed = 0;
volatile uint32_t rxTimeout = 0;
volatile bool newData = false;

// Receive buffer
#define MAX_RX_LEN 128
volatile byte rxData[MAX_RX_LEN];
volatile uint16_t rxLen = 0;

// Callback handlers - called from ISR context
void handleReceived() {
    rxCount++;
    rxLen = DW1000.getDataLength();

    if (rxLen > 0 && rxLen <= MAX_RX_LEN) {
        DW1000.getData((byte*)rxData, rxLen);
        rxGood++;
        newData = true;
    } else {
        rxFailed++;
    }
}

void handleReceiveFailed() {
    rxFailed++;
}

void handleReceiveTimeout() {
    rxTimeout++;
}

void handleError() {
    // Clock/PLL error
    Serial.println(F("[IRQ: Error]"));
}

void applyLDOTuning() {
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);

    if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        aonCtrl[0] |= 0x40;  // Set OTP_LDO bit
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
        delay(1);
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Test with IRQ Callbacks ==="));
    Serial.println(F("Using library interrupt handler"));

    // Initialize DW1000 WITH IRQ (don't use 0xFF)
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Device info
    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    // Apply LDO tuning
    Serial.println(F("Applying LDO tuning..."));
    applyLDOTuning();

    // Configure
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_SHORTDATA_FAST_LOWPOWER);
    DW1000.setFrameFilter(false);  // Accept all frames for now
    DW1000.suppressFrameCheck(false);  // Check CRC
    DW1000.commitConfiguration();

    // Re-apply LDO after config
    applyLDOTuning();

    // Attach callbacks
    DW1000.attachReceivedHandler(handleReceived);
    DW1000.attachReceiveFailedHandler(handleReceiveFailed);
    DW1000.attachReceiveTimeoutHandler(handleReceiveTimeout);
    DW1000.attachErrorHandler(handleError);

    // Start receiver
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);  // Auto-restart after each receive
    DW1000.startReceive();

    Serial.println(F("Receiver started with permanent mode"));
    Serial.println(F("Waiting for packets..."));
    Serial.println();
}

void loop() {
    // Check for new data (set by ISR)
    if (newData) {
        newData = false;

        Serial.print(F("RX #"));
        Serial.print(rxGood);
        Serial.print(F(" len="));
        Serial.print(rxLen);
        Serial.print(F(" data=\""));

        // Print as string if printable, else hex
        bool printable = true;
        for (int i = 0; i < min((int)rxLen, 20); i++) {
            if (rxData[i] < 32 || rxData[i] > 126) {
                printable = false;
                break;
            }
        }

        if (printable && rxLen < 64) {
            for (int i = 0; i < rxLen; i++) {
                Serial.write((char)rxData[i]);
            }
        } else {
            for (int i = 0; i < min((int)rxLen, 16); i++) {
                if (rxData[i] < 16) Serial.print("0");
                Serial.print(rxData[i], HEX);
                Serial.print(" ");
            }
            if (rxLen > 16) Serial.print("...");
        }
        Serial.println(F("\""));
    }

    // Periodic status report
    static uint32_t lastReport = 0;
    if (millis() - lastReport > 5000) {
        lastReport = millis();

        Serial.print(F("[Stats] RX:"));
        Serial.print(rxCount);
        Serial.print(F(" Good:"));
        Serial.print(rxGood);
        Serial.print(F(" Failed:"));
        Serial.print(rxFailed);
        Serial.print(F(" Timeout:"));
        Serial.println(rxTimeout);
    }

    delay(10);  // Small delay to reduce power consumption
}
