/**
 * SPI_EDGE Fix Test
 *
 * Tests whether setting the SPI_EDGE bit in SYS_CFG fixes
 * SPI corruption during RX mode.
 *
 * The DW1000's default MISO hold time is only ~13-20ns after
 * the sampling edge. Setting SPI_EDGE changes the MISO launch
 * edge to make DW1000 SPI "more standard" (per Qorvo engineers).
 *
 * This test runs the same diagnostic as test_spi_diagnostic.cpp
 * but with SPI_EDGE set.
 *
 * Source: https://forum.qorvo.com/t/dwm1000-returns-wrong-vaules/5745
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

#define SYS_CFG_REG     0x04
#define SYS_STATUS_REG  0x0F
#define AON_REG         0x2C
#define AON_CTRL_SUB    0x02

// SPI_EDGE is bit 10 of SYS_CFG (register 0x04)
// Writing 0x1600 to SYS_CFG sets SPI_EDGE + HIRQ_POL (active high IRQ)
// But we should read-modify-write to preserve other config bits.

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
        Serial.print(F("  LDO tuning applied (0x"));
        Serial.print(ldoTune[0], HEX);
        Serial.println(F(")"));
    }
}

void setSPIEdge() {
    // Read current SYS_CFG
    byte sysCfg[4];
    DW1000.readBytes(SYS_CFG_REG, 0x00, sysCfg, 4);

    Serial.print(F("  SYS_CFG before: 0x"));
    for (int i = 3; i >= 0; i--) {
        if (sysCfg[i] < 16) Serial.print("0");
        Serial.print(sysCfg[i], HEX);
    }
    Serial.println();

    // Set SPI_EDGE (bit 10) = byte[1] bit 2
    sysCfg[1] |= 0x04;  // bit 10 = byte[1] bit 2

    DW1000.writeBytes(SYS_CFG_REG, 0x00, sysCfg, 4);

    // Verify
    byte verify[4];
    DW1000.readBytes(SYS_CFG_REG, 0x00, verify, 4);
    Serial.print(F("  SYS_CFG after:  0x"));
    for (int i = 3; i >= 0; i--) {
        if (verify[i] < 16) Serial.print("0");
        Serial.print(verify[i], HEX);
    }
    Serial.println();

    bool set = (verify[1] & 0x04) != 0;
    Serial.print(F("  SPI_EDGE bit: "));
    Serial.println(set ? F("SET") : F("NOT SET - FAILED!"));
}

void runStatusTest(const __FlashStringHelper* label, int count) {
    int good = 0;
    int corrupt = 0;

    Serial.print(F("--- "));
    Serial.print(label);
    Serial.print(F(" ("));
    Serial.print(count);
    Serial.println(F(" reads) ---"));

    for (int i = 0; i < count; i++) {
        byte status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

        // Check for corruption (all 0xFF = chip not responding)
        bool allFF = (status[0] == 0xFF && status[1] == 0xFF && status[2] == 0xFF);
        bool allZero = (status[0] == 0x00 && status[1] == 0x00 && status[2] == 0x00
                        && status[3] == 0x00 && status[4] == 0x00);

        // In any active state, at least CPLOCK (bit 1) should be set
        bool hasCPLOCK = (status[0] & 0x02) != 0;

        bool valid = !allFF && !allZero && hasCPLOCK;
        if (valid) good++; else corrupt++;

        // Print first 5 and last 5, plus any corrupt reads
        if (i < 5 || i >= count - 5 || !valid) {
            Serial.print(F("  ["));
            if (i < 10) Serial.print(" ");
            Serial.print(i);
            Serial.print(F("] "));
            for (int j = 0; j < 5; j++) {
                if (status[j] < 16) Serial.print("0");
                Serial.print(status[j], HEX);
                Serial.print(" ");
            }
            Serial.println(valid ? F("OK") : F("CORRUPT"));
        } else if (i == 5) {
            Serial.println(F("  ..."));
        }

        delay(10);
    }

    Serial.print(F("  Result: "));
    Serial.print(good);
    Serial.print(F("/"));
    Serial.print(count);
    Serial.print(F(" good ("));
    Serial.print((good * 100) / count);
    Serial.println(F("%)"));
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n========================================"));
    Serial.println(F("  SPI_EDGE Fix Test"));
    Serial.println(F("  Testing MISO timing improvement"));
    Serial.println(F("========================================\n"));

    // Init DW1000 WITHOUT IRQ (pure SPI test, no ISR contention)
    DW1000.begin(0xFF, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    // Apply LDO tuning
    Serial.println(F("\nStep 1: LDO Tuning"));
    applyLDOTuning();

    // Test BEFORE SPI_EDGE
    Serial.println(F("\n====== WITHOUT SPI_EDGE ======\n"));

    // Configure for RX
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setFrameFilter(false);
    DW1000.commitConfiguration();
    applyLDOTuning();

    runStatusTest(F("IDLE (no SPI_EDGE)"), 50);

    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.startReceive();
    delay(200);

    runStatusTest(F("RX MODE (no SPI_EDGE)"), 100);

    DW1000.idle();
    delay(100);

    runStatusTest(F("Back to IDLE (no SPI_EDGE)"), 50);

    // Now set SPI_EDGE and retest
    Serial.println(F("\n====== WITH SPI_EDGE ======\n"));

    Serial.println(F("Step 2: Setting SPI_EDGE bit"));
    setSPIEdge();
    Serial.println();

    // Re-apply LDO after modifying SYS_CFG
    applyLDOTuning();

    runStatusTest(F("IDLE (with SPI_EDGE)"), 50);

    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.startReceive();
    delay(200);

    runStatusTest(F("RX MODE (with SPI_EDGE)"), 100);

    DW1000.idle();
    delay(100);

    runStatusTest(F("Back to IDLE (with SPI_EDGE)"), 50);

    Serial.println(F("\n========================================"));
    Serial.println(F("  Test Complete"));
    Serial.println(F("  Compare RX MODE results above"));
    Serial.println(F("========================================"));
}

void loop() {
    delay(5000);
    Serial.println(F("[idle]"));
}
