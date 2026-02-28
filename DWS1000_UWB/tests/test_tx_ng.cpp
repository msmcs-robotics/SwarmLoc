/**
 * TX Test using DW1000-ng library
 *
 * DW1000-ng has better initialization (PLLLDT, slow SPI, proper clock sequencing)
 * which previously yielded 63+ frame detections vs 0 with thotro library.
 *
 * 110kbps, 16MHz PRF, Ch5, Preamble 2048, Code 4
 * PIN_RST = 7
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000Ng.hpp>
#include <DW1000NgConstants.hpp>
#include <DW1000NgConfiguration.hpp>

const uint8_t PIN_RST = 7;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

volatile bool txDone = false;
uint32_t txCount = 0;
uint32_t txGood = 0;

device_configuration_t DEFAULT_CONFIG = {
    false,                       // extendedFrameLength
    false,                       // receiverAutoReenable
    true,                        // smartPower
    true,                        // frameCheck
    false,                       // nlos
    SFDMode::STANDARD_SFD,       // sfd
    Channel::CHANNEL_5,          // channel
    DataRate::RATE_110KBPS,      // dataRate
    PulseFrequency::FREQ_16MHZ,  // pulseFreq
    PreambleLength::LEN_2048,    // preambleLen
    PreambleCode::CODE_4         // preaCode
};

interrupt_configuration_t INT_CONFIG = {
    true,   // interruptOnSent
    false,  // interruptOnReceived
    false,  // interruptOnReceiveFailed
    false,  // interruptOnReceiveTimeout
    false,  // interruptOnReceiveTimestampAvailable
    false   // interruptOnAutomaticAcknowledgeTrigger
};

void handleSent() {
    txGood++;
    txDone = true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== TX (DW1000-ng) ==="));

    DW1000Ng::initialize(PIN_SS, PIN_IRQ, PIN_RST);
    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
    DW1000Ng::applyInterruptConfiguration(INT_CONFIG);
    DW1000Ng::attachSentHandler(handleSent);

    DW1000Ng::setDeviceAddress(1);
    DW1000Ng::setNetworkId(10);

    DW1000Ng::setAntennaDelay(16384);

    char msg[128];
    DW1000Ng::getPrintableDeviceMode(msg);
    Serial.print(F("Mode: "));
    Serial.println(msg);

    char devId[128];
    DW1000Ng::getPrintableDeviceIdentifier(devId);
    Serial.print(F("Device: "));
    Serial.println(devId);

    Serial.println(F("Ready\n"));
}

void loop() {
    static uint32_t lastTx = 0;
    if (millis() - lastTx >= 2000) {
        lastTx = millis();
        txCount++;

        char data[32];
        snprintf(data, sizeof(data), "PING#%05lu", txCount);

        txDone = false;

        DW1000Ng::setTransmitData((byte*)data, strlen(data));
        DW1000Ng::startTransmit();

        uint32_t timeout = millis() + 200;
        while (!txDone && millis() < timeout) {
            delayMicroseconds(100);
        }

        Serial.print(F("TX #"));
        Serial.print(txCount);
        Serial.print(F(" \""));
        Serial.print(data);
        Serial.print(F("\" "));

        if (txDone) {
            Serial.print(F("OK ("));
            Serial.print(txGood);
            Serial.print(F("/"));
            Serial.print(txCount);
            Serial.println(F(")"));
        } else {
            Serial.println(F("TIMEOUT"));
        }
    }
    delay(10);
}
