/**
 * DWS1000 CPLOCK Test
 *
 * Check if the Clock PLL locks after initialization.
 * CPLOCK must be set for the DW1000 to work properly.
 */

#include <SPI.h>
#include "DW1000.h"

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

#define SYS_STATUS_REG 0x0F
#define SYS_STATE_REG 0x19
#define PMSC_REG 0x36
#define PMSC_CTRL0_SUB 0x00

// CPLOCK is bit 1 of SYS_STATUS
#define CPLOCK_BIT 1

void printStatusHex() {
    byte status[5];
    DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);
    Serial.print("SYS_STATUS = 0x");
    for (int i = 4; i >= 0; i--) {
        if (status[i] < 16) Serial.print("0");
        Serial.print(status[i], HEX);
    }
    Serial.println();

    // Decode bits
    uint32_t stat = ((uint32_t)status[3] << 24) | ((uint32_t)status[2] << 16) |
                   ((uint32_t)status[1] << 8) | status[0];

    if (stat & 0x02) Serial.println("  [CPLOCK] Clock PLL LOCKED - GOOD!");
    else Serial.println("  [CPLOCK] Clock PLL NOT locked - BAD!");

    if (stat & 0x2000000) Serial.println("  [RFPLL_LL] RF PLL LOSING LOCK - BAD!");
    if (stat & 0x4000000) Serial.println("  [CLKPLL_LL] CLK PLL LOSING LOCK - BAD!");
}

void printStateHex() {
    byte state[4];
    DW1000.readBytes(SYS_STATE_REG, 0x00, state, 4);
    Serial.print("SYS_STATE = 0x");
    for (int i = 3; i >= 0; i--) {
        if (state[i] < 16) Serial.print("0");
        Serial.print(state[i], HEX);
    }
    uint8_t pmsc = state[0] & 0x1F;
    Serial.print(" (PMSC=");
    switch(pmsc) {
        case 0x00: Serial.print("INIT"); break;
        case 0x01: Serial.print("IDLE"); break;
        case 0x02: Serial.print("TX_WAIT"); break;
        case 0x03: Serial.print("TX"); break;
        case 0x06: Serial.print("RX_WAIT"); break;
        case 0x0D: Serial.print("RX"); break;
        default: Serial.print(pmsc, HEX); break;
    }
    Serial.println(")");
}

void printPMSC() {
    byte pmsc[4];
    DW1000.readBytes(PMSC_REG, PMSC_CTRL0_SUB, pmsc, 4);
    Serial.print("PMSC_CTRL0 = 0x");
    for (int i = 3; i >= 0; i--) {
        if (pmsc[i] < 16) Serial.print("0");
        Serial.print(pmsc[i], HEX);
    }
    Serial.println();

    // Decode SYSCLKS bits [1:0]
    uint8_t sysclks = pmsc[0] & 0x03;
    Serial.print("  SYSCLKS = ");
    switch(sysclks) {
        case 0: Serial.println("AUTO"); break;
        case 1: Serial.println("XTI (19.2MHz crystal)"); break;
        case 2: Serial.println("PLL (125MHz)"); break;
        default: Serial.println("reserved"); break;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("DWS1000 CPLOCK TEST");
    Serial.println("==========================================");

    // Initialize SPI manually first
    SPI.begin();
    pinMode(PIN_SS, OUTPUT);
    digitalWrite(PIN_SS, HIGH);
    pinMode(PIN_RST, INPUT);  // RST should be floating

    Serial.println("\n[1] Before DW1000.begin():");
    // Can't read anything yet without proper init

    // Initialize
    Serial.println("\n[2] Calling DW1000.begin()...");
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char deviceId[32];
    DW1000.getPrintableDeviceIdentifier(deviceId);
    Serial.print("Device ID: ");
    Serial.println(deviceId);

    Serial.println("\n[3] After DW1000.begin()/select():");
    printStatusHex();
    printStateHex();
    printPMSC();

    // Wait and check for CPLOCK
    Serial.println("\n[4] Waiting up to 1 second for CPLOCK...");
    unsigned long start = millis();
    bool gotCplock = false;
    while (millis() - start < 1000) {
        byte status[1];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 1);
        if (status[0] & 0x02) {
            gotCplock = true;
            Serial.print("  CPLOCK set after ");
            Serial.print(millis() - start);
            Serial.println(" ms");
            break;
        }
        delay(10);
    }

    if (!gotCplock) {
        Serial.println("  CPLOCK NEVER SET after 1 second!");
    }

    // Try manually forcing clock to PLL
    Serial.println("\n[5] Trying to force PLL clock...");

    byte pmscctrl0[4];
    DW1000.readBytes(PMSC_REG, PMSC_CTRL0_SUB, pmscctrl0, 4);
    Serial.print("  Before: PMSC_CTRL0 = 0x");
    for (int i = 3; i >= 0; i--) {
        if (pmscctrl0[i] < 16) Serial.print("0");
        Serial.print(pmscctrl0[i], HEX);
    }
    Serial.println();

    // Try forcing SYSCLKS to PLL (bits 1:0 = 10)
    pmscctrl0[0] = (pmscctrl0[0] & 0xFC) | 0x02;  // SYSCLKS = PLL
    DW1000.writeBytes(PMSC_REG, PMSC_CTRL0_SUB, pmscctrl0, 4);

    delay(100);

    Serial.println("\n[6] After forcing PLL:");
    printStatusHex();
    printStateHex();
    printPMSC();

    // Now try configuring
    Serial.println("\n[7] Configuring DW1000...");
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    Serial.println("\n[8] After configuration:");
    printStatusHex();
    printStateHex();

    // Try calling idle() explicitly
    Serial.println("\n[9] Calling idle() explicitly...");
    DW1000.idle();
    delay(100);

    Serial.println("\n[10] After idle():");
    printStatusHex();
    printStateHex();

    // Enable receiver
    Serial.println("\n[11] Enabling receiver...");
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
    delay(100);

    Serial.println("\n[12] After startReceive():");
    printStatusHex();
    printStateHex();

    Serial.println("\n=== DIAGNOSIS ===");
    byte finalStatus[1];
    DW1000.readBytes(SYS_STATUS_REG, 0x00, finalStatus, 1);
    if (finalStatus[0] & 0x02) {
        Serial.println("CPLOCK is SET - PLL is working");
    } else {
        Serial.println("CPLOCK NOT SET - DW1000 clock PLL not locking!");
        Serial.println("");
        Serial.println("Possible causes:");
        Serial.println("1. Power issue - DW1000 not getting proper 3.3V");
        Serial.println("2. Crystal problem");
        Serial.println("3. SPI communication issue");
        Serial.println("4. Chip damaged");
    }
}

void loop() {
    delay(5000);
    Serial.println("\n--- Status check ---");
    printStatusHex();
    printStateHex();
}
