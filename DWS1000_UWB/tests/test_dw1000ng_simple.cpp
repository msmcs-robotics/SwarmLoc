/**
 * DW1000-ng Simple Test
 *
 * Test using DW1000-ng library for PLL stability.
 * DW1000-ng enables CPLL lock detect automatically during init.
 */

#include <SPI.h>
#include <DW1000Ng.hpp>
#include <DW1000NgConstants.hpp>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Direct register access for diagnostics
#define SYS_STATUS_REG 0x0F

void readStatusDirect(byte* status, int len) {
    // Use SPI directly to read status
    digitalWrite(PIN_SS, LOW);
    SPI.transfer(SYS_STATUS_REG);  // Read command
    for (int i = 0; i < len; i++) {
        status[i] = SPI.transfer(0x00);
    }
    digitalWrite(PIN_SS, HIGH);
}

void printStatus() {
    byte status[5];
    readStatusDirect(status, 5);

    uint32_t stat = ((uint32_t)status[3] << 24) | ((uint32_t)status[2] << 16) |
                   ((uint32_t)status[1] << 8) | status[0];

    Serial.print("Status=0x");
    Serial.print(stat, HEX);

    if (stat & 0x02) Serial.print(" CPLOCK");
    else Serial.print(" !CPLOCK");

    if (stat & 0x2000000) Serial.print(" RFPLL_LL!");
    if (stat & 0x4000000) Serial.print(" CLKPLL_LL!");

    Serial.println();
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("DW1000-ng SIMPLE TEST");
    Serial.println("==========================================");
    Serial.println();

    Serial.println("[1] Initializing with DW1000-ng...");
    Serial.println("    (DW1000-ng enables CPLL lock detect & XTAL trim)");

    // Initialize - DW1000-ng does:
    // - Slow SPI during init
    // - Enable CPLL lock detect (PLLLDT bit)
    // - Apply XTAL trim from OTP
    // - Proper LDE microcode loading
    DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);

    Serial.println("\n[2] Getting device info...");

    char msg[128];
    DW1000Ng::getPrintableDeviceIdentifier(msg);
    Serial.print("Device ID: ");
    Serial.println(msg);

    if (strstr(msg, "DECA") == NULL) {
        Serial.println("[FAIL] DW1000 not detected!");
        while(1) delay(1000);
    }

    Serial.println("\n[3] Status after init:");
    printStatus();

    Serial.println("\n[4] Configuring...");

    // Use a simple configuration
    device_configuration_t config = {
        false,  // extendedFrameLength
        false,  // receiverAutoReenable
        true,   // smartPower
        false,  // frameCheck
        false,  // nlos
        SFDMode::STANDARD_SFD,
        Channel::CHANNEL_5,
        DataRate::RATE_110KBPS,
        PulseFrequency::FREQ_16MHZ,
        PreambleLength::LEN_2048,
        PreambleCode::CODE_4
    };

    DW1000Ng::applyConfiguration(config);

    DW1000Ng::getPrintableDeviceMode(msg);
    Serial.print("Mode: ");
    Serial.println(msg);

    Serial.println("\n[5] Status after config:");
    printStatus();

    Serial.println("\n[6] Enabling receiver...");
    DW1000Ng::startReceive();

    Serial.println("\n[7] Status after startReceive:");
    printStatus();

    Serial.println("\n[8] Monitoring (every 3 sec)...\n");
}

int iteration = 0;
unsigned long lastCheck = 0;

void loop() {
    if (millis() - lastCheck > 3000) {
        lastCheck = millis();
        iteration++;

        Serial.print("[");
        Serial.print(iteration);
        Serial.print("] ");

        // Check device ID
        char msg[32];
        DW1000Ng::getPrintableDeviceIdentifier(msg);

        if (strstr(msg, "DECA") != NULL) {
            Serial.print("ID=OK ");
            printStatus();
        } else {
            Serial.print("ID=FAIL(");
            Serial.print(msg);
            Serial.println(") - chip crashed!");

            // Try to recover
            Serial.println("    Attempting recovery...");
            DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);
            DW1000Ng::getPrintableDeviceIdentifier(msg);
            Serial.print("    After reset: ");
            Serial.println(msg);
        }

        // Check for receive
        if (DW1000Ng::isReceiveDone()) {
            uint16_t len = DW1000Ng::getReceivedDataLength();
            Serial.print("  RX: len=");
            Serial.print(len);

            if (len > 0 && len < 128) {
                byte data[128];
                DW1000Ng::getReceivedData(data, len);
                Serial.print(" data=");
                for (int i = 0; i < min((int)len, 16); i++) {
                    if (data[i] < 16) Serial.print("0");
                    Serial.print(data[i], HEX);
                    Serial.print(" ");
                }
            }
            Serial.println();

            // Clear and restart receiver
            DW1000Ng::clearReceiveStatus();
            DW1000Ng::startReceive();
        }
    }

    delay(50);
}
