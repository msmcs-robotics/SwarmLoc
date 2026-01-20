/**
 * Test: LDO Tuning Fix for RFPLL_LL Issue
 *
 * This test implements the missing LDO tuning from OTP that both
 * DW1000 and DW1000-ng libraries have as TODO but never implemented.
 *
 * Hardware: DWS1000 shield on Arduino Uno
 * Config: NO J1 jumper, D8->D2 wire for IRQ
 *
 * Build: pio run -e uno
 * Upload: pio run -e uno -t upload
 */

#include <Arduino.h>
#include <SPI.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;  // D10

// DW1000 Register addresses
#define DEV_ID      0x00
#define SYS_STATUS  0x0F
#define PMSC        0x36
#define OTP_IF      0x2D
#define AON         0x2C
#define FS_CTRL     0x2B
#define EXT_SYNC    0x24

// Sub-register offsets
#define PMSC_CTRL0_SUB     0x00
#define OTP_ADDR_SUB       0x04
#define OTP_CTRL_SUB       0x06
#define OTP_RDAT_SUB       0x0A
#define AON_CTRL_SUB       0x02
#define FS_PLLCFG_SUB      0x07
#define FS_PLLTUNE_SUB     0x0B
#define FS_XTALT_SUB       0x0E
#define EC_CTRL_SUB        0x00

// Status bits
#define CPLOCK_BIT    1   // Bit 1 in SYS_STATUS byte 0
#define RFPLL_LL_BIT  24  // Bit 24 (byte 3, bit 0)
#define CLKPLL_LL_BIT 25  // Bit 25 (byte 3, bit 1)

// Clock modes
#define AUTO_CLOCK  0x00
#define XTI_CLOCK   0x01
#define PLL_CLOCK   0x02

// OTP addresses
#define OTP_LDOTUNE_ADDR 0x04
#define OTP_XTALT_ADDR   0x1E

// SPI settings
SPISettings slowSPI(2000000, MSBFIRST, SPI_MODE0);   // 2 MHz during init
SPISettings fastSPI(8000000, MSBFIRST, SPI_MODE0);   // 8 MHz normal operation

// Forward declarations
void readBytes(uint8_t reg, uint16_t sub, uint8_t* data, uint16_t len);
void writeBytes(uint8_t reg, uint16_t sub, uint8_t* data, uint16_t len);
uint32_t readOTP(uint16_t addr);
void enableClock(uint8_t clock);
void applyLDOTuning();
void applyXTALTrim();
void configurePLL();
void hardReset();
void printStatus();

