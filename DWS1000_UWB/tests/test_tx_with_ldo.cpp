/**
 * TX Test with LDO Tuning Fix
 *
 * Transmitter using the original DW1000 library with the LDO tuning fix applied.
 * This should produce clean, uncorrupted transmissions now that PLL is stable.
 *
 * Upload to: /dev/ttyACM0 (DEV0)
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
uint32_t txCount = 0;
uint32_t txSuccess = 0;
uint32_t txFailed = 0;
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
    Serial.println(F("  TX TEST WITH LDO TUNING FIX"));
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

    // Configure device
    Serial.println(F("[INIT] Configuring for TX..."));
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.interruptOnSent(true);
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

    Serial.println(F(""));
    Serial.println(F("=========================================="));
    Serial.println(F("  Starting TX - sending every 2 seconds"));
    Serial.println(F("=========================================="));
    Serial.println(F(""));
}

void loop() {
    static uint32_t lastTx = 0;
    static uint32_t lastStatus = 0;

    // Print status every 10 seconds
    if (millis() - lastStatus >= 10000) {
        lastStatus = millis();
        printStatus();
    }

    // Transmit every 2 seconds
    if (millis() - lastTx < 2000) {
        delay(10);
        return;
    }
    lastTx = millis();
    txCount++;

    // Check PLL before transmitting
    if (!checkPLLStatus()) {
        pllErrors++;
        Serial.print(F("[TX #"));
        Serial.print(txCount);
        Serial.println(F("] PLL ERROR - skipping"));
        return;
    }

    // Prepare message with sequence number for verification
    char txData[32];
    snprintf(txData, sizeof(txData), "PING%04lu", txCount);

    Serial.print(F("[TX #"));
    Serial.print(txCount);
    Serial.print(F("] Sending \""));
    Serial.print(txData);
    Serial.print(F("\"... "));

    // Setup and send
    DW1000.newTransmit();
    DW1000.setDefaults();
    DW1000.setData((byte*)txData, strlen(txData) + 1);
    DW1000.startTransmit();

    // Wait for TX complete (polling)
    uint32_t start = millis();
    bool sent = false;
    while (millis() - start < 500) {
        if (DW1000.isTransmitDone()) {
            sent = true;
            break;
        }
        delay(1);
    }

    if (sent) {
        txSuccess++;
        DW1000.clearTransmitStatus();
        Serial.println(F("OK"));
    } else {
        txFailed++;
        Serial.println(F("TIMEOUT"));

        // Try to recover
        DW1000.idle();
    }
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
    Serial.println(F("--- TX Status ---"));
    Serial.print(F("Total: "));
    Serial.print(txCount);
    Serial.print(F(" | Success: "));
    Serial.print(txSuccess);
    Serial.print(F(" | Failed: "));
    Serial.print(txFailed);
    Serial.print(F(" | PLL Errors: "));
    Serial.println(pllErrors);

    if (txCount > 0) {
        float rate = 100.0 * txSuccess / txCount;
        Serial.print(F("Success rate: "));
        Serial.print(rate, 1);
        Serial.println(F("%"));
    }

    if (ldoApplied) {
        Serial.print(F("LDO: 0x"));
        Serial.print(ldoValue, HEX);
        Serial.println(F(" (applied)"));
    }
    Serial.println(F(""));
}
