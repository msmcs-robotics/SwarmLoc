/**
 * TWR Anchor (Responder) — DW1000-ng
 *
 * Asymmetric Two-Way Ranging: receives POLL, sends POLL_ACK, receives RANGE,
 * computes distance, sends RANGE_REPORT.
 *
 * Based on DW1000-ng TwoWayRangingResponder example.
 * DWS1000 shield: PIN_RST=7, D8→D2 wire for IRQ.
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000Ng.hpp>
#include <DW1000NgUtils.hpp>
#include <DW1000NgRanging.hpp>
#include <DW1000NgConstants.hpp>

const uint8_t PIN_RST = 7;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// TWR message types
#define POLL 0
#define POLL_ACK 1
#define RANGE 2
#define RANGE_REPORT 3
#define RANGE_FAILED 255

volatile byte expectedMsgId = POLL;
volatile boolean sentAck = false;
volatile boolean receivedAck = false;
boolean protocolFailed = false;

// Timestamps
uint64_t timePollSent;
uint64_t timePollReceived;
uint64_t timePollAckSent;
uint64_t timePollAckReceived;
uint64_t timeRangeSent;
uint64_t timeRangeReceived;

// Data buffer
#define LEN_DATA 16
byte data[LEN_DATA];

// Timing
uint32_t lastActivity;
uint32_t resetPeriod = 500;

// Stats
uint32_t rangeCount = 0;
uint32_t failCount = 0;
uint32_t resetCount = 0;

device_configuration_t DEFAULT_CONFIG = {
    false,                       // extendedFrameLength
    true,                        // receiverAutoReenable
    true,                        // smartPower
    true,                        // frameCheck
    false,                       // nlos
    SFDMode::STANDARD_SFD,       // sfd
    Channel::CHANNEL_5,          // channel
    DataRate::RATE_850KBPS,      // dataRate
    PulseFrequency::FREQ_16MHZ,  // pulseFreq
    PreambleLength::LEN_256,     // preambleLen
    PreambleCode::CODE_3         // preaCode
};

interrupt_configuration_t DEFAULT_INTERRUPT_CONFIG = {
    true,   // interruptOnSent
    true,   // interruptOnReceived
    true,   // interruptOnReceiveFailed
    false,  // interruptOnReceiveTimeout
    true    // interruptOnReceiveTimestampAvailable
};

void handleSent() {
    sentAck = true;
}

void handleReceived() {
    receivedAck = true;
}

void noteActivity() {
    lastActivity = millis();
}

void receiver() {
    DW1000Ng::forceTRxOff();
    DW1000Ng::startReceive();
}

void transmitPollAck() {
    data[0] = POLL_ACK;
    DW1000Ng::setTransmitData(data, LEN_DATA);
    DW1000Ng::startTransmit();
}

void transmitRangeReport(float curRange) {
    data[0] = RANGE_REPORT;
    memcpy(data + 1, &curRange, 4);
    DW1000Ng::setTransmitData(data, LEN_DATA);
    DW1000Ng::startTransmit();
}

void transmitRangeFailed() {
    data[0] = RANGE_FAILED;
    DW1000Ng::setTransmitData(data, LEN_DATA);
    DW1000Ng::startTransmit();
}

void resetInactive() {
    resetCount++;
    expectedMsgId = POLL;
    receiver();
    noteActivity();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== TWR Anchor (Responder) ==="));

    DW1000Ng::initialize(PIN_SS, PIN_IRQ, PIN_RST);
    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
    DW1000Ng::applyInterruptConfiguration(DEFAULT_INTERRUPT_CONFIG);

    DW1000Ng::setDeviceAddress(1);
    DW1000Ng::setNetworkId(10);
    DW1000Ng::setAntennaDelay(16405);

    char msg[128];
    DW1000Ng::getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: ")); Serial.println(msg);
    DW1000Ng::getPrintableDeviceMode(msg);
    Serial.print(F("Mode: ")); Serial.println(msg);

    DW1000Ng::attachSentHandler(handleSent);
    DW1000Ng::attachReceivedHandler(handleReceived);

    Serial.println(F("Listening for POLL...\n"));

    receiver();
    noteActivity();
}

void loop() {
    static uint32_t lastReport = 0;

    if (!sentAck && !receivedAck) {
        if (millis() - lastActivity > resetPeriod) {
            resetInactive();
        }
        return;
    }

    if (sentAck) {
        sentAck = false;
        byte msgId = data[0];
        if (msgId == POLL_ACK) {
            timePollAckSent = DW1000Ng::getTransmitTimestamp();
            noteActivity();
        }
        DW1000Ng::startReceive();
    }

    if (receivedAck) {
        receivedAck = false;
        DW1000Ng::getReceivedData(data, LEN_DATA);
        byte msgId = data[0];

        if (msgId != expectedMsgId) {
            protocolFailed = true;
        }

        if (msgId == POLL) {
            protocolFailed = false;
            timePollReceived = DW1000Ng::getReceiveTimestamp();
            expectedMsgId = RANGE;
            transmitPollAck();
            noteActivity();

        } else if (msgId == RANGE) {
            timeRangeReceived = DW1000Ng::getReceiveTimestamp();
            expectedMsgId = POLL;

            if (!protocolFailed) {
                timePollSent = DW1000NgUtils::bytesAsValue(data + 1, LENGTH_TIMESTAMP);
                timePollAckReceived = DW1000NgUtils::bytesAsValue(data + 6, LENGTH_TIMESTAMP);
                timeRangeSent = DW1000NgUtils::bytesAsValue(data + 11, LENGTH_TIMESTAMP);

                double distance = DW1000NgRanging::computeRangeAsymmetric(
                    timePollSent, timePollReceived,
                    timePollAckSent, timePollAckReceived,
                    timeRangeSent, timeRangeReceived
                );
                distance = DW1000NgRanging::correctRange(distance);

                rangeCount++;
                Serial.print(F("R#"));
                Serial.print(rangeCount);
                Serial.print(F(" dist="));
                Serial.print(distance, 2);
                Serial.print(F(" m  pwr="));
                Serial.print(DW1000Ng::getReceivePower(), 1);
                Serial.println(F(" dBm"));

                transmitRangeReport(distance * DISTANCE_OF_RADIO_INV);
            } else {
                failCount++;
                transmitRangeFailed();
            }
            noteActivity();
        }
    }

    if (millis() - lastReport >= 10000) {
        lastReport = millis();
        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] ranges:"));
        Serial.print(rangeCount);
        Serial.print(F(" fail:"));
        Serial.print(failCount);
        Serial.print(F(" reset:"));
        Serial.println(resetCount);
    }
}