// Global state
bool ldoTuningApplied = false;
uint8_t ldoTuneValue = 0;
uint8_t xtalTrimValue = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    Serial.println(F(""));
    Serial.println(F("========================================"));
    Serial.println(F("  LDO TUNING FIX TEST"));
    Serial.println(F("  Testing missing OTP LDO application"));
    Serial.println(F("========================================"));
    Serial.println(F(""));

    // Initialize SPI
    SPI.begin();
    pinMode(PIN_SS, OUTPUT);
    digitalWrite(PIN_SS, HIGH);
    pinMode(PIN_RST, INPUT);  // Hi-Z (external pull-up)
    pinMode(PIN_IRQ, INPUT);

    // Step 1: Hard reset with extended delay
    Serial.println(F("[INIT] Step 1: Hard reset with 10ms delay"));
    hardReset();

    // Step 2: Verify device ID
    Serial.println(F("[INIT] Step 2: Reading device ID"));
    SPI.beginTransaction(slowSPI);
    uint8_t devId[4];
    readBytes(DEV_ID, 0x00, devId, 4);
    SPI.endTransaction();

    uint32_t id = devId[0] | (devId[1] << 8) | (devId[2] << 16) | ((uint32_t)devId[3] << 24);
    Serial.print(F("  Device ID: 0x"));
    Serial.println(id, HEX);

    if (id != 0xDECA0130) {
        Serial.println(F("[ERROR] Invalid device ID! Check connections."));
        while (1) delay(1000);
    }
    Serial.println(F("  OK - DW1000 detected"));

    // Step 3: Enable XTI clock for safe initialization
    Serial.println(F("[INIT] Step 3: Enable XTI clock"));
    SPI.beginTransaction(slowSPI);
    enableClock(XTI_CLOCK);
    SPI.endTransaction();
    delay(10);  // Extended delay

    // Step 4: Read OTP LDO tuning value
    Serial.println(F("[INIT] Step 4: Reading OTP LDO tuning"));
    SPI.beginTransaction(slowSPI);
    uint32_t ldoOTP = readOTP(OTP_LDOTUNE_ADDR);
    SPI.endTransaction();

    ldoTuneValue = ldoOTP & 0xFF;
    Serial.print(F("  OTP LDO Tune Raw: 0x"));
    Serial.println(ldoOTP, HEX);
    Serial.print(F("  LDO Tune Value: 0x"));
    Serial.println(ldoTuneValue, HEX);

    // Step 5: Apply LDO tuning (THE MISSING FIX!)
    if (ldoTuneValue != 0 && ldoTuneValue != 0xFF) {
        Serial.println(F("[INIT] Step 5: APPLYING LDO TUNING (missing in libraries!)"));
        SPI.beginTransaction(slowSPI);
        applyLDOTuning();
        SPI.endTransaction();
        ldoTuningApplied = true;
        Serial.println(F("  LDO tuning applied successfully"));
    } else {
        Serial.println(F("[INIT] Step 5: No valid LDO tune value in OTP"));
        Serial.println(F("  Skipping LDO tuning"));
    }

    // Step 6: Read and apply XTAL trim
    Serial.println(F("[INIT] Step 6: Reading OTP XTAL trim"));
    SPI.beginTransaction(slowSPI);
    uint32_t xtalOTP = readOTP(OTP_XTALT_ADDR);
    SPI.endTransaction();

    xtalTrimValue = xtalOTP & 0x1F;
    Serial.print(F("  OTP XTAL Trim: 0x"));
    Serial.println(xtalTrimValue, HEX);

    Serial.println(F("[INIT] Step 7: Applying XTAL trim"));
    SPI.beginTransaction(slowSPI);
    applyXTALTrim();
    SPI.endTransaction();

    // Step 8: Enable CPLL lock detection
    Serial.println(F("[INIT] Step 8: Enable CPLL lock detection"));
    SPI.beginTransaction(slowSPI);
    uint8_t ecctrl[4] = {0};
    readBytes(EXT_SYNC, EC_CTRL_SUB, ecctrl, 4);
    ecctrl[0] |= 0x04;  // PLLLDT bit
    writeBytes(EXT_SYNC, EC_CTRL_SUB, ecctrl, 4);
    SPI.endTransaction();

    // Step 9: Configure PLL for Channel 5
    Serial.println(F("[INIT] Step 9: Configure PLL (Channel 5)"));
    SPI.beginTransaction(slowSPI);
    configurePLL();
    SPI.endTransaction();
    delay(5);

    // Step 10: Switch to AUTO clock
    Serial.println(F("[INIT] Step 10: Switch to AUTO clock"));
    SPI.beginTransaction(slowSPI);
    enableClock(AUTO_CLOCK);
    SPI.endTransaction();
    delay(10);  // Extended delay for PLL lock

    // Step 11: Check initial status
    Serial.println(F("[INIT] Step 11: Initial status check"));
    printStatus();

    Serial.println(F(""));
    Serial.println(F("========================================"));
    Serial.println(F("  INIT COMPLETE - Monitoring PLL"));
    Serial.println(F("========================================"));
    Serial.println(F(""));
}

