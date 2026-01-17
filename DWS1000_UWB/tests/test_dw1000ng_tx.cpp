/**
 * DW1000-ng TX Test
 *
 * Transmitter using DW1000-ng library.
 * Upload to: /dev/ttyACM0 (or whichever device is more stable)
 */

#include <SPI.h>
#include <DW1000Ng.hpp>
#include <DW1000NgConstants.hpp>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

unsigned long txCount = 0;
unsigned long lastTx = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("DW1000-ng TX TEST");
    Serial.println("==========================================");

    DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);

    char msg[128];
    DW1000Ng::getPrintableDeviceIdentifier(msg);
    Serial.print("Device ID: ");
    Serial.println(msg);

    if (strstr(msg, "DECA") == NULL) {
        Serial.println("[FAIL] DW1000 not detected!");
        while(1) delay(1000);
    }

    // Configure
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

    Serial.println("\nStarting TX - sending PING every 2 seconds...\n");
}

void loop() {
    if (millis() - lastTx < 2000) {
        delay(50);
        return;
    }
    lastTx = millis();
    txCount++;

    // Check device is still alive
    char msg[32];
    DW1000Ng::getPrintableDeviceIdentifier(msg);
    if (strstr(msg, "DECA") == NULL) {
        Serial.print("[TX #");
        Serial.print(txCount);
        Serial.println("] CHIP CRASHED - attempting recovery...");

        DW1000Ng::initializeNoInterrupt(PIN_SS, PIN_RST);

        device_configuration_t config = {
            false, false, true, false, false,
            SFDMode::STANDARD_SFD,
            Channel::CHANNEL_5,
            DataRate::RATE_110KBPS,
            PulseFrequency::FREQ_16MHZ,
            PreambleLength::LEN_2048,
            PreambleCode::CODE_4
        };
        DW1000Ng::applyConfiguration(config);
        return;
    }

    Serial.print("[TX #");
    Serial.print(txCount);
    Serial.print("] Sending PING... ");

    // Prepare message
    byte txData[] = "PING1234";

    // Send
    DW1000Ng::setTransmitData(txData, sizeof(txData));
    DW1000Ng::startTransmit();

    // Wait for TX complete (poll)
    unsigned long start = millis();
    bool sent = false;
    while (millis() - start < 200) {
        if (DW1000Ng::isTransmitDone()) {
            sent = true;
            break;
        }
        delay(1);
    }

    if (sent) {
        Serial.println("SENT");
        DW1000Ng::clearTransmitStatus();
    } else {
        Serial.println("TIMEOUT");
    }
}
