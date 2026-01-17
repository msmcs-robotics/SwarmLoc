/**
 * DWS1000 RF Status Test
 *
 * Check RF PLL lock, system state, and other RF indicators
 */

#include <SPI.h>
#include "DW1000.h"

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

#define SYS_STATUS_REG 0x0F
#define SYS_STATE_REG 0x19

void printStatus() {
    byte status[5];
    DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

    uint32_t stat = ((uint32_t)status[3] << 24) | ((uint32_t)status[2] << 16) |
                   ((uint32_t)status[1] << 8) | status[0];

    Serial.print("  SYS_STATUS=0x");
    Serial.print(stat, HEX);

    // Key bits
    if (stat & 0x02) Serial.print(" CPLOCK");       // Clock PLL locked
    if (stat & 0x80) Serial.print(" TXFRS");        // TX frame sent
    if (stat & 0x100) Serial.print(" RXPRD");       // RX preamble detected
    if (stat & 0x200) Serial.print(" RXSFDD");      // RX SFD detected
    if (stat & 0x400) Serial.print(" LDEDONE");     // LDE done
    if (stat & 0x2000) Serial.print(" RXDFR");      // RX data frame ready
    if (stat & 0x4000) Serial.print(" RXFCG");      // RX FCS good
    if (stat & 0x8000) Serial.print(" RXFCE");      // RX FCS error
    if (stat & 0x20000) Serial.print(" RXRFTO");    // RX frame wait timeout
    if (stat & 0x200000) Serial.print(" RXPTO");    // Preamble timeout
    if (stat & 0x2000000) Serial.print(" RFPLL_LL"); // RF PLL losing lock!
    if (stat & 0x4000000) Serial.print(" CLKPLL_LL");// CLK PLL losing lock!
    Serial.println();
}

void printState() {
    byte state[4];
    DW1000.readBytes(SYS_STATE_REG, 0x00, state, 4);
    uint8_t pmsc = state[0] & 0x1F;

    Serial.print("  SYS_STATE=0x");
    Serial.print(state[0], HEX);
    Serial.print(" (");
    switch(pmsc) {
        case 0x00: Serial.print("INIT"); break;
        case 0x01: Serial.print("IDLE"); break;
        case 0x02: Serial.print("TX_WAIT"); break;
        case 0x03: Serial.print("TX"); break;
        case 0x06: Serial.print("RX_WAIT"); break;
        case 0x0D: Serial.print("RX"); break;
        default: Serial.print("?"); break;
    }
    Serial.println(")");
}

void clearStatus() {
    byte clear[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clear, 5);
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("DWS1000 RF STATUS TEST");
    Serial.println("==========================================");

    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    char deviceId[32];
    DW1000.getPrintableDeviceIdentifier(deviceId);
    Serial.print("Device ID: ");
    Serial.println(deviceId);

    // Check initial status
    Serial.println("\n[1] After init:");
    printStatus();
    printState();

    // Configure
    Serial.println("\n[2] Configuring...");
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    char mode[128];
    DW1000.getPrintableDeviceMode(mode);
    Serial.print("  Mode: ");
    Serial.println(mode);

    Serial.println("\n[3] After config:");
    printStatus();
    printState();

    // Clear and enable receiver
    Serial.println("\n[4] Starting receiver...");
    clearStatus();
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();

    Serial.println("\n[5] After startReceive:");
    printStatus();
    printState();

    Serial.println("\n[6] Monitoring (5s intervals)...");
}

unsigned long lastPrint = 0;
int iteration = 0;

void loop() {
    if (millis() - lastPrint > 5000) {
        lastPrint = millis();
        iteration++;
        Serial.print("\n[Monitor ");
        Serial.print(iteration);
        Serial.print("]");
        printStatus();
        printState();
    }
    delay(100);
}
