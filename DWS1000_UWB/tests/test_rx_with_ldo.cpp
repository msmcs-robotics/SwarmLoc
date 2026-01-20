/**
 * RX Test with LDO Tuning Fix
 *
 * Receiver using the original DW1000 library with the LDO tuning fix applied.
 * This should receive clean, uncorrupted data now that PLL is stable.
 *
 * Upload to: /dev/ttyACM1 (DEV1)
 *
 * Hardware: DWS1000 shield on Arduino Uno
 * Config: NO J1 jumper, D8->D2 wire for IRQ
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;  // D10

// Register addresses
#define SYS_STATUS_REG  0x0F
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// Counters
uint32_t rxCount = 0;
uint32_t rxGood = 0;
uint32_t rxCorrupted = 0;
uint32_t rxErrors = 0;
uint32_t pllErrors = 0;

// LDO status
bool ldoApplied = false;
uint8_t ldoValue = 0;

// Forward declarations
void applyLDOTuning();
bool checkPLLStatus();
void printStatus();

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    Serial.println(F(""));
    Serial.println(F("=========================================="));
    Serial.println(F("  RX TEST WITH LDO TUNING FIX"));
    Serial.println(F("=========================================="));
    Serial.println(F(""));

    // Initialize DW1000
    Serial.println(F("[INIT] Initializing DW1000..."));
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Verify device
    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("  Device ID: "));
    Serial.println(msg);

    if (strstr(msg, "DECA") == NULL) {
        Serial.println(F("[FAIL] DW1000 not detected!"));
        while (1) delay(1000);
    }

    // Apply LDO tuning fix (THE CRITICAL FIX!)
    Serial.println(F("[INIT] Applying LDO tuning from OTP..."));
    applyLDOTuning();

    // Configure device - must match TX settings!
    Serial.println(F("[INIT] Configuring for RX..."));
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setReceiverAutoReenable(true);
    DW1000.interruptOnReceived(true);
    DW1000.commitConfiguration();

    DW1000.getPrintableDeviceMode(msg);
    Serial.print(F("  Mode: "));
    Serial.println(msg);

    // Check initial PLL status
    Serial.println(F("[INIT] Checking PLL status..."));
    if (checkPLLStatus()) {
        Serial.println(F("  PLL: LOCKED (good!)"));
    } else {
        Serial.println(F("  PLL: UNLOCKED (warning!)"));
    }

    // Start receiving
    Serial.println(F("[INIT] Starting receiver..."));
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();

    Serial.println(F(""));
    Serial.println(F("=========================================="));
    Serial.println(F("  Waiting for transmissions..."));
    Serial.println(F("  Expected format: PING0001, PING0002, ..."));
    Serial.println(F("=========================================="));
    Serial.println(F(""));
}

void loop() {
    static uint32_t lastStatus = 0;
    static uint32_t lastPllCheck = 0;

    // Check PLL status periodically
    if (millis() - lastPllCheck >= 1000) {
        lastPllCheck = millis();
        if (!checkPLLStatus()) {
            pllErrors++;
        }
    }

    // Print status every 10 seconds
    if (millis() - lastStatus >= 10000) {
        lastStatus = millis();
        printStatus();
    }

    // Check for received data
    if (DW1000.isReceiveDone()) {
        rxCount++;

        uint16_t len = DW1000.getDataLength();

        Serial.print(F("[RX #"));
        Serial.print(rxCount);
        Serial.print(F("] len="));
        Serial.print(len);

        if (len > 0 && len < 128) {
            byte rxData[128];
            memset(rxData, 0, sizeof(rxData));
            DW1000.getData(rxData, len);

            // Print as hex
            Serial.print(F(" hex="));
            for (int i = 0; i < min((int)len, 16); i++) {
                if (rxData[i] < 16) Serial.print("0");
                Serial.print(rxData[i], HEX);
                Serial.print(" ");
            }

            // Print as string
            Serial.print(F(" str=\""));
            for (int i = 0; i < min((int)len, 20); i++) {
                char c = rxData[i];
                if (c >= 32 && c < 127) {
                    Serial.print(c);
                } else if (c == 0) {
                    break;
                } else {
                    Serial.print('.');
                }
            }
            Serial.print(F("\""));

            // Verify data integrity - check if it starts with "PING"
            bool valid = (len >= 8) &&
                         (rxData[0] == 'P') &&
                         (rxData[1] == 'I') &&
                         (rxData[2] == 'N') &&
                         (rxData[3] == 'G');

            if (valid) {
                // Check if the number portion is valid
                bool numValid = true;
                for (int i = 4; i < 8; i++) {
                    if (rxData[i] < '0' || rxData[i] > '9') {
                        numValid = false;
                        break;
                    }
                }
                if (numValid) {
                    rxGood++;
                    Serial.print(F(" [VALID]"));
                } else {
                    rxCorrupted++;
                    Serial.print(F(" [CORRUPT-NUM]"));
                }
            } else {
                rxCorrupted++;
                Serial.print(F(" [CORRUPT]"));
            }
        } else {
            rxCorrupted++;
            Serial.print(F(" [BAD-LEN]"));
        }

        Serial.println();

        // Clear and restart receiver
        DW1000.clearReceiveStatus();
    }

    // Check for receive errors
    if (DW1000.isReceiveFailed()) {
        rxErrors++;
        Serial.println(F("[ERROR] Receive failed"));

        DW1000.clearReceiveStatus();
        DW1000.idle();
        delay(1);
        DW1000.newReceive();
        DW1000.setDefaults();
        DW1000.receivePermanently(true);
        DW1000.startReceive();
    }

    delay(10);
}

void applyLDOTuning() {
    // Read LDO tune value from OTP address 0x04
    byte ldoTune[4];
    DW1000.readBytesOTP(0x04, ldoTune);

    ldoValue = ldoTune[0];
    Serial.print(F("  OTP LDO value: 0x"));
    Serial.println(ldoValue, HEX);

    if (ldoValue != 0 && ldoValue != 0xFF) {
        // Apply LDO tuning by setting OTP_LDO bit in AON_CTRL
        byte aonCtrl[4];
        DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);

        // Set OTP_LDO bit (bit 6)
        aonCtrl[0] |= 0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);

        delay(1);  // Wait for transfer

        // Clear the bit
        aonCtrl[0] &= ~0x40;
        DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);

        ldoApplied = true;
        Serial.println(F("  LDO tuning APPLIED!"));
    } else {
        Serial.println(F("  No valid LDO value in OTP"));
    }
}

bool checkPLLStatus() {
    byte status[5];
    DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

    bool cplock = status[0] & 0x02;       // Bit 1
    bool rfpllLL = status[3] & 0x01;      // Bit 24
    bool clkpllLL = status[3] & 0x02;     // Bit 25

    // Clear error flags if set
    if (rfpllLL || clkpllLL) {
        byte clearFlags[4] = {0, 0, 0, 0x03};
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearFlags, 4);
    }

    return cplock && !rfpllLL && !clkpllLL;
}

void printStatus() {
    Serial.println(F(""));
    Serial.println(F("--- RX Status ---"));
    Serial.print(F("Total RX: "));
    Serial.print(rxCount);
    Serial.print(F(" | Valid: "));
    Serial.print(rxGood);
    Serial.print(F(" | Corrupted: "));
    Serial.print(rxCorrupted);
    Serial.print(F(" | Errors: "));
    Serial.print(rxErrors);
    Serial.print(F(" | PLL: "));
    Serial.println(pllErrors);

    if (rxCount > 0) {
        float rate = 100.0 * rxGood / rxCount;
        Serial.print(F("Valid data rate: "));
        Serial.print(rate, 1);
        Serial.println(F("%"));
    }

    if (ldoApplied) {
        Serial.print(F("LDO: 0x"));
        Serial.print(ldoValue, HEX);
        Serial.println(F(" (applied)"));
    }

    // Check current PLL status
    if (checkPLLStatus()) {
        Serial.println(F("PLL: LOCKED"));
    } else {
        Serial.println(F("PLL: UNLOCKED!"));
    }
    Serial.println(F(""));
}
