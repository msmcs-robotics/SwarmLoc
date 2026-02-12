/**
 * RX Test with All Software Fixes Applied
 *
 * Combines every known software mitigation:
 * 1. SPI_EDGE bit set (extends MISO hold time)
 * 2. LDO tuning from OTP applied
 * 3. IRQ-driven reception (no polling during active RX)
 * 4. TRXOFF before reading data (chip in IDLE during SPI reads)
 * 5. Small delay between IRQ and SPI reads
 * 6. Corruption detection and retry
 *
 * Pair with test_tx_irq.cpp on the other device.
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

// Stats
volatile bool irqFired = false;
uint32_t rxAttempts = 0;
uint32_t rxGood = 0;
uint32_t rxCorrupt = 0;
uint32_t rxTimeout = 0;
uint32_t rxCRCFail = 0;
uint32_t spiRetries = 0;

// Simple ISR - just sets flag, no SPI in interrupt context
void onIRQ() {
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
        Serial.print(F("  LDO: 0x"));
        Serial.println(ldoTune[0], HEX);
    }
}

void setSPIEdge() {
    byte sysCfg[4];
    DW1000.readBytes(SYS_CFG_REG, 0x00, sysCfg, 4);
    sysCfg[1] |= 0x04;  // SPI_EDGE = bit 10 = byte[1] bit 2
    DW1000.writeBytes(SYS_CFG_REG, 0x00, sysCfg, 4);

    // Verify
    byte verify[4];
    DW1000.readBytes(SYS_CFG_REG, 0x00, verify, 4);
    bool ok = (verify[1] & 0x04) != 0;
    Serial.print(F("  SPI_EDGE: "));
    Serial.println(ok ? F("SET") : F("FAILED"));
}

// Read status with corruption check and retry
uint32_t readStatusSafe() {
    for (int attempt = 0; attempt < 3; attempt++) {
        byte status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

        // Check for all-0xFF corruption
        if (status[0] == 0xFF && status[1] == 0xFF && status[2] == 0xFF) {
            spiRetries++;
            delayMicroseconds(100);  // Brief pause before retry
            continue;
        }

        // Assemble 32-bit status (lower 4 bytes)
        uint32_t s = ((uint32_t)status[3] << 24) |
                     ((uint32_t)status[2] << 16) |
                     ((uint32_t)status[1] << 8) |
                     (uint32_t)status[0];
        return s;
    }
    return 0xFFFFFFFF;  // All retries failed
}

void startReceiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.startReceive();
    irqFired = false;
}

void printStats() {
    Serial.print(F("  [Stats] good="));
    Serial.print(rxGood);
    Serial.print(F(" corrupt="));
    Serial.print(rxCorrupt);
    Serial.print(F(" crc_fail="));
    Serial.print(rxCRCFail);
    Serial.print(F(" timeout="));
    Serial.print(rxTimeout);
    Serial.print(F(" spi_retries="));
    Serial.println(spiRetries);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n========================================"));
    Serial.println(F("  RX Test - All Software Fixes"));
    Serial.println(F("  SPI_EDGE + LDO + TRXOFF + delay"));
    Serial.println(F("========================================\n"));

    // Initialize WITHOUT library IRQ handler
    // We'll attach our own minimal ISR
    DW1000.begin(0xFF, PIN_RST);
    DW1000.select(PIN_SS);

    char msg[64];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: "));
    Serial.println(msg);

    // Step 1: LDO tuning
    Serial.println(F("\nApplying fixes:"));
    applyLDOTuning();

    // Step 2: SPI_EDGE
    setSPIEdge();

    // Step 3: Configure radio
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.setFrameFilter(false);
    DW1000.commitConfiguration();

    // Re-apply fixes after commitConfiguration resets them
    applyLDOTuning();
    setSPIEdge();

    // Step 4: Enable interrupt mask for RX events
    DW1000.interruptOnReceived(true);
    DW1000.interruptOnReceiveFailed(true);
    DW1000.interruptOnReceiveTimeout(true);

    // Step 5: Attach minimal ISR (just sets flag)
    pinMode(PIN_IRQ, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_IRQ), onIRQ, RISING);

    // Verify SPI is clean before starting RX
    Serial.println(F("\nPre-RX SPI check:"));
    for (int i = 0; i < 5; i++) {
        uint32_t s = readStatusSafe();
        Serial.print(F("  STATUS: 0x"));
        Serial.println(s, HEX);
    }

    Serial.println(F("\nStarting receiver..."));
    Serial.println(F("Pair with test_tx_irq.cpp on other device\n"));
    startReceiver();
}

void loop() {
    if (irqFired) {
        irqFired = false;
        rxAttempts++;

        // KEY FIX: Force chip to IDLE before any SPI reads
        // This stops the receiver and ensures the chip's SPI
        // interface is not contending with RX processing
        DW1000.idle();

        // Small delay for chip to settle
        delayMicroseconds(50);

        // Now read status safely
        uint32_t status = readStatusSafe();

        if (status == 0xFFFFFFFF) {
            // SPI still corrupt even after TRXOFF + retries
            rxCorrupt++;
            Serial.print(F("["));
            Serial.print(rxAttempts);
            Serial.println(F("] SPI CORRUPT (all retries failed)"));
        } else {
            // Check RX status bits
            bool rxDone = (status >> 13) & 1;  // RXDFR
            bool rxGoodCRC = (status >> 14) & 1;  // RXFCG
            bool rxBadCRC = (status >> 15) & 1;  // RXFCE
            bool rxTimeout_bit = (status >> 17) & 1;  // RXRFTO
            bool rxError = (status >> 12) & 1;  // RXPHE

            if (rxGoodCRC) {
                // Good frame received - read data
                uint16_t len = DW1000.getDataLength();

                // Sanity check length
                if (len > 0 && len <= 127) {
                    byte data[128];
                    DW1000.getData(data, len);

                    rxGood++;
                    Serial.print(F("["));
                    Serial.print(rxAttempts);
                    Serial.print(F("] RX OK len="));
                    Serial.print(len);
                    Serial.print(F(" data=\""));
                    // Print as ASCII if printable
                    for (int i = 0; i < len && i < 32; i++) {
                        if (data[i] >= 32 && data[i] < 127) {
                            Serial.print((char)data[i]);
                        } else {
                            Serial.print('.');
                        }
                    }
                    Serial.println(F("\""));
                } else {
                    rxCorrupt++;
                    Serial.print(F("["));
                    Serial.print(rxAttempts);
                    Serial.print(F("] BAD LEN="));
                    Serial.print(len);
                    Serial.print(F(" status=0x"));
                    Serial.println(status, HEX);
                }
            } else if (rxBadCRC) {
                rxCRCFail++;
                Serial.print(F("["));
                Serial.print(rxAttempts);
                Serial.println(F("] CRC FAIL"));
            } else if (rxTimeout_bit) {
                rxTimeout++;
                // Don't print timeouts to avoid spam
            } else if (rxError) {
                rxCorrupt++;
                Serial.print(F("["));
                Serial.print(rxAttempts);
                Serial.print(F("] RX ERROR status=0x"));
                Serial.println(status, HEX);
            } else {
                // Unexpected status
                Serial.print(F("["));
                Serial.print(rxAttempts);
                Serial.print(F("] UNKNOWN status=0x"));
                Serial.println(status, HEX);
            }
        }

        // Clear all status bits
        byte clearAll[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DW1000.writeBytes(SYS_STATUS_REG, 0x00, clearAll, 5);

        // Restart receiver
        startReceiver();
    }

    // Periodic stats (every 10 seconds)
    static uint32_t lastStats = 0;
    if (millis() - lastStats >= 10000) {
        lastStats = millis();
        printStats();
    }
}
