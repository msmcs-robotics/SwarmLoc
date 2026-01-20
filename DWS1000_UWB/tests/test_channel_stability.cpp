/**
 * Test: Channel Stability Comparison
 *
 * Tests different DW1000 channels to find which has best PLL stability.
 * Lower frequency channels (1, 2, 4) may have better stability.
 *
 * Hardware: DWS1000 shield on Arduino Uno
 * Config: NO J1 jumper, D8->D2 wire for IRQ
 */

#include <Arduino.h>
#include <SPI.h>

// Pin configuration
const uint8_t PIN_RST = 9;
const uint8_t PIN_SS = SS;

// Register addresses
#define DEV_ID      0x00
#define SYS_STATUS  0x0F
#define PMSC        0x36
#define FS_CTRL     0x2B
#define RF_CONF     0x28
#define TX_CAL      0x2A

// Sub-registers
#define PMSC_CTRL0_SUB  0x00
#define FS_PLLCFG_SUB   0x07
#define FS_PLLTUNE_SUB  0x0B
#define FS_XTALT_SUB    0x0E
#define RF_TXCTRL_SUB   0x0C
#define RF_RXCTRLH_SUB  0x0B
#define TC_PGDELAY_SUB  0x0B

// Status bits
#define CPLOCK_BIT    1
#define RFPLL_LL_BIT  24
#define CLKPLL_LL_BIT 25

// Clock modes
#define AUTO_CLOCK 0x00
#define XTI_CLOCK  0x01

// Channel configurations
struct ChannelConfig {
    uint8_t channel;
    uint32_t fsPllcfg;
    uint8_t fsPlltune;
    uint32_t rfTxctrl;
    uint8_t rfRxctrlh;
    uint8_t tcPgdelay;
    const char* name;
};

// From DW1000 User Manual and library code
const ChannelConfig channels[] = {
    {1, 0x09000407, 0x1E, 0x00005C40, 0xD8, 0xC9, "CH1 (3.5GHz)"},
    {2, 0x08400508, 0x26, 0x00045CA0, 0xD8, 0xC2, "CH2 (4.0GHz)"},
    {3, 0x08401009, 0x56, 0x00086CC0, 0xD8, 0xC5, "CH3 (4.5GHz)"},
    {4, 0x08400508, 0x26, 0x00045C80, 0xBC, 0x95, "CH4 (4.0GHz wide)"},
    {5, 0x0800041D, 0xBE, 0x001E3FE0, 0xD8, 0xC0, "CH5 (6.5GHz)"},
    {7, 0x0800041D, 0xBE, 0x001E7DE0, 0xBC, 0x93, "CH7 (6.5GHz wide)"},
};
const int numChannels = 6;

// SPI settings
SPISettings slowSPI(2000000, MSBFIRST, SPI_MODE0);
SPISettings fastSPI(8000000, MSBFIRST, SPI_MODE0);

// Forward declarations
void readBytes(uint8_t reg, uint16_t sub, uint8_t* data, uint16_t len);
void writeBytes(uint8_t reg, uint16_t sub, uint8_t* data, uint16_t len);
void hardReset();
void enableClock(uint8_t clock);
void configureChannel(int idx);
void testChannelStability(int idx, int durationMs);

int currentChannel = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    Serial.println(F(""));
    Serial.println(F("========================================"));
    Serial.println(F("  CHANNEL STABILITY TEST"));
    Serial.println(F("  Testing which channel has best PLL"));
    Serial.println(F("========================================"));
    Serial.println(F(""));

    // Initialize SPI
    SPI.begin();
    pinMode(PIN_SS, OUTPUT);
    digitalWrite(PIN_SS, HIGH);
    pinMode(PIN_RST, INPUT);

    // Verify device
    hardReset();
    SPI.beginTransaction(slowSPI);
    uint8_t devId[4];
    readBytes(DEV_ID, 0x00, devId, 4);
    SPI.endTransaction();

    uint32_t id = devId[0] | (devId[1] << 8) | (devId[2] << 16) | ((uint32_t)devId[3] << 24);
    Serial.print(F("Device ID: 0x"));
    Serial.println(id, HEX);

    if (id != 0xDECA0130) {
        Serial.println(F("[ERROR] Invalid device ID!"));
        while (1) delay(1000);
    }

    Serial.println(F(""));
    Serial.println(F("Testing each channel for 10 seconds..."));
    Serial.println(F("Lower frequency channels may be more stable."));
    Serial.println(F(""));
}

void loop() {
    // Test each channel in sequence
    Serial.println(F("----------------------------------------"));
    Serial.print(F("Testing: "));
    Serial.println(channels[currentChannel].name);
    Serial.println(F("----------------------------------------"));

    // Reset and configure this channel
    hardReset();

    SPI.beginTransaction(slowSPI);

    // Set XTI clock
    enableClock(XTI_CLOCK);
    delay(5);

    // Apply XTAL trim (midrange)
    uint8_t fsxtalt = 0x70;  // 0x60 | 0x10
    writeBytes(FS_CTRL, FS_XTALT_SUB, &fsxtalt, 1);

    // Configure this channel
    configureChannel(currentChannel);

    // Switch to AUTO clock
    enableClock(AUTO_CLOCK);
    SPI.endTransaction();

    delay(10);  // Let PLL stabilize

    // Run stability test for 10 seconds
    testChannelStability(currentChannel, 10000);

    // Move to next channel
    currentChannel++;
    if (currentChannel >= numChannels) {
        Serial.println(F(""));
        Serial.println(F("========================================"));
        Serial.println(F("  ALL CHANNELS TESTED"));
        Serial.println(F("  Review results above to find"));
        Serial.println(F("  the most stable channel."));
        Serial.println(F("========================================"));
        Serial.println(F("Restarting in 5 seconds..."));
        delay(5000);
        currentChannel = 0;
    }

    delay(1000);
}

