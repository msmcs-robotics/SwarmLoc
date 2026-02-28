/**
 * RX Test v5 - Library-Managed IRQ Callbacks
 *
 * Uses the DW1000 library's built-in interrupt handling:
 *   - attachReceivedHandler() for good frames
 *   - attachReceiveFailedHandler() for errors
 *
 * Since TX callbacks work perfectly (30/30), this tests whether
 * the same library mechanism works for RX events.
 *
 * The library's setDefaults() enables:
 *   - interruptOnReceived(true)   → RXDFR + RXFCG bits in SYS_MASK
 *   - interruptOnReceiveFailed(true) → LDEERR + RXFCE + RXPHE + RXRFSL
 *   - setReceiverAutoReenable(true) → auto-restart after receive
 *
 * Upload to: receiver device while TX runs on other device
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02
#define SYS_MASK_REG    0x0E
#define SYS_STATUS_REG  0x0F

// Stats
volatile uint32_t rxGood = 0;
volatile uint32_t rxFailed = 0;
volatile bool newFrame = false;
volatile bool newError = false;

void handleReceived() {
    rxGood++;
    newFrame = true;
}

void handleReceiveFailed() {
    rxFailed++;
    newError = true;
}

void handleError() {
    // PLL sticky bits — ignore
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

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== RX Library IRQ Test v5 ==="));

    // Initialize WITH real IRQ pin (library attaches ISR)
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    applyLDOTuning();

    // Configure - match TX settings exactly
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

    // Attach callbacks — EXACTLY like TX test does for its handlers
    DW1000.attachReceivedHandler(handleReceived);
    DW1000.attachReceiveFailedHandler(handleReceiveFailed);

    // Verify SYS_MASK
    byte maskRead[4];
    DW1000.readBytes(SYS_MASK_REG, 0x00, maskRead, 4);
    uint32_t m = (uint32_t)maskRead[0] | ((uint32_t)maskRead[1] << 8) |
                 ((uint32_t)maskRead[2] << 16) | ((uint32_t)maskRead[3] << 24);
    Serial.print(F("SYS_MASK: 0x"));
    Serial.println(m, HEX);

    // Check IRQ pin state
    Serial.print(F("IRQ pin: "));
    Serial.println(digitalRead(PIN_IRQ) ? "HIGH" : "LOW");

    // Start receiving — use permanent receive mode (auto-reenable)
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();

    Serial.println(F("Listening for frames...\n"));
}

void loop() {
    static uint32_t lastReport = 0;

    // Check for new frame (set by ISR callback)
    if (newFrame) {
        newFrame = false;

        // Read data
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
    }

    // Check for error
    if (newError) {
        newError = false;
        Serial.print(F("[ERR #"));
        Serial.print(rxFailed);
        Serial.println(F("]"));
    }

    // Periodic report every 5s
    if (millis() - lastReport >= 5000) {
        lastReport = millis();

        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] G:"));
        Serial.print(rxGood);
        Serial.print(F(" F:"));
        Serial.print(rxFailed);
        Serial.print(F(" pin="));
        Serial.println(digitalRead(PIN_IRQ) ? "HIGH" : "LOW");
    }

    delay(10);
}
