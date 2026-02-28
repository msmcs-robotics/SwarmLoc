/**
 * TWR Tag (Initiator) — DW1000-ng
 *
 * Asymmetric Two-Way Ranging: sends POLL, receives POLL_ACK, sends RANGE.
 * Anchor computes distance and sends RANGE_REPORT back.
 *
 * Based on DW1000-ng TwoWayRangingInitiator example.
 * DWS1000 shield: PIN_RST=7, D8→D2 wire for IRQ.
 */

#include <Arduino.h>
#include <SPI.h>
#include <DW1000Ng.hpp>
#include <DW1000NgUtils.hpp>
#include <DW1000NgTime.hpp>
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

volatile byte expectedMsgId = POLL_ACK;
volatile boolean sentAck = false;
volatile boolean receivedAck = false;

// Timestamps
uint64_t timePollSent;
uint64_t timePollAckReceived;
uint64_t timeRangeSent;

// Data buffer
#define LEN_DATA 16
byte data[LEN_DATA];

// Timing
uint32_t lastActivity;
uint32_t resetPeriod = 500;
uint16_t replyDelayTimeUS = 3000;

// Stats
uint32_t pollCount = 0;
uint32_t rangeCount = 0;
uint32_t timeoutCount = 0;

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

void transmitPoll() {
    pollCount++;
    data[0] = POLL;
    DW1000Ng::setTransmitData(data, LEN_DATA);
    DW1000Ng::startTransmit();
}

void transmitRange() {
    data[0] = RANGE;

    byte futureTimeBytes[LENGTH_TIMESTAMP];
    timeRangeSent = DW1000Ng::getSystemTimestamp();
    timeRangeSent += DW1000NgTime::microsecondsToUWBTime(replyDelayTimeUS);
    DW1000NgUtils::writeValueToBytes(futureTimeBytes, timeRangeSent, LENGTH_TIMESTAMP);
    DW1000Ng::setDelayedTRX(futureTimeBytes);
    timeRangeSent += DW1000Ng::getTxAntennaDelay();

    DW1000NgUtils::writeValueToBytes(data + 1, timePollSent, LENGTH_TIMESTAMP);
    DW1000NgUtils::writeValueToBytes(data + 6, timePollAckReceived, LENGTH_TIMESTAMP);
    DW1000NgUtils::writeValueToBytes(data + 11, timeRangeSent, LENGTH_TIMESTAMP);
    DW1000Ng::setTransmitData(data, LEN_DATA);
    DW1000Ng::startTransmit(TransmitMode::DELAYED);
}

void resetInactive() {
    timeoutCount++;
    expectedMsgId = POLL_ACK;
    DW1000Ng::forceTRxOff();
    transmitPoll();
    noteActivity();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== TWR Tag (Initiator) ==="));

    DW1000Ng::initialize(PIN_SS, PIN_IRQ, PIN_RST);
    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
    DW1000Ng::applyInterruptConfiguration(DEFAULT_INTERRUPT_CONFIG);

    DW1000Ng::setDeviceAddress(2);
    DW1000Ng::setNetworkId(10);
    DW1000Ng::setAntennaDelay(16405);

    char msg[128];
    DW1000Ng::getPrintableDeviceIdentifier(msg);
    Serial.print(F("Device: ")); Serial.println(msg);
    DW1000Ng::getPrintableDeviceMode(msg);
    Serial.print(F("Mode: ")); Serial.println(msg);

    DW1000Ng::attachSentHandler(handleSent);
    DW1000Ng::attachReceivedHandler(handleReceived);

    Serial.println(F("Starting TWR...\n"));

    transmitPoll();
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
        DW1000Ng::startReceive();
    }

    if (receivedAck) {
        receivedAck = false;
        DW1000Ng::getReceivedData(data, LEN_DATA);
        byte msgId = data[0];

        if (msgId != expectedMsgId) {
            expectedMsgId = POLL_ACK;
            transmitPoll();
            return;
        }

        if (msgId == POLL_ACK) {
            timePollSent = DW1000Ng::getTransmitTimestamp();
            timePollAckReceived = DW1000Ng::getReceiveTimestamp();
            expectedMsgId = RANGE_REPORT;
            transmitRange();
            noteActivity();

        } else if (msgId == RANGE_REPORT) {
            rangeCount++;
            float curRange;
            memcpy(&curRange, data + 1, 4);
            float distM = curRange * DISTANCE_OF_RADIO;

            Serial.print(F("R#"));
            Serial.print(rangeCount);
            Serial.print(F(" "));
            Serial.print(distM, 2);
            Serial.println(F(" m"));

            expectedMsgId = POLL_ACK;
            transmitPoll();
            noteActivity();

        } else if (msgId == RANGE_FAILED) {
            expectedMsgId = POLL_ACK;
            transmitPoll();
            noteActivity();
        }
    }

    if (millis() - lastReport >= 10000) {
        lastReport = millis();
        Serial.print(F("["));
        Serial.print(millis() / 1000);
        Serial.print(F("s] polls:"));
        Serial.print(pollCount);
        Serial.print(F(" ranges:"));
        Serial.print(rangeCount);
        Serial.print(F(" timeouts:"));
        Serial.println(timeoutCount);
    }
}
