/**
 * DW1000-ng CPLOCK Test
 *
 * Test using DW1000-ng library to see if PLL stability improves.
 * DW1000-ng has better initialization with CPLL lock detect enabled.
 */

#include <SPI.h>
#include <DW1000Ng.hpp>
#include <DW1000NgConstants.hpp>
#include <DW1000NgRegisters.hpp>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

void printStatus() {
    byte status[5];
    DW1000Ng::readSystemEventStatusRegister();

    // Read raw status bytes
    byte rawStatus[5];
    // Use internal read function
    DW1000Ng::getSystemStatus(rawStatus);

    uint32_t stat = ((uint32_t)rawStatus[3] << 24) | ((uint32_t)rawStatus[2] << 16) |
                   ((uint32_t)rawStatus[1] << 8) | rawStatus[0];

    Serial.print("  SYS_STATUS=0x");
    Serial.print(stat, HEX);

    if (stat & 0x02) Serial.print(" CPLOCK");
    if (stat & 0x2000000) Serial.print(" RFPLL_LL");
    if (stat & 0x4000000) Serial.print(" CLKPLL_LL");
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("DW1000-ng CPLOCK TEST");
    Serial.println("==========================================");
    Serial.println();

    Serial.println("[1] Initializing DW1000-ng...");

    // Initialize with DW1000-ng
    DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);

    Serial.println("[2] Getting device info...");

    char msg[128];
    DW1000Ng::getPrintableDeviceIdentifier(msg);
    Serial.print("Device ID: ");
    Serial.println(msg);

    // Check if it's valid
    if (strstr(msg, "DECA") == NULL) {
        Serial.println("[FAIL] DW1000 not detected!");
        Serial.println("Device may not be properly powered.");
        while(1) {
            delay(5000);
            Serial.println("Retrying...");
            DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);
            DW1000Ng::getPrintableDeviceIdentifier(msg);
            Serial.print("Device ID: ");
            Serial.println(msg);
        }
    }

    Serial.println("[3] After initialization:");
    // DW1000-ng should have enabled CPLL lock detect automatically

    // Read and print status
    byte status[5];
    // Read SYS_STATUS directly using low-level access
    // DW1000Ng uses namespace, need to access registers directly
    Serial.println("  (DW1000-ng enables CPLL lock detect during init)");

    Serial.println("\n[4] Configuring device...");

    // Configure for basic operation
    device_configuration_t config = {
        false,  // extendedFrameLength
        false,  // receiverAutoReenable
        true,   // smartPower
        false,  // frameCheck
        false,  // nlos
        SFDMode::STANDARD_SFD,
        Channel::CHANNEL_5,
        DataRate::RATE_850KBPS,  // Try faster rate
        PulseFrequency::FREQ_16MHZ,
        PreambleLength::LEN_256,
        PreambleCode::CODE_3
    };

    DW1000Ng::applyConfiguration(config);

    DW1000Ng::getPrintableDeviceMode(msg);
    Serial.print("Mode: ");
    Serial.println(msg);

    Serial.println("\n[5] Checking PLL status...");

    // Check for clock problems
    if (DW1000Ng::isClockProblem()) {
        Serial.println("  [WARNING] Clock problem detected!");
    } else {
        Serial.println("  [OK] No clock problems detected");
    }

    Serial.println("\n[6] Starting receiver...");
    DW1000Ng::startReceive();

    Serial.println("\n[7] Monitoring for 30 seconds...");
    Serial.println("    (checking every 3 seconds)\n");
}

int iteration = 0;
unsigned long lastCheck = 0;

void loop() {
    if (millis() - lastCheck > 3000) {
        lastCheck = millis();
        iteration++;

        Serial.print("[Monitor ");
        Serial.print(iteration);
        Serial.print("] ");

        // Re-read device ID to check for crashes
        char msg[32];
        DW1000Ng::getPrintableDeviceIdentifier(msg);

        if (strstr(msg, "DECA") != NULL) {
            Serial.print("ID=OK ");
        } else {
            Serial.print("ID=BAD(");
            Serial.print(msg);
            Serial.print(") ");
        }

        // Check clock problems
        if (DW1000Ng::isClockProblem()) {
            Serial.print("PLL=UNSTABLE ");

            // Try to recover
            Serial.print("(recovering...) ");
            DW1000Ng::forceTRxOff();
            delay(10);
            DW1000Ng::clearAllStatus();
            DW1000Ng::startReceive();
        } else {
            Serial.print("PLL=OK ");
        }

        // Check if we received anything
        if (DW1000Ng::isReceiveDone()) {
            Serial.print("RX=YES ");
            size_t len = DW1000Ng::getReceivedDataLength();
            Serial.print("len=");
            Serial.print(len);

            byte data[64];
            DW1000Ng::getReceivedData(data, len);
            Serial.print(" data=");
            for (int i = 0; i < min((int)len, 8); i++) {
                if (data[i] < 16) Serial.print("0");
                Serial.print(data[i], HEX);
            }

            DW1000Ng::clearReceiveStatus();
            DW1000Ng::startReceive();
        }

        Serial.println();
    }

    delay(100);
}
