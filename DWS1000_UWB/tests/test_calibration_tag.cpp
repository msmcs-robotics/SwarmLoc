/**
 * Antenna Delay Calibration Tag — DW1000-ng
 *
 * Place devices at a KNOWN distance, run this firmware on the tag (ACM1)
 * and test_twr_anchor.cpp on the anchor (ACM0).
 *
 * Collects 200 TWR measurements, computes statistics, and outputs
 * the recommended antenna delay adjustment.
 *
 * Set KNOWN_DISTANCE_M to the actual measured distance (antenna-to-antenna).
 *
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

// ============================================================
// SET THIS TO THE ACTUAL MEASURED DISTANCE (meters)
// Measure antenna center to antenna center with a tape measure.
// ============================================================
const float KNOWN_DISTANCE_M = 0.6096;  // 24 inches

// Starting antenna delay (current uncalibrated value)
uint16_t antennaDelay = 16436;

// Number of measurements to collect per calibration round
const uint16_t NUM_SAMPLES = 200;

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

// Statistics
float samples[NUM_SAMPLES];
uint16_t sampleCount = 0;
uint32_t timeoutCount = 0;
uint8_t calibrationRound = 0;

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

void computeAndPrintStats() {
    if (sampleCount == 0) {
        Serial.println(F("NO SAMPLES COLLECTED"));
        return;
    }

    // Compute mean
    float sum = 0;
    for (uint16_t i = 0; i < sampleCount; i++) {
        sum += samples[i];
    }
    float mean = sum / sampleCount;

    // Compute stddev, min, max
    float sumSq = 0;
    float minVal = samples[0];
    float maxVal = samples[0];
    for (uint16_t i = 0; i < sampleCount; i++) {
        float diff = samples[i] - mean;
        sumSq += diff * diff;
        if (samples[i] < minVal) minVal = samples[i];
        if (samples[i] > maxVal) maxVal = samples[i];
    }
    float stddev = sqrt(sumSq / sampleCount);

    // Compute antenna delay adjustment
    float error = mean - KNOWN_DISTANCE_M;
    float errorPerDevice = error / 2.0;
    int16_t delayAdj = (int16_t)(errorPerDevice / DISTANCE_OF_RADIO);
    uint16_t newDelay = antennaDelay + delayAdj;

    Serial.println(F("\n========================================"));
    Serial.print(F("CALIBRATION ROUND "));
    Serial.println(calibrationRound);
    Serial.println(F("========================================"));
    Serial.print(F("Known distance:  "));
    Serial.print(KNOWN_DISTANCE_M, 3);
    Serial.println(F(" m"));
    Serial.print(F("Antenna delay:   "));
    Serial.println(antennaDelay);
    Serial.print(F("Samples:         "));
    Serial.print(sampleCount);
    Serial.print(F("/"));
    Serial.println(NUM_SAMPLES);
    Serial.print(F("Timeouts:        "));
    Serial.println(timeoutCount);
    Serial.println(F("--- Measured Distance ---"));
    Serial.print(F("Mean:    "));
    Serial.print(mean, 4);
    Serial.println(F(" m"));
    Serial.print(F("StdDev:  "));
    Serial.print(stddev, 4);
    Serial.println(F(" m"));
    Serial.print(F("Min:     "));
    Serial.print(minVal, 4);
    Serial.println(F(" m"));
    Serial.print(F("Max:     "));
    Serial.print(maxVal, 4);
    Serial.println(F(" m"));
    Serial.println(F("--- Calibration ---"));
    Serial.print(F("Error:   "));
    Serial.print(error, 4);
    Serial.print(F(" m ("));
    Serial.print(error * 100, 1);
    Serial.println(F(" cm)"));
    Serial.print(F("Adj/dev: "));
    Serial.print(delayAdj);
    Serial.println(F(" ticks"));
    Serial.print(F("NEW_DELAY: "));
    Serial.println(newDelay);
    Serial.println(F("========================================"));

    // Check if within tolerance
    if (abs(error) < 0.05) {
        Serial.println(F("CALIBRATION COMPLETE — error < 5 cm"));
        Serial.print(F("Final antenna delay: "));
        Serial.println(antennaDelay);
        Serial.println(F("DONE"));
    } else {
        // Apply new delay and run another round
        Serial.print(F("Applying new delay "));
        Serial.println(newDelay);
        antennaDelay = newDelay;
        DW1000Ng::setAntennaDelay(antennaDelay);

        // Reset for next round
        calibrationRound++;
        sampleCount = 0;
        timeoutCount = 0;
        Serial.println(F("Starting next calibration round..."));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n=== Antenna Delay Calibration Tag ==="));
    Serial.print(F("Known distance: "));
    Serial.print(KNOWN_DISTANCE_M, 3);
    Serial.println(F(" m"));
    Serial.print(F("Initial delay:  "));
    Serial.println(antennaDelay);
    Serial.print(F("Samples/round:  "));
    Serial.println(NUM_SAMPLES);

    DW1000Ng::initialize(PIN_SS, PIN_IRQ, PIN_RST);
    DW1000Ng::applyConfiguration(DEFAULT_CONFIG);
    DW1000Ng::applyInterruptConfiguration(DEFAULT_INTERRUPT_CONFIG);

    DW1000Ng::setDeviceAddress(2);
    DW1000Ng::setNetworkId(10);
    DW1000Ng::setAntennaDelay(antennaDelay);

    DW1000Ng::attachSentHandler(handleSent);
    DW1000Ng::attachReceivedHandler(handleReceived);

    Serial.println(F("Collecting measurements...\n"));

    transmitPoll();
    noteActivity();
}

void loop() {
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
            float curRange;
            memcpy(&curRange, data + 1, 4);
            float distM = curRange * DISTANCE_OF_RADIO;

            // Store sample
            if (sampleCount < NUM_SAMPLES) {
                samples[sampleCount] = distM;
                sampleCount++;

                // Progress every 20 samples
                if (sampleCount % 20 == 0) {
                    Serial.print(F("  ["));
                    Serial.print(sampleCount);
                    Serial.print(F("/"));
                    Serial.print(NUM_SAMPLES);
                    Serial.print(F("] last="));
                    Serial.print(distM, 3);
                    Serial.println(F(" m"));
                }

                // Round complete
                if (sampleCount >= NUM_SAMPLES) {
                    computeAndPrintStats();
                }
            }

            expectedMsgId = POLL_ACK;
            transmitPoll();
            noteActivity();

        } else if (msgId == RANGE_FAILED) {
            expectedMsgId = POLL_ACK;
            transmitPoll();
            noteActivity();
        }
    }
}