void loop() {
    static uint32_t lastPrint = 0;
    static uint32_t goodCount = 0;
    static uint32_t badCount = 0;

    // Check status every 500ms
    if (millis() - lastPrint >= 500) {
        lastPrint = millis();

        SPI.beginTransaction(fastSPI);
        uint8_t status[5];
        readBytes(SYS_STATUS, 0x00, status, 5);
        SPI.endTransaction();

        bool cplock = status[0] & (1 << CPLOCK_BIT);
        bool rfpllLL = status[3] & 0x01;  // Bit 24
        bool clkpllLL = status[3] & 0x02; // Bit 25

        if (cplock && !rfpllLL && !clkpllLL) {
            goodCount++;
            Serial.print(F("[GOOD] "));
        } else {
            badCount++;
            Serial.print(F("[BAD]  "));
        }

        Serial.print(F("CPLOCK="));
        Serial.print(cplock ? "1" : "0");
        Serial.print(F(" RFPLL_LL="));
        Serial.print(rfpllLL ? "1" : "0");
        Serial.print(F(" CLKPLL_LL="));
        Serial.print(clkpllLL ? "1" : "0");

        Serial.print(F(" | Good:"));
        Serial.print(goodCount);
        Serial.print(F(" Bad:"));
        Serial.print(badCount);

        if (ldoTuningApplied) {
            Serial.print(F(" [LDO=0x"));
            Serial.print(ldoTuneValue, HEX);
            Serial.print(F("]"));
        }

        Serial.println();

        // If PLL errors detected, try recovery
        if (rfpllLL || clkpllLL) {
            Serial.println(F("[RECOVERY] Attempting PLL recovery..."));

            // Clear error flags
            SPI.beginTransaction(slowSPI);
            uint8_t clearFlags[4] = {0, 0, 0, 0x03};
            writeBytes(SYS_STATUS, 0x00, clearFlags, 4);

            // Re-apply PLL configuration
            enableClock(XTI_CLOCK);
            delay(5);
            configurePLL();
            delay(5);
            enableClock(AUTO_CLOCK);
            delay(10);
            SPI.endTransaction();

            Serial.println(F("[RECOVERY] PLL re-tuned"));
        }
    }
}

void hardReset() {
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(2);
    pinMode(PIN_RST, INPUT);  // Hi-Z
    delay(10);  // Extended delay for PLL stabilization
}

void readBytes(uint8_t reg, uint16_t sub, uint8_t* data, uint16_t len) {
    uint8_t header[3];
    uint8_t headerLen = 1;

    if (sub > 0) {
        header[0] = 0x40 | reg;  // Read with sub-address
        if (sub < 128) {
            header[1] = sub;
            headerLen = 2;
        } else {
            header[1] = 0x80 | (sub & 0x7F);
            header[2] = (sub >> 7) & 0xFF;
            headerLen = 3;
        }
    } else {
        header[0] = reg;  // Simple read
    }

    digitalWrite(PIN_SS, LOW);
    for (int i = 0; i < headerLen; i++) {
        SPI.transfer(header[i]);
    }
    for (int i = 0; i < len; i++) {
        data[i] = SPI.transfer(0x00);
    }
    digitalWrite(PIN_SS, HIGH);
}

void writeBytes(uint8_t reg, uint16_t sub, uint8_t* data, uint16_t len) {
    uint8_t header[3];
    uint8_t headerLen = 1;

    if (sub > 0) {
        header[0] = 0xC0 | reg;  // Write with sub-address
        if (sub < 128) {
            header[1] = sub;
            headerLen = 2;
        } else {
            header[1] = 0x80 | (sub & 0x7F);
            header[2] = (sub >> 7) & 0xFF;
            headerLen = 3;
        }
    } else {
        header[0] = 0x80 | reg;  // Simple write
    }

    digitalWrite(PIN_SS, LOW);
    for (int i = 0; i < headerLen; i++) {
        SPI.transfer(header[i]);
    }
    for (int i = 0; i < len; i++) {
        SPI.transfer(data[i]);
    }
    digitalWrite(PIN_SS, HIGH);
}