void testChannelStability(int idx, int durationMs) {
    uint32_t startTime = millis();
    uint32_t goodSamples = 0;
    uint32_t badSamples = 0;
    uint32_t rfpllErrors = 0;
    uint32_t clkpllErrors = 0;
    uint32_t noCplockErrors = 0;

    while (millis() - startTime < durationMs) {
        SPI.beginTransaction(fastSPI);
        uint8_t status[5];
        readBytes(SYS_STATUS, 0x00, status, 5);
        SPI.endTransaction();

        bool cplock = status[0] & (1 << CPLOCK_BIT);
        bool rfpllLL = status[3] & 0x01;
        bool clkpllLL = status[3] & 0x02;

        if (cplock && !rfpllLL && !clkpllLL) {
            goodSamples++;
        } else {
            badSamples++;
            if (rfpllLL) rfpllErrors++;
            if (clkpllLL) clkpllErrors++;
            if (!cplock) noCplockErrors++;
        }

        delay(100);  // Sample every 100ms
    }

    uint32_t total = goodSamples + badSamples;
    float successRate = (total > 0) ? (100.0 * goodSamples / total) : 0;

    Serial.println(F(""));
    Serial.print(F("Results for "));
    Serial.println(channels[idx].name);
    Serial.print(F("  Total samples: "));
    Serial.println(total);
    Serial.print(F("  Good: "));
    Serial.print(goodSamples);
    Serial.print(F(" ("));
    Serial.print(successRate, 1);
    Serial.println(F("%)"));
    Serial.print(F("  Bad: "));
    Serial.println(badSamples);
    Serial.print(F("    - RFPLL_LL: "));
    Serial.println(rfpllErrors);
    Serial.print(F("    - CLKPLL_LL: "));
    Serial.println(clkpllErrors);
    Serial.print(F("    - No CPLOCK: "));
    Serial.println(noCplockErrors);

    if (successRate >= 95) {
        Serial.println(F("  ==> EXCELLENT stability"));
    } else if (successRate >= 80) {
        Serial.println(F("  ==> GOOD stability"));
    } else if (successRate >= 50) {
        Serial.println(F("  ==> POOR stability"));
    } else {
        Serial.println(F("  ==> VERY POOR - try different channel"));
    }
    Serial.println(F(""));
}

void configureChannel(int idx) {
    const ChannelConfig& cfg = channels[idx];

    // FS_PLLCFG
    uint8_t pllcfg[4] = {
        (uint8_t)(cfg.fsPllcfg & 0xFF),
        (uint8_t)((cfg.fsPllcfg >> 8) & 0xFF),
        (uint8_t)((cfg.fsPllcfg >> 16) & 0xFF),
        (uint8_t)((cfg.fsPllcfg >> 24) & 0xFF)
    };
    writeBytes(FS_CTRL, FS_PLLCFG_SUB, pllcfg, 4);

    // FS_PLLTUNE
    uint8_t plltune = cfg.fsPlltune;
    writeBytes(FS_CTRL, FS_PLLTUNE_SUB, &plltune, 1);

    // RF_TXCTRL
    uint8_t txctrl[4] = {
        (uint8_t)(cfg.rfTxctrl & 0xFF),
        (uint8_t)((cfg.rfTxctrl >> 8) & 0xFF),
        (uint8_t)((cfg.rfTxctrl >> 16) & 0xFF),
        (uint8_t)((cfg.rfTxctrl >> 24) & 0xFF)
    };
    writeBytes(RF_CONF, RF_TXCTRL_SUB, txctrl, 4);

    // RF_RXCTRLH
    uint8_t rxctrlh = cfg.rfRxctrlh;
    writeBytes(RF_CONF, RF_RXCTRLH_SUB, &rxctrlh, 1);

    // TC_PGDELAY
    uint8_t pgdelay = cfg.tcPgdelay;
    writeBytes(TX_CAL, TC_PGDELAY_SUB, &pgdelay, 1);
}

void hardReset() {
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(2);
    pinMode(PIN_RST, INPUT);
    delay(10);
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
    }

    writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, 2);
}

void readBytes(uint8_t reg, uint16_t sub, uint8_t* data, uint16_t len) {
    uint8_t header[3];
    uint8_t headerLen = 1;

    if (sub > 0) {
        header[0] = 0x40 | reg;
        if (sub < 128) {
            header[1] = sub;
            headerLen = 2;
        } else {
            header[1] = 0x80 | (sub & 0x7F);
            header[2] = (sub >> 7) & 0xFF;
            headerLen = 3;
        }
    } else {
        header[0] = reg;
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
        header[0] = 0xC0 | reg;
        if (sub < 128) {
            header[1] = sub;
            headerLen = 2;
        } else {
            header[1] = 0x80 | (sub & 0x7F);
            header[2] = (sub >> 7) & 0xFF;
            headerLen = 3;
        }
    } else {
        header[0] = 0x80 | reg;
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
