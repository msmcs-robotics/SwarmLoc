/**
 * DWS1000 RF Diagnostic Test
 *
 * Dumps key registers to diagnose why RF communication fails.
 * Both devices should run this, then compare output.
 */

#include <SPI.h>
#include "DW1000.h"

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Key register addresses
#define DEV_ID_REG      0x00
#define SYS_CFG_REG     0x04
#define SYS_STATUS_REG  0x0F
#define SYS_STATE_REG   0x19
#define CHAN_CTRL_REG   0x1F
#define AGC_CTRL_REG    0x23
#define FS_CTRL_REG     0x2B
#define RF_CONF_REG     0x28

void readAndPrintReg(uint8_t reg, const char* name, int len) {
    byte data[8];
    memset(data, 0, 8);
    DW1000.readBytes(reg, 0x00, data, len);

    Serial.print(name);
    Serial.print(" (0x");
    Serial.print(reg, HEX);
    Serial.print("): ");

    // Print as hex
    for (int i = len-1; i >= 0; i--) {
        if (data[i] < 16) Serial.print("0");
        Serial.print(data[i], HEX);
    }
    Serial.println();
}

void printAllStatus() {
    byte status[5];
    DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 5);

    Serial.print("SYS_STATUS: 0x");
    for (int i = 4; i >= 0; i--) {
        if (status[i] < 16) Serial.print("0");
        Serial.print(status[i], HEX);
    }
    Serial.println();

    // Decode important bits
    uint32_t stat = ((uint32_t)status[3] << 24) | ((uint32_t)status[2] << 16) |
                   ((uint32_t)status[1] << 8) | status[0];

    Serial.println("  Bits set:");
    if (stat & 0x00000001) Serial.println("    - IRQS (interrupt)");
    if (stat & 0x00000002) Serial.println("    - CPLOCK (clock PLL locked)");
    if (stat & 0x00000004) Serial.println("    - ESYNCR (external sync)");
    if (stat & 0x00000008) Serial.println("    - AAT (auto ack TX)");
    if (stat & 0x00000010) Serial.println("    - TXFRB (TX frame begins)");
    if (stat & 0x00000020) Serial.println("    - TXPRS (TX preamble sent)");
    if (stat & 0x00000040) Serial.println("    - TXPHS (TX PHR sent)");
    if (stat & 0x00000080) Serial.println("    - TXFRS (TX frame sent)");
    if (stat & 0x00000100) Serial.println("    - RXPRD (RX preamble detected)");
    if (stat & 0x00000200) Serial.println("    - RXSFDD (RX SFD detected)");
    if (stat & 0x00000400) Serial.println("    - LDEDONE (LDE done)");
    if (stat & 0x00000800) Serial.println("    - RXPHD (RX PHR detected)");
    if (stat & 0x00001000) Serial.println("    - RXPHE (RX PHR error)");
    if (stat & 0x00002000) Serial.println("    - RXDFR (RX data frame ready)");
    if (stat & 0x00004000) Serial.println("    - RXFCG (RX FCS good)");
    if (stat & 0x00008000) Serial.println("    - RXFCE (RX FCS error)");
    if (stat & 0x00010000) Serial.println("    - RXRFSL (RX Reed-Solomon)");
    if (stat & 0x00020000) Serial.println("    - RXRFTO (RX frame timeout)");
    if (stat & 0x00040000) Serial.println("    - LDEERR (LDE error)");
    if (stat & 0x00100000) Serial.println("    - RXOVRR (RX overrun)");
    if (stat & 0x00200000) Serial.println("    - RXPTO (RX preamble timeout)");
    if (stat & 0x01000000) Serial.println("    - SLP2INIT (sleep to init)");
    if (stat & 0x02000000) Serial.println("    - RFPLL_LL (RF PLL losing lock)");
    if (stat & 0x04000000) Serial.println("    - CLKPLL_LL (CLK PLL losing lock)");
    if (stat & 0x20000000) Serial.println("    - AFFREJ (auto filter reject)");

    // Print byte 5 separately (high status bits)
    if (status[4] & 0x01) Serial.println("    - HPDWARN (HP detect warning)");
    if (status[4] & 0x02) Serial.println("    - TXBERR (TX buffer error)");
}