uint32_t readOTP(uint16_t addr) {
    // Write OTP address
    uint8_t addrBytes[2] = {(uint8_t)(addr & 0xFF), (uint8_t)((addr >> 8) & 0x07)};
    writeBytes(OTP_IF, OTP_ADDR_SUB, addrBytes, 2);

    // Trigger OTP read
    uint8_t ctrl[2] = {0x03, 0x00};  // OTPRDEN | OTPREAD
    writeBytes(OTP_IF, OTP_CTRL_SUB, ctrl, 2);

    // Small delay for OTP read
    delayMicroseconds(10);

    // Read result
    uint8_t data[4];
    readBytes(OTP_IF, OTP_RDAT_SUB, data, 4);

    // Clear OTP control
    ctrl[0] = 0x00;
    writeBytes(OTP_IF, OTP_CTRL_SUB, ctrl, 2);

    return data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

void enableClock(uint8_t clock) {
    uint8_t pmscctrl0[4];
    readBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, 4);

    if (clock == AUTO_CLOCK) {
        pmscctrl0[0] = 0x00;
        pmscctrl0[1] &= 0xFE;
    } else if (clock == XTI_CLOCK) {
        pmscctrl0[0] &= 0xFC;
        pmscctrl0[0] |= 0x01;
    } else if (clock == PLL_CLOCK) {
        pmscctrl0[0] &= 0xFC;
        pmscctrl0[0] |= 0x02;
    }

    writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, 2);
}

void applyLDOTuning() {
    // This is the MISSING code from both libraries!
    // Set OTP_LDO bit in AON_CTRL to transfer LDO values from OTP to RAM
    uint8_t aonCtrl[4];
    readBytes(AON, AON_CTRL_SUB, aonCtrl, 4);

    // Set OTP_LDO bit (bit 6)
    aonCtrl[0] |= 0x40;
    writeBytes(AON, AON_CTRL_SUB, aonCtrl, 4);

    delay(1);  // Allow time for transfer

    // Clear the bit
    aonCtrl[0] &= ~0x40;
    writeBytes(AON, AON_CTRL_SUB, aonCtrl, 4);
}

void applyXTALTrim() {
    uint8_t fsxtalt;

    if (xtalTrimValue == 0) {
        // No OTP value - use midrange
        fsxtalt = 0x60 | 0x10;  // 0x10 = midrange
    } else {
        fsxtalt = 0x60 | (xtalTrimValue & 0x1F);
    }

    writeBytes(FS_CTRL, FS_XTALT_SUB, &fsxtalt, 1);
}

void configurePLL() {
    // Channel 5 configuration
    // FS_PLLCFG = 0x0800041D
    uint8_t pllcfg[4] = {0x1D, 0x04, 0x00, 0x08};
    writeBytes(FS_CTRL, FS_PLLCFG_SUB, pllcfg, 4);

    // FS_PLLTUNE = 0xBE (try 0xA6 if this doesn't work)
    uint8_t plltune = 0xBE;
    writeBytes(FS_CTRL, FS_PLLTUNE_SUB, &plltune, 1);
}

void printStatus() {
    uint8_t status[5];
    readBytes(SYS_STATUS, 0x00, status, 5);

    Serial.print(F("  SYS_STATUS: 0x"));
    for (int i = 4; i >= 0; i--) {
        if (status[i] < 0x10) Serial.print("0");
        Serial.print(status[i], HEX);
    }
    Serial.println();

    bool cplock = status[0] & (1 << CPLOCK_BIT);
    bool rfpllLL = status[3] & 0x01;
    bool clkpllLL = status[3] & 0x02;

    Serial.print(F("  CPLOCK: "));
    Serial.println(cplock ? "SET (good)" : "NOT SET (bad)");
    Serial.print(F("  RFPLL_LL: "));
    Serial.println(rfpllLL ? "SET (PLL losing lock!)" : "CLEAR (good)");
    Serial.print(F("  CLKPLL_LL: "));
    Serial.println(clkpllLL ? "SET (clock PLL losing lock!)" : "CLEAR (good)");
}
