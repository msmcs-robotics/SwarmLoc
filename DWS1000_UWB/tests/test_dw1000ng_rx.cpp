/**
 * DW1000-ng RX Test
 *
 * Receiver using DW1000-ng library.
 * Upload to: /dev/ttyACM1 (the more stable device)
 */

#include <SPI.h>
#include <DW1000Ng.hpp>
#include <DW1000NgConstants.hpp>

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

unsigned long rxCount = 0;
unsigned long lastStatus = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("DW1000-ng RX TEST");
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

    // Configure - must match TX!
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

    // Start receiving
    DW1000Ng::startReceive();

    Serial.println("\nReceiver started - waiting for PING...\n");
}

void loop() {
    // Check for received data
    if (DW1000Ng::isReceiveDone()) {
        rxCount++;

        uint16_t len = DW1000Ng::getReceivedDataLength();

        Serial.print("[RX #");
        Serial.print(rxCount);
        Serial.print("] len=");
        Serial.print(len);

        if (len > 0 && len < 128) {
            byte rxData[128];
            DW1000Ng::getReceivedData(rxData, len);

            Serial.print(" hex=");
            for (int i = 0; i < min((int)len, 16); i++) {
                if (rxData[i] < 16) Serial.print("0");
                Serial.print(rxData[i], HEX);
                Serial.print(" ");
            }

            Serial.print(" str=\"");
            for (int i = 0; i < min((int)len, 16); i++) {
                char c = rxData[i];
                if (c >= 32 && c < 127) Serial.print(c);
                else Serial.print('.');
            }
            Serial.print("\"");
        }
        Serial.println();

        // Clear and restart
        DW1000Ng::clearReceiveStatus();
        DW1000Ng::startReceive();
    }

    // Check for receive errors
    if (DW1000Ng::isReceiveFailed()) {
        Serial.println("[ERROR] Receive failed - resetting receiver");
        DW1000Ng::clearReceiveFailedStatus();
        DW1000Ng::forceTRxOff();
        delay(1);
        DW1000Ng::startReceive();
    }

    // Status update every 5 seconds
    if (millis() - lastStatus > 5000) {
        lastStatus = millis();

        // Check device health
        char msg[32];
        DW1000Ng::getPrintableDeviceIdentifier(msg);

        Serial.print("... waiting (RX=");
        Serial.print(rxCount);
        Serial.print(") ID=");

        if (strstr(msg, "DECA") != NULL) {
            Serial.println("OK");
        } else {
            Serial.print("FAIL(");
            Serial.print(msg);
            Serial.println(") - recovering...");

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
            DW1000Ng::startReceive();
        }
    }

    delay(10);
}