void printSysState() {
    byte state[4];
    DW1000.readBytes(SYS_STATE_REG, 0x00, state, 4);

    Serial.print("SYS_STATE: 0x");
    for (int i = 3; i >= 0; i--) {
        if (state[i] < 16) Serial.print("0");
        Serial.print(state[i], HEX);
    }

    // Decode PMSC state
    uint8_t pmscState = state[0] & 0x1F;
    Serial.print(" (PMSC=");
    switch(pmscState) {
        case 0x00: Serial.print("INIT"); break;
        case 0x01: Serial.print("IDLE"); break;
        case 0x02: Serial.print("TX_WAIT"); break;
        case 0x03: Serial.print("TX"); break;
        case 0x06: Serial.print("RX_WAIT"); break;
        case 0x0D: Serial.print("RX"); break;
        default: Serial.print(pmscState, HEX); break;
    }
    Serial.println(")");
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("DWS1000 RF DIAGNOSTIC TEST");
    Serial.println("==========================================");
    Serial.println();

    // Initialize
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);

    // Print device ID
    char deviceId[32];
    DW1000.getPrintableDeviceIdentifier(deviceId);
    Serial.print("Device ID: ");
    Serial.println(deviceId);

    // Read key configuration registers
    Serial.println("\n--- Key Registers BEFORE config ---");
    readAndPrintReg(DEV_ID_REG, "DEV_ID", 4);
    readAndPrintReg(SYS_CFG_REG, "SYS_CFG", 4);
    readAndPrintReg(CHAN_CTRL_REG, "CHAN_CTRL", 4);
    readAndPrintReg(AGC_CTRL_REG, "AGC_CTRL", 4);
    printSysState();
    printAllStatus();

    // Now configure
    Serial.println("\n--- Configuring device ---");
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000.commitConfiguration();

    char mode[128];
    DW1000.getPrintableDeviceMode(mode);
    Serial.print("Mode: ");
    Serial.println(mode);

    Serial.println("\n--- Key Registers AFTER config ---");
    readAndPrintReg(DEV_ID_REG, "DEV_ID", 4);
    readAndPrintReg(SYS_CFG_REG, "SYS_CFG", 4);
    readAndPrintReg(CHAN_CTRL_REG, "CHAN_CTRL", 4);
    readAndPrintReg(AGC_CTRL_REG, "AGC_CTRL", 4);
    printSysState();
    printAllStatus();

    // Try to transmit something
    Serial.println("\n--- Attempting transmit ---");

    // Clear all status
    byte clear[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clear, 5);

    DW1000.newTransmit();
    DW1000.setDefaults();
    byte msg[] = "TEST1234";
    DW1000.setData(msg, 8);

    Serial.println("Before startTransmit:");
    printSysState();

    DW1000.startTransmit();

    // Poll for completion
    unsigned long start = millis();
    bool txDone = false;
    while (millis() - start < 500) {
        byte status[5];
        DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 4);
        if (status[0] & 0x80) {  // TXFRS bit
            txDone = true;
            break;
        }
    }

    Serial.print("TX result: ");
    Serial.println(txDone ? "SUCCESS" : "FAILED/TIMEOUT");

    Serial.println("\nAfter transmit:");
    printSysState();
    printAllStatus();

    // Now try receive mode
    Serial.println("\n--- Enabling receiver ---");
    DW1000.writeBytes(SYS_STATUS_REG, 0x00, clear, 5);

    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();

    Serial.println("After startReceive:");
    printSysState();
    printAllStatus();

    Serial.println("\n--- Monitoring for 30 seconds ---");
    Serial.println("(watching for any status changes)\n");
}

uint32_t lastStatus = 0;
unsigned long lastPrint = 0;

void loop() {
    byte status[4];
    DW1000.readBytes(SYS_STATUS_REG, 0x00, status, 4);
    uint32_t stat = ((uint32_t)status[3] << 24) | ((uint32_t)status[2] << 16) |
                   ((uint32_t)status[1] << 8) | status[0];

    // Print if status changed or every 5 seconds
    if (stat != lastStatus) {
        Serial.print("[");
        Serial.print(millis() / 1000);
        Serial.print("s] Status changed: 0x");
        Serial.println(stat, HEX);
        lastStatus = stat;
        lastPrint = millis();

        // If we received something, decode it
        if (stat & 0x00004000) {  // RXFCG
            Serial.println("  FRAME RECEIVED!");
            uint16_t len = DW1000.getDataLength();
            byte data[64];
            DW1000.getData(data, len);
            Serial.print("  Length: ");
            Serial.print(len);
            Serial.print(", Data: ");
            for (int i = 0; i < min((int)len, 16); i++) {
                if (data[i] < 16) Serial.print("0");
                Serial.print(data[i], HEX);
                Serial.print(" ");
            }
            Serial.println();

            // Clear status and restart receiver
            byte clear[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            DW1000.writeBytes(SYS_STATUS_REG, 0x00, clear, 5);
        }
    } else if (millis() - lastPrint > 5000) {
        lastPrint = millis();
        Serial.print("[");
        Serial.print(millis() / 1000);
        Serial.print("s] Status: 0x");
        Serial.print(stat, HEX);
        Serial.print(" State: ");
        byte state[4];
        DW1000.readBytes(SYS_STATE_REG, 0x00, state, 4);
        uint8_t pmscState = state[0] & 0x1F;
        switch(pmscState) {
            case 0x00: Serial.print("INIT"); break;
            case 0x01: Serial.print("IDLE"); break;
            case 0x02: Serial.print("TX_WAIT"); break;
            case 0x03: Serial.print("TX"); break;
            case 0x06: Serial.print("RX_WAIT"); break;
            case 0x0D: Serial.print("RX"); break;
            default: Serial.print(pmscState, HEX); break;
        }
        Serial.println();
    }

    delay(10);
}
