# DW1000 Antenna Delay Calibration Guide

**Goal**: Achieve ±10 cm ranging accuracy with Arduino Uno and DW1000 modules

**Time Required**: 30-60 minutes per device pair

---

## Table of Contents

1. [Understanding Antenna Delay](#1-understanding-antenna-delay)
2. [Why Calibration Matters](#2-why-calibration-matters)
3. [Equipment Needed](#3-equipment-needed)
4. [Calibration Theory](#4-calibration-theory)
5. [Step-by-Step Calibration Procedure](#5-step-by-step-calibration-procedure)
6. [Arduino Uno Implementation](#6-arduino-uno-implementation)
7. [Multi-Distance Validation](#7-multi-distance-validation)
8. [Expected Results](#8-expected-results)
9. [Troubleshooting](#9-troubleshooting)
10. [Advanced Topics](#10-advanced-topics)

---

## 1. Understanding Antenna Delay

### What is Antenna Delay?

**Antenna delay** is the total signal propagation delay through the analog RF circuitry before/after the digital timestamp is captured.

**Components**:
- PCB trace delays (signal path from DW1000 chip to antenna)
- Antenna matching network delays
- Connectors and cables (if present)
- Antenna radiation delay (physical antenna structure)
- Internal DW1000 analog delays (TX/RX chain)

### Physical Explanation

```
[DW1000 Chip] --PCB traces--> [Matching Network] --> [Antenna] --> RF Signal
      ^                                                    ^
      |                                                    |
  Digital Timestamp                                 Actual TX time
  captured here                                     happens here

      <-------------- Antenna Delay ----------------->
```

The DW1000 timestamps the signal at the digital baseband, but the RF signal actually leaves/arrives at the antenna slightly later/earlier. This fixed delay must be compensated.

### Time Units in DW1000

The DW1000 uses a 40-bit counter running at 499.2 MHz × 128 = 63.8976 GHz

**Key Constants**:
- **Time Resolution**: 15.65 picoseconds per tick
- **Distance per Tick**: ~0.00469 meters (4.69 mm)
- **Conversion**: 1 meter ≈ 213.14 time units

**From DW1000Time.h**:
```cpp
static constexpr float TIME_RES = 0.000015650040064103f;  // seconds per tick
static constexpr float DISTANCE_OF_RADIO = 0.0046917639786159f;  // meters per tick
```

### Antenna Delay Units

Antenna delay is specified in **DW1000 time units** (not microseconds or nanoseconds).

**Typical Range**: 16400 - 16500 time units for DWM1000 modules

This corresponds to:
- **Physical delay**: ~2.5 nanoseconds
- **Apparent distance error**: ~77 cm (if not compensated!)

---

## 2. Why Calibration Matters

### Impact on Accuracy

**Without Calibration**:
- Systematic offset: 50-100 cm (depending on hardware)
- Error is constant at all distances
- May still see relative distance changes correctly
- Absolute positioning severely compromised

**With Calibration**:
- Offset eliminated
- Achievable accuracy: ±10 cm (Arduino Uno)
- Achievable accuracy: ±5 cm (ESP32 with optimizations)
- Reliable absolute positioning

### Manufacturing Variations

Each DW1000 module has **slightly different** antenna delays due to:
- PCB manufacturing tolerances
- Component variations
- Antenna assembly differences
- Soldering quality

**Critical**: You must calibrate **each module individually**.

### Temperature Effects

Antenna delay changes with temperature:
- **Typical drift**: ±1-2 cm per 10°C
- For best accuracy, calibrate at operating temperature
- Consider periodic recalibration for critical applications

---

## 3. Equipment Needed

### Required

1. **Two DW1000 modules** (TAG and ANCHOR)
2. **Two Arduino Uno boards** (or your target MCU)
3. **Tape measure or ruler** (±1 mm accuracy)
   - Ideally 5+ meters long
   - Metal tape measure preferred (no sag)
4. **USB cables** for programming and serial monitor
5. **Clear line-of-sight test area** (5+ meters)
6. **Notebook** for recording measurements

### Optional but Recommended

7. **Tripods or stands** (for consistent module positioning)
8. **Laser level** (ensure modules at same height)
9. **Temperature sensor** (track environmental conditions)
10. **Calibration test fixtures** (for repeatable positioning)

### Test Environment

- **Clear space**: 5+ meters with no obstacles
- **Minimal RF interference**: Away from WiFi routers, microwaves
- **Stable temperature**: Indoor, climate-controlled preferred
- **Non-reflective surfaces**: Avoid metal walls/floors if possible

---

## 4. Calibration Theory

### DS-TWR (Double-Sided Two-Way Ranging)

The DW1000 examples use **asymmetric DS-TWR**, which inherently compensates for clock drift but NOT antenna delay.

**Ranging Protocol**:
```
TAG                          ANCHOR
 |                              |
 |--------POLL----------------->|  (T1: TAG TX timestamp)
 |                              |  (T2: ANCHOR RX timestamp)
 |                              |
 |<-------POLL_ACK--------------|  (T3: ANCHOR TX timestamp)
 |                              |  (T4: TAG RX timestamp)
 |                              |
 |--------RANGE---------------->|  (T5: TAG TX timestamp)
 |    (contains T1, T4, T5)     |  (T6: ANCHOR RX timestamp)
 |                              |
 |<-------RANGE_REPORT----------|  (Contains calculated distance)
 |                              |
```

**Time-of-Flight Calculation** (Asymmetric DS-TWR):
```cpp
round1 = (T4 - T1) = TAG's round-trip time
reply1 = (T3 - T2) = ANCHOR's processing time
round2 = (T6 - T3) = ANCHOR's round-trip time
reply2 = (T5 - T4) = TAG's processing time

ToF = (round1 × round2 - reply1 × reply2) / (round1 + round2 + reply1 + reply2)
```

**Antenna Delay Effect**:

The measured ToF includes **both** TAG and ANCHOR antenna delays:
```
ToF_measured = ToF_actual + AntennaDelay_TAG + AntennaDelay_ANCHOR
```

When both devices have the same antenna delay `D`:
```
ToF_measured = ToF_actual + 2×D
Distance_measured = Distance_actual + 2×D×DISTANCE_OF_RADIO
```

### Calibration Principle

**Goal**: Find the antenna delay `D` such that `Distance_measured = Distance_actual`

**Method**:
1. Measure at known distance `D_actual`
2. Record measured distance `D_measured`
3. Calculate error: `Error = D_measured - D_actual`
4. Adjust antenna delay to compensate

**Conversion Formula**:

```cpp
// Error in meters → Antenna delay adjustment in time units
float error_m = measured_distance - actual_distance;  // meters

// Each device contributes equally to error
float error_per_device = error_m / 2.0;  // meters

// Convert to time units (DW1000 ticks)
// 1 meter = 213.14 time units (approx)
int16_t delay_adjustment = (int16_t)(error_per_device * 213.14);

// Apply adjustment
uint16_t new_delay = current_delay + delay_adjustment;
```

**Important**: The sign convention:
- If **measuring too long** → **increase** antenna delay
- If **measuring too short** → **decrease** antenna delay

This seems counterintuitive, but remember: increasing antenna delay adds to the timestamp, which gets **subtracted** in the ToF calculation.

---

## 5. Step-by-Step Calibration Procedure

### Method 1: Single-Distance Quick Calibration

**Best for**: Quick testing, single-distance applications

**Procedure**:

1. **Setup Initial Configuration**
   ```cpp
   // Start with typical default value
   DW1000.setAntennaDelay(16450);
   ```

2. **Position Devices**
   - Place TAG and ANCHOR at exactly 1.000 meter apart (recommended starting distance)
   - Ensure clear line-of-sight
   - Align antennas (both vertical or both horizontal)
   - Use tape measure center-of-antenna to center-of-antenna

3. **Collect Measurements**
   - Run ranging for 30-60 seconds
   - Record minimum 30 stable measurements
   - Calculate average distance
   - Note: Ignore initial few measurements (settling time)

4. **Calculate Error**
   ```cpp
   float actual_distance = 1.000;  // meters
   float measured_distance = 1.087;  // example reading
   float error = measured_distance - actual_distance;  // +0.087 m
   ```

5. **Calculate Adjustment**
   ```cpp
   uint16_t current_delay = 16450;
   float error_per_device = error / 2.0;  // 0.0435 m
   int16_t adjustment = (int16_t)(error_per_device * 213.14);  // +9.27 → +9
   uint16_t new_delay = current_delay + adjustment;  // 16459
   ```

6. **Apply and Verify**
   ```cpp
   DW1000.setAntennaDelay(16459);
   ```
   - Re-run ranging at same distance
   - Verify error < 5 cm
   - If not, iterate steps 3-6

7. **Test at Different Distance**
   - Move to 2-3 meters
   - Verify accuracy maintained
   - If error increases proportionally → clock drift issue (unlikely with DS-TWR)
   - If constant offset → antenna delay needs refinement

### Method 2: Multi-Distance Calibration (Recommended)

**Best for**: High accuracy, production calibration, validation

**Procedure**:

1. **Choose Test Distances**
   ```cpp
   float test_distances[] = {0.5, 1.0, 2.0, 3.0, 5.0};  // meters
   const int NUM_DISTANCES = 5;
   ```

   **Distance Selection Guidelines**:
   - **0.5 m**: Near-field behavior (optional)
   - **1.0 m**: Primary calibration distance
   - **2.0 m**: Mid-range validation
   - **3.0 m**: Extended validation
   - **5.0 m**: Far-range validation
   - Avoid < 0.3 m (near-field effects)
   - Avoid > 10 m indoors (multipath)

2. **Collect Multi-Distance Data**
   ```cpp
   // Data collection structure
   struct CalibrationData {
       float actual;      // Measured with tape measure
       float measured;    // DW1000 measurement
       float error;       // measured - actual
       int samples;       // Number of measurements
   };

   CalibrationData data[NUM_DISTANCES];
   ```

   For each distance:
   - Position devices accurately (±1 cm)
   - Collect 50+ measurements
   - Calculate mean and standard deviation
   - Record in notebook

3. **Calculate Statistics**
   ```cpp
   // Calculate average error across all distances
   float total_error = 0;
   for (int i = 0; i < NUM_DISTANCES; i++) {
       data[i].error = data[i].measured - data[i].actual;
       total_error += data[i].error;
   }
   float avg_error = total_error / NUM_DISTANCES;

   // Check for linearity (should be constant offset)
   float max_deviation = 0;
   for (int i = 0; i < NUM_DISTANCES; i++) {
       float deviation = abs(data[i].error - avg_error);
       if (deviation > max_deviation) max_deviation = deviation;
   }

   // Good calibration: max_deviation < 0.05 m
   ```

4. **Detect Issues**
   - **Constant offset**: Normal, correct with antenna delay
   - **Linear error (increases with distance)**: Clock drift (shouldn't happen with DS-TWR)
   - **Non-linear error**: Multipath, interference, hardware issue

5. **Calculate Optimal Delay**
   ```cpp
   uint16_t current_delay = 16450;
   float error_per_device = avg_error / 2.0;
   int16_t adjustment = (int16_t)(error_per_device * 213.14);
   uint16_t new_delay = current_delay + adjustment;

   // Sanity check
   if (new_delay < 16300 || new_delay > 16600) {
       Serial.println("WARNING: Unusual antenna delay value!");
       Serial.println("Check hardware and measurements.");
   }
   ```

6. **Validate Calibration**
   - Apply new antenna delay to both devices
   - Re-test at all distances
   - Verify error < 0.1 m at all distances
   - If error still present, iterate

7. **Document Results**
   ```
   Calibration Report
   ------------------
   Date: YYYY-MM-DD
   Temperature: 22°C

   TAG Module SN: XXXX
   ANCHOR Module SN: YYYY

   Initial Antenna Delay: 16450
   Final Antenna Delay: 16459

   Distance    Measured    Error    StdDev
   --------    --------    -----    ------
   0.5 m       0.51 m      +1 cm    0.8 cm
   1.0 m       1.01 m      +1 cm    1.2 cm
   2.0 m       2.00 m       0 cm    1.5 cm
   3.0 m       3.02 m      +2 cm    1.8 cm
   5.0 m       5.04 m      +4 cm    2.5 cm

   Average Error: +1.6 cm
   Max Error: +4.0 cm
   Result: PASS (< 10 cm)
   ```

### Method 3: Two-Device Differential Calibration

**Best for**: Calibrating multiple devices relative to a reference

**Procedure**:

1. **Calibrate Reference Pair**
   - Use Method 2 to calibrate one TAG and one ANCHOR
   - This becomes your "golden pair"
   - Record calibrated antenna delays

2. **Calibrate Additional TAGs**
   - Keep reference ANCHOR
   - Replace TAG with new device (default delay)
   - Measure at 1.0 m
   - Calculate TAG antenna delay adjustment
   - Formula:
     ```cpp
     new_TAG_delay = default_delay + (error_m / 2.0) * 213.14;
     ```

3. **Calibrate Additional ANCHORs**
   - Keep reference TAG (calibrated)
   - Replace ANCHOR with new device (default delay)
   - Measure at 1.0 m
   - Calculate ANCHOR antenna delay adjustment

4. **Validate Cross-Compatibility**
   - Test all combinations
   - Ensure accuracy maintained
   - Record per-device antenna delays

**Advantage**: Faster calibration of multiple devices (after initial reference pair)

---

## 6. Arduino Uno Implementation

### Calibration Test Sketch (TAG)

Save as `CalibrationTAG.ino`:

```cpp
/*
 * DW1000 Antenna Delay Calibration - TAG
 *
 * Purpose: Calibrate antenna delay for accurate ranging
 * Hardware: Arduino Uno + DWM1000 module
 *
 * Instructions:
 * 1. Set ACTUAL_DISTANCE to your test distance (measured with tape measure)
 * 2. Upload to TAG device
 * 3. Position devices at exact distance
 * 4. Open Serial Monitor (115200 baud)
 * 5. Record measurements for 30-60 seconds
 * 6. Calculate average measured distance
 * 7. Adjust ANTENNA_DELAY using formula in Serial output
 * 8. Repeat until error < 5 cm
 */

#include <SPI.h>
#include <DW1000.h>

// Pin definitions
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Calibration settings
uint16_t ANTENNA_DELAY = 16450;  // START WITH DEFAULT VALUE
const float ACTUAL_DISTANCE = 1.000;  // Meters (measure accurately!)

// Protocol messages
#define POLL 0
#define POLL_ACK 1
#define RANGE 2
#define RANGE_REPORT 3
#define RANGE_FAILED 255

// State variables
volatile byte expectedMsgId = POLL_ACK;
volatile boolean sentAck = false;
volatile boolean receivedAck = false;

// Timestamps
DW1000Time timePollSent;
DW1000Time timePollAckReceived;
DW1000Time timeRangeSent;

// Data buffer
#define LEN_DATA 16
byte data[LEN_DATA];

// Timing
uint32_t lastActivity;
uint32_t resetPeriod = 250;
uint16_t replyDelayTimeUS = 3000;

// Measurement statistics
#define MAX_SAMPLES 100
float measurements[MAX_SAMPLES];
int sampleCount = 0;
int validRanges = 0;
unsigned long startTime;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("=== DW1000 Antenna Delay Calibration (TAG) ==="));
    Serial.println();
    Serial.print(F("Actual distance: "));
    Serial.print(ACTUAL_DISTANCE, 3);
    Serial.println(F(" m"));
    Serial.print(F("Current antenna delay: "));
    Serial.println(ANTENNA_DELAY);
    Serial.println();
    Serial.println(F("Collecting measurements..."));
    Serial.println();

    // Initialize DW1000
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);
    Serial.println(F("DW1000 initialized"));

    // Configuration
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);

    // SET ANTENNA DELAY
    DW1000.setAntennaDelay(ANTENNA_DELAY);

    DW1000.commitConfiguration();
    Serial.println(F("Configuration committed"));

    // Attach handlers
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);

    // Start ranging
    receiver();
    transmitPoll();
    noteActivity();

    startTime = millis();
}

void loop() {
    // Handle ranging protocol
    if (!sentAck && !receivedAck) {
        if (millis() - lastActivity > resetPeriod) {
            resetInactive();
        }
        return;
    }

    if (sentAck) {
        sentAck = false;
        byte msgId = data[0];
        if (msgId == POLL) {
            DW1000.getTransmitTimestamp(timePollSent);
        } else if (msgId == RANGE) {
            DW1000.getTransmitTimestamp(timeRangeSent);
            noteActivity();
        }
    }

    if (receivedAck) {
        receivedAck = false;
        DW1000.getData(data, LEN_DATA);
        byte msgId = data[0];

        if (msgId != expectedMsgId) {
            expectedMsgId = POLL_ACK;
            transmitPoll();
            return;
        }

        if (msgId == POLL_ACK) {
            DW1000.getReceiveTimestamp(timePollAckReceived);
            expectedMsgId = RANGE_REPORT;
            transmitRange();
            noteActivity();
        } else if (msgId == RANGE_REPORT) {
            expectedMsgId = POLL_ACK;
            float curRange;
            memcpy(&curRange, data + 1, 4);

            // Record measurement
            recordMeasurement(curRange);

            transmitPoll();
            noteActivity();
        } else if (msgId == RANGE_FAILED) {
            expectedMsgId = POLL_ACK;
            transmitPoll();
            noteActivity();
        }
    }

    // Print summary periodically
    if (validRanges > 0 && validRanges % 10 == 0) {
        if (millis() - startTime > 1000) {  // Print every batch after 1 second
            printStatistics();
            startTime = millis();
        }
    }
}

void recordMeasurement(float range) {
    // Convert from microseconds to meters
    float distance_m = range / 1000000.0 * 299702547.0;

    // Filter outliers (> 50% error)
    float error = abs(distance_m - ACTUAL_DISTANCE);
    if (error > ACTUAL_DISTANCE * 0.5) {
        return;  // Ignore bad measurement
    }

    if (sampleCount < MAX_SAMPLES) {
        measurements[sampleCount++] = distance_m;
    } else {
        // Shift array and add new measurement
        for (int i = 0; i < MAX_SAMPLES - 1; i++) {
            measurements[i] = measurements[i + 1];
        }
        measurements[MAX_SAMPLES - 1] = distance_m;
    }

    validRanges++;
}

void printStatistics() {
    if (sampleCount == 0) return;

    // Calculate mean
    float sum = 0;
    for (int i = 0; i < sampleCount; i++) {
        sum += measurements[i];
    }
    float mean = sum / sampleCount;

    // Calculate standard deviation
    float variance = 0;
    for (int i = 0; i < sampleCount; i++) {
        float diff = measurements[i] - mean;
        variance += diff * diff;
    }
    float stddev = sqrt(variance / sampleCount);

    // Calculate error
    float error = mean - ACTUAL_DISTANCE;
    float error_cm = error * 100.0;

    // Print results
    Serial.println(F("--- Measurement Statistics ---"));
    Serial.print(F("Samples: ")); Serial.println(sampleCount);
    Serial.print(F("Actual distance:   ")); Serial.print(ACTUAL_DISTANCE, 3); Serial.println(F(" m"));
    Serial.print(F("Measured distance: ")); Serial.print(mean, 3); Serial.println(F(" m"));
    Serial.print(F("Error: ")); Serial.print(error_cm, 1); Serial.println(F(" cm"));
    Serial.print(F("Std Dev: ")); Serial.print(stddev * 100, 1); Serial.println(F(" cm"));

    // Calculate recommended adjustment
    if (abs(error) > 0.005) {  // Only suggest if error > 5 mm
        float error_per_device = error / 2.0;
        int16_t adjustment = (int16_t)(error_per_device * 213.14);
        uint16_t new_delay = ANTENNA_DELAY + adjustment;

        Serial.println();
        Serial.println(F("CALIBRATION RECOMMENDATION:"));
        Serial.print(F("Current antenna delay: ")); Serial.println(ANTENNA_DELAY);
        Serial.print(F("Suggested adjustment: ")); Serial.println(adjustment);
        Serial.print(F("New antenna delay: ")); Serial.println(new_delay);
        Serial.println(F("Update ANTENNA_DELAY constant and re-upload"));
    } else {
        Serial.println(F("CALIBRATION COMPLETE! Error < 5 mm"));
    }

    Serial.println(F("------------------------------"));
    Serial.println();
}

void noteActivity() {
    lastActivity = millis();
}

void resetInactive() {
    expectedMsgId = POLL_ACK;
    transmitPoll();
    noteActivity();
}

void handleSent() {
    sentAck = true;
}

void handleReceived() {
    receivedAck = true;
}

void transmitPoll() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = POLL;
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void transmitRange() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = RANGE;
    DW1000Time deltaTime = DW1000Time(replyDelayTimeUS, DW1000Time::MICROSECONDS);
    timeRangeSent = DW1000.setDelay(deltaTime);
    timePollSent.getTimestamp(data + 1);
    timePollAckReceived.getTimestamp(data + 6);
    timeRangeSent.getTimestamp(data + 11);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void receiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}
```

### Calibration Test Sketch (ANCHOR)

Save as `CalibrationANCHOR.ino`:

```cpp
/*
 * DW1000 Antenna Delay Calibration - ANCHOR
 *
 * Purpose: Pair with TAG for antenna delay calibration
 * Hardware: Arduino Uno + DWM1000 module
 *
 * Instructions:
 * 1. Set same ANTENNA_DELAY as TAG
 * 2. Upload to ANCHOR device
 * 3. Position at exact distance from TAG
 * 4. Let TAG collect measurements
 * 5. ANCHOR just runs ranging protocol (no output needed)
 */

#include <SPI.h>
#include <DW1000.h>

// Pin definitions
const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Calibration settings
uint16_t ANTENNA_DELAY = 16450;  // MUST MATCH TAG VALUE

// Protocol messages
#define POLL 0
#define POLL_ACK 1
#define RANGE 2
#define RANGE_REPORT 3
#define RANGE_FAILED 255

// State variables
volatile byte expectedMsgId = POLL;
volatile boolean sentAck = false;
volatile boolean receivedAck = false;
boolean protocolFailed = false;

// Timestamps
DW1000Time timePollSent;
DW1000Time timePollReceived;
DW1000Time timePollAckSent;
DW1000Time timePollAckReceived;
DW1000Time timeRangeSent;
DW1000Time timeRangeReceived;
DW1000Time timeComputedRange;

// Data buffer
#define LEN_DATA 16
byte data[LEN_DATA];

// Timing
uint32_t lastActivity;
uint32_t resetPeriod = 250;
uint16_t replyDelayTimeUS = 3000;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("=== DW1000 Antenna Delay Calibration (ANCHOR) ==="));
    Serial.print(F("Antenna delay: ")); Serial.println(ANTENNA_DELAY);

    // Initialize DW1000
    DW1000.begin(PIN_IRQ, PIN_RST);
    DW1000.select(PIN_SS);
    Serial.println(F("DW1000 initialized"));

    // Configuration
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(1);
    DW1000.setNetworkId(10);
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);

    // SET ANTENNA DELAY
    DW1000.setAntennaDelay(ANTENNA_DELAY);

    DW1000.commitConfiguration();
    Serial.println(F("Configuration committed"));
    Serial.println(F("Ready for ranging..."));

    // Attach handlers
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);

    // Start receiver
    receiver();
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
        byte msgId = data[0];
        if (msgId == POLL_ACK) {
            DW1000.getTransmitTimestamp(timePollAckSent);
            noteActivity();
        }
    }

    if (receivedAck) {
        receivedAck = false;
        DW1000.getData(data, LEN_DATA);
        byte msgId = data[0];

        if (msgId != expectedMsgId) {
            protocolFailed = true;
        }

        if (msgId == POLL) {
            protocolFailed = false;
            DW1000.getReceiveTimestamp(timePollReceived);
            expectedMsgId = RANGE;
            transmitPollAck();
            noteActivity();
        } else if (msgId == RANGE) {
            DW1000.getReceiveTimestamp(timeRangeReceived);
            expectedMsgId = POLL;

            if (!protocolFailed) {
                timePollSent.setTimestamp(data + 1);
                timePollAckReceived.setTimestamp(data + 6);
                timeRangeSent.setTimestamp(data + 11);

                // Compute range using asymmetric DS-TWR
                computeRangeAsymmetric();

                // Send range report to TAG
                transmitRangeReport(timeComputedRange.getAsMicroSeconds());
            } else {
                transmitRangeFailed();
            }

            noteActivity();
        }
    }
}

void computeRangeAsymmetric() {
    DW1000Time round1 = (timePollAckReceived - timePollSent).wrap();
    DW1000Time reply1 = (timePollAckSent - timePollReceived).wrap();
    DW1000Time round2 = (timeRangeReceived - timePollAckSent).wrap();
    DW1000Time reply2 = (timeRangeSent - timePollAckReceived).wrap();
    DW1000Time tof = (round1 * round2 - reply1 * reply2) / (round1 + round2 + reply1 + reply2);
    timeComputedRange.setTimestamp(tof);
}

void noteActivity() {
    lastActivity = millis();
}

void resetInactive() {
    expectedMsgId = POLL;
    receiver();
    noteActivity();
}

void handleSent() {
    sentAck = true;
}

void handleReceived() {
    receivedAck = true;
}

void transmitPollAck() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = POLL_ACK;
    DW1000Time deltaTime = DW1000Time(replyDelayTimeUS, DW1000Time::MICROSECONDS);
    DW1000.setDelay(deltaTime);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void transmitRangeReport(float curRange) {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = RANGE_REPORT;
    memcpy(data + 1, &curRange, 4);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void transmitRangeFailed() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = RANGE_FAILED;
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void receiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}
```

### Using the Calibration Sketches

1. **Upload ANCHOR sketch** to first Arduino + DW1000
2. **Upload TAG sketch** to second Arduino + DW1000
3. **Position devices** at exactly 1.000 m (measure center-to-center)
4. **Open Serial Monitor** on TAG (115200 baud)
5. **Wait 60 seconds** for measurements
6. **Read statistics** from Serial Monitor:
   ```
   --- Measurement Statistics ---
   Samples: 87
   Actual distance:   1.000 m
   Measured distance: 1.087 m
   Error: +8.7 cm
   Std Dev: 1.2 cm

   CALIBRATION RECOMMENDATION:
   Current antenna delay: 16450
   Suggested adjustment: +9
   New antenna delay: 16459
   Update ANTENNA_DELAY constant and re-upload
   ```

7. **Update ANTENNA_DELAY** in both sketches to 16459
8. **Re-upload** to both devices
9. **Verify** error < 5 cm
10. **Repeat** if necessary

---

## 7. Multi-Distance Validation

### Test Protocol

After calibration, validate at multiple distances to ensure:
- Constant accuracy across range
- No distance-dependent errors
- Calibration is correct

### Recommended Test Distances

| Distance | Purpose | Expected Error |
|----------|---------|----------------|
| 0.5 m | Near-field test | ±2 cm |
| 1.0 m | Primary calibration | ±1 cm |
| 2.0 m | Mid-range validation | ±2 cm |
| 3.0 m | Extended range | ±3 cm |
| 5.0 m | Far-range validation | ±5 cm |
| 10.0 m | Maximum practical (indoors) | ±10 cm |

### Data Collection

For each distance:
1. Measure actual distance with tape measure (±1 cm accuracy)
2. Position devices with clear line-of-sight
3. Collect minimum 30 measurements
4. Calculate mean and standard deviation
5. Record in calibration log

### Example Validation Results

**Good Calibration** (constant small error):
```
Distance    Measured    Error    Std Dev
--------    --------    -----    -------
0.5 m       0.51 m      +1 cm    0.8 cm
1.0 m       1.01 m      +1 cm    1.1 cm
2.0 m       2.01 m      +1 cm    1.5 cm
3.0 m       3.02 m      +2 cm    2.0 cm
5.0 m       5.03 m      +3 cm    2.5 cm

Result: PASS - Constant offset, very small
```

**Needs Refinement** (larger constant error):
```
Distance    Measured    Error    Std Dev
--------    --------    -----    -------
0.5 m       0.58 m      +8 cm    1.0 cm
1.0 m       1.08 m      +8 cm    1.2 cm
2.0 m       2.08 m      +8 cm    1.6 cm
3.0 m       3.09 m      +9 cm    2.1 cm
5.0 m       5.09 m      +9 cm    2.8 cm

Result: NEEDS REFINEMENT - Constant offset too large
Action: Adjust antenna delay by +8 more time units
```

**Problem Detected** (distance-dependent error):
```
Distance    Measured    Error    Std Dev
--------    --------    -----    -------
0.5 m       0.51 m      +1 cm    1.0 cm
1.0 m       1.03 m      +3 cm    1.2 cm
2.0 m       2.08 m      +8 cm    1.8 cm
3.0 m       3.15 m      +15 cm   2.5 cm
5.0 m       5.28 m      +28 cm   3.5 cm

Result: FAIL - Error increases with distance
Problem: Clock drift or timing issue
Action: Check crystal oscillator, review protocol timing
```

### Error Analysis

**Acceptable Error Sources**:
- ±1-2 cm: DW1000 timestamp quantization
- ±1-3 cm: Arduino Uno interrupt latency
- ±1-2 cm: Multipath (even with LOS)
- ±2-3 cm: Temperature variations

**Total Expected Error**: ±5-10 cm (RSS of individual errors)

**Unacceptable Errors**:
- > 20 cm constant offset → Antenna delay not calibrated
- Proportional to distance → Clock drift (rare with DS-TWR)
- Large variance (> 10 cm std dev) → RF interference, multipath

---

## 8. Expected Results

### Before Calibration

**Typical Behavior**:
- Constant offset: 50-100 cm (too long)
- Can vary significantly between modules
- Relative distances may appear correct
- Absolute positioning impossible

**Example**:
```
Actual: 1.0 m → Measured: 1.08 m (error: +8 cm)
Actual: 2.0 m → Measured: 2.08 m (error: +8 cm)
Actual: 5.0 m → Measured: 5.08 m (error: +8 cm)
```

### After Calibration (Arduino Uno)

**Achievable Performance**:
- **Short range (< 3 m)**: ±5 cm
- **Medium range (3-5 m)**: ±10 cm
- **Long range (5-10 m)**: ±15 cm
- **Update rate**: 1-5 Hz

**Limitations**:
- Arduino Uno 16 MHz CPU limits update rate
- Interrupt latency adds timing uncertainty
- Cannot achieve ESP32-level accuracy (±2-5 cm)
- Indoor multipath affects longer distances

### After Calibration (ESP32)

**Achievable Performance**:
- **Short range (< 5 m)**: ±2-5 cm
- **Medium range (5-10 m)**: ±5-10 cm
- **Long range (10-30 m)**: ±10-20 cm
- **Update rate**: 10-50 Hz

**Advantages**:
- 240 MHz CPU → lower latency
- Better timing precision
- Can run more sophisticated algorithms

### Factors Affecting Accuracy

| Factor | Impact | Mitigation |
|--------|--------|------------|
| Antenna delay | 50-100 cm | **Calibration** |
| Interrupt latency | 2-5 cm | Faster MCU (ESP32) |
| Multipath | 5-20 cm | Clear LOS, outdoor testing |
| Temperature | 1-2 cm per 10°C | Calibrate at operating temp |
| RF interference | 5-10 cm | Change channel, avoid WiFi |
| Clock drift | < 1 cm (DS-TWR) | N/A (inherently compensated) |

---

## 9. Troubleshooting

### Issue 1: Very Large Error (> 1 meter)

**Symptoms**:
- Measured distance much longer than actual (e.g., 1.0 m reads as 1.8 m)

**Possible Causes**:
1. Antenna delay not set at all (defaults to 0)
2. Wrong device address (TAG and ANCHOR both address 1 or 2)
3. Time-of-flight calculation error

**Solutions**:
```cpp
// Verify antenna delay is set
DW1000.setAntennaDelay(16450);  // Must be called BEFORE commitConfiguration()
DW1000.commitConfiguration();

// Verify device addresses are different
// TAG:
DW1000.setDeviceAddress(2);
// ANCHOR:
DW1000.setDeviceAddress(1);

// Verify network IDs match
DW1000.setNetworkId(10);  // Same on both devices
```

### Issue 2: Unstable Measurements (High Variance)

**Symptoms**:
- Standard deviation > 10 cm
- Measurements jump wildly (0.5 m to 1.5 m)

**Possible Causes**:
1. RF interference (WiFi, Bluetooth)
2. Multipath (reflections from walls/metal)
3. Poor antenna orientation
4. Low signal power

**Solutions**:
```cpp
// Check signal quality
float rxPower = DW1000.getReceivePower();  // Should be > -80 dBm
float fpPower = DW1000.getFirstPathPower();  // Should be > -85 dBm

// Filter measurements by signal quality
if (rxPower < -90.0) {
    // Discard measurement
    return;
}

// Try different channel (less interference)
DW1000.setChannel(5);  // Options: 1, 2, 3, 4, 5, 7

// Ensure antennas aligned (both vertical or both horizontal)
```

### Issue 3: Distance-Proportional Error

**Symptoms**:
- Error increases linearly with distance
- Example: 1 m reads 1.02 m (+2 cm), 5 m reads 5.10 m (+10 cm)

**Possible Causes**:
1. Clock drift (very rare with DS-TWR)
2. Crystal oscillator issue
3. Incorrect ToF formula

**Solutions**:
- Verify using asymmetric DS-TWR (not symmetric)
- Check crystal oscillator on both modules
- Measure crystal frequency (should be 38.4 MHz ±10 ppm)
- Try different modules (may be hardware defect)

### Issue 4: Calibration Won't Converge

**Symptoms**:
- After adjusting antenna delay, error doesn't improve
- Error changes but doesn't approach zero

**Possible Causes**:
1. Measurement distance incorrect (tape measure error)
2. Antennas not aligned center-to-center
3. Hardware fault

**Solutions**:
- Double-check tape measure accuracy
- Measure from antenna center, not board edge
- Try calibration at different distance (2.0 m instead of 1.0 m)
- Verify both devices have same antenna delay value
- Test with known-good reference module

### Issue 5: Different Calibration Per Device Pair

**Symptoms**:
- TAG A + ANCHOR B calibrated to 16460
- TAG A + ANCHOR C calibrated to 16490
- Should be same for all combinations

**Explanation**:
Each device has unique antenna delay. If you calibrate pairs together, you're finding the **average** of both devices.

**Solution**:
Use Method 3 (Differential Calibration):
1. Calibrate reference TAG + ANCHOR pair
2. Fix ANCHOR, calibrate each TAG individually
3. Fix reference TAG, calibrate each ANCHOR individually
4. This gives true per-device antenna delays

### Issue 6: Antenna Delay Outside Normal Range

**Symptoms**:
- Calibrated value < 16300 or > 16600
- Device needs extreme adjustment

**Possible Causes**:
1. Wrong distance measurement
2. Hardware defect (antenna, PCB)
3. Wrong RF mode/configuration

**Solutions**:
- Verify tape measure accuracy
- Check antenna connection (loose connector?)
- Try different RF mode:
  ```cpp
  // Try different modes
  DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);  // Default
  DW1000.enableMode(DW1000.MODE_SHORTDATA_FAST_LOWPOWER);
  DW1000.enableMode(DW1000.MODE_LONGDATA_FAST_LOWPOWER);
  ```
- Contact module manufacturer (may be out of spec)

### Issue 7: Temperature Drift

**Symptoms**:
- Calibration accurate at 20°C
- Reads +5 cm error at 30°C

**Explanation**:
Antenna delay changes with temperature due to PCB expansion and component drift.

**Solutions**:
- Calibrate at expected operating temperature
- For outdoor use, calibrate at mid-range temperature
- For critical applications, implement temperature compensation:
  ```cpp
  float temp = readTemperatureSensor();  // Your sensor
  float tempCoeff = -0.01;  // -1 time unit per degree (measure this)
  int16_t tempAdjustment = (int16_t)((temp - 20.0) * tempCoeff);
  uint16_t adjusted_delay = base_delay + tempAdjustment;
  DW1000.setAntennaDelay(adjusted_delay);
  ```

---

## 10. Advanced Topics

### Topic 1: Individual Device Calibration

For production systems with many devices, calibrate each device's antenna delay independently.

**Procedure**:
1. Calibrate one "golden" TAG and one "golden" ANCHOR using Method 2
2. Record their antenna delays (e.g., TAG: 16455, ANCHOR: 16462)
3. For each new TAG:
   - Pair with golden ANCHOR (set to 16462)
   - Run calibration
   - Calculate TAG-specific delay
4. For each new ANCHOR:
   - Pair with golden TAG (set to 16455)
   - Run calibration
   - Calculate ANCHOR-specific delay

**Store per-device delays**:
```cpp
// Option 1: Flash memory (permanent)
#include <EEPROM.h>

void saveAntennaDelay(uint16_t delay) {
    EEPROM.write(0, delay & 0xFF);
    EEPROM.write(1, (delay >> 8) & 0xFF);
}

uint16_t loadAntennaDelay() {
    uint16_t delay = EEPROM.read(0);
    delay |= (EEPROM.read(1) << 8);
    return delay;
}

void setup() {
    uint16_t antenna_delay = loadAntennaDelay();
    if (antenna_delay < 16300 || antenna_delay > 16600) {
        antenna_delay = 16450;  // Use default if invalid
    }
    DW1000.setAntennaDelay(antenna_delay);
}

// Option 2: Hard-coded (if unique sketch per device)
#define DEVICE_ID 1
#if DEVICE_ID == 1
    #define ANTENNA_DELAY 16455
#elif DEVICE_ID == 2
    #define ANTENNA_DELAY 16462
#endif
```

### Topic 2: Asymmetric vs Symmetric Antenna Delays

**Scenario**: TAG and ANCHOR have different antenna delays.

**Example**:
- TAG antenna delay: 16450
- ANCHOR antenna delay: 16480

**Does DS-TWR handle this?** YES! The algorithm inherently compensates for different delays on each device.

**How it works**:
The ToF calculation includes terms that cancel out the individual antenna delays:
```
ToF_measured = ToF_actual + delay_TAG + delay_ANCHOR

But DS-TWR computes:
ToF = (round1 × round2 - reply1 × reply2) / (round1 + round2 + reply1 + reply2)

This formula cancels both delays when both devices perform ranging.
```

**Practical Implication**:
- Can use different antenna delays on TAG and ANCHOR
- Both should still be calibrated for best accuracy
- Useful when devices have different hardware (different PCB, antenna)

### Topic 3: Per-Channel Calibration

Antenna delay may vary slightly by RF channel due to frequency-dependent effects.

**Typical Variation**: ±5-10 time units across channels 1-7

**When to use**:
- High-precision applications (< 5 cm accuracy required)
- Fixed-channel operation (not hopping)

**Procedure**:
1. Calibrate at Channel 1 → record delay_ch1
2. Calibrate at Channel 2 → record delay_ch2
3. Repeat for all channels
4. Apply channel-specific delay:
   ```cpp
   uint16_t antenna_delays[8] = {
       0,      // Channel 0 (not used)
       16450,  // Channel 1
       16455,  // Channel 2
       16448,  // Channel 3
       16452,  // Channel 4
       16460,  // Channel 5
       0,      // Channel 6 (not used)
       16458   // Channel 7
   };

   uint8_t channel = 5;
   DW1000.setChannel(channel);
   DW1000.setAntennaDelay(antenna_delays[channel]);
   ```

**Cost-Benefit**: Usually not worth the effort unless absolute best accuracy needed.

### Topic 4: Automatic Calibration Algorithms

For systems that can't use tape measures (e.g., robots), implement automatic calibration.

**Method**: Use known waypoints or reference anchors.

**Algorithm**:
```cpp
// Assume 3 reference anchors at known positions
struct Anchor {
    float x, y, z;       // Known position
    float measured_dist; // Measured distance
};

Anchor anchors[3] = {
    {0.0, 0.0, 0.0, 0.0},
    {5.0, 0.0, 0.0, 0.0},
    {0.0, 5.0, 0.0, 0.0}
};

// Measure distances
for (int i = 0; i < 3; i++) {
    anchors[i].measured_dist = rangeTo(anchors[i]);
}

// Calculate TAG position using multilateration
Vector3 estimated_pos = multilaterate(anchors);

// Calculate expected distances from estimated position
float total_error = 0;
for (int i = 0; i < 3; i++) {
    float expected = distance(estimated_pos, anchors[i]);
    float error = anchors[i].measured_dist - expected;
    total_error += error;
}

// Average error → antenna delay adjustment
float avg_error = total_error / 3.0;
float error_per_device = avg_error / 2.0;
int16_t adjustment = (int16_t)(error_per_device * 213.14);
uint16_t new_delay = current_delay + adjustment;

DW1000.setAntennaDelay(new_delay);
```

**Advantages**:
- No manual measurement needed
- Can run periodically (adaptive calibration)
- Compensates for temperature drift

**Disadvantages**:
- Requires known reference positions
- More complex algorithm
- Needs multilateration implementation

### Topic 5: Factory Calibration and Storage

For commercial products, implement factory calibration workflow.

**Production Test Procedure**:
1. **Manufacture devices** with default antenna delay (16450)
2. **Test station**: Automated ranging against reference ANCHOR at 1.000 m
3. **Measure error** over 30-60 seconds
4. **Calculate optimal antenna delay** using calibration formula
5. **Program delay into EEPROM** or flash
6. **Verify** calibration with second measurement
7. **Label device** with calibration date and delay value
8. **Store in database** (device S/N → antenna delay)

**Code for Factory Test Station**:
```cpp
#define FACTORY_TEST_DISTANCE 1.000  // meters

void factoryCalibration() {
    Serial.println(F("=== FACTORY CALIBRATION MODE ==="));

    // Start with default
    uint16_t test_delay = 16450;
    DW1000.setAntennaDelay(test_delay);

    // Measure error
    float measured_avg = measureAverageDistance(30);  // 30 samples
    float error = measured_avg - FACTORY_TEST_DISTANCE;

    // Calculate optimal delay
    float error_per_device = error / 2.0;
    int16_t adjustment = (int16_t)(error_per_device * 213.14);
    uint16_t calibrated_delay = test_delay + adjustment;

    // Verify in range
    if (calibrated_delay < 16300 || calibrated_delay > 16600) {
        Serial.println(F("ERROR: Calibration out of range!"));
        Serial.println(F("Check hardware!"));
        return;
    }

    // Save to EEPROM
    saveAntennaDelay(calibrated_delay);

    // Verify
    DW1000.setAntennaDelay(calibrated_delay);
    float verified_avg = measureAverageDistance(30);
    float final_error = verified_avg - FACTORY_TEST_DISTANCE;

    if (abs(final_error) < 0.05) {  // < 5 cm
        Serial.println(F("CALIBRATION PASSED"));
        Serial.print(F("Antenna delay: ")); Serial.println(calibrated_delay);
        Serial.print(F("Final error: ")); Serial.print(final_error * 100); Serial.println(F(" cm"));
    } else {
        Serial.println(F("CALIBRATION FAILED"));
        Serial.println(F("Retest required"));
    }
}
```

### Topic 6: Calibration Quality Metrics

Define pass/fail criteria for calibration.

**Metrics**:

1. **Average Error**
   - Excellent: < 2 cm
   - Good: 2-5 cm
   - Acceptable: 5-10 cm
   - Poor: > 10 cm

2. **Standard Deviation**
   - Excellent: < 1 cm
   - Good: 1-2 cm
   - Acceptable: 2-5 cm
   - Poor: > 5 cm

3. **Linearity** (error variance across distances)
   - Excellent: < 2 cm variance
   - Good: 2-5 cm variance
   - Acceptable: 5-10 cm variance
   - Poor: > 10 cm variance

4. **Antenna Delay Value**
   - Normal: 16400-16500
   - Marginal: 16300-16400 or 16500-16600
   - Suspect: < 16300 or > 16600

**Quality Report**:
```cpp
struct CalibrationQuality {
    float avg_error_cm;
    float stddev_cm;
    float max_error_cm;
    float linearity_variance_cm;
    uint16_t antenna_delay;
    bool passed;
};

CalibrationQuality assessQuality(float errors[], int num_distances) {
    CalibrationQuality q;

    // Calculate average error
    float sum = 0;
    for (int i = 0; i < num_distances; i++) {
        sum += errors[i];
    }
    q.avg_error_cm = (sum / num_distances) * 100;

    // Calculate variance
    float variance = 0;
    for (int i = 0; i < num_distances; i++) {
        float dev = errors[i] - (q.avg_error_cm / 100);
        variance += dev * dev;
    }
    q.linearity_variance_cm = sqrt(variance / num_distances) * 100;

    // Determine pass/fail
    q.passed = (abs(q.avg_error_cm) < 10.0) &&
               (q.linearity_variance_cm < 5.0) &&
               (q.antenna_delay >= 16300) &&
               (q.antenna_delay <= 16600);

    return q;
}
```

---

## Summary

### Quick Calibration Checklist

- [ ] Prepare two DW1000 modules (TAG + ANCHOR)
- [ ] Upload calibration sketches
- [ ] Measure exact distance with tape measure (1.000 m)
- [ ] Position devices with clear line-of-sight
- [ ] Collect 30+ measurements (60 seconds)
- [ ] Calculate antenna delay adjustment
- [ ] Update ANTENNA_DELAY constant
- [ ] Re-upload and verify error < 5 cm
- [ ] Validate at multiple distances (0.5, 2.0, 5.0 m)
- [ ] Document calibrated delay values
- [ ] Store per-device delays (EEPROM or label)

### Key Formulas

**Antenna Delay Adjustment**:
```cpp
error_m = measured_distance - actual_distance;
error_per_device = error_m / 2.0;
adjustment = (int16_t)(error_per_device * 213.14);
new_delay = current_delay + adjustment;
```

**Time Units**:
- 1 time unit = 15.65 picoseconds
- 1 meter = 213.14 time units
- Typical delay: 16400-16500 time units

**Expected Accuracy** (Arduino Uno after calibration):
- Short range (< 3 m): ±5 cm
- Medium range (3-5 m): ±10 cm
- Long range (5-10 m): ±15 cm

### Common Pitfalls

1. **Forgetting to set antenna delay on both devices**
2. **Measuring distance incorrectly** (board edge instead of antenna center)
3. **Not waiting for measurements to stabilize** (< 30 samples)
4. **Calibrating with obstacles in line-of-sight**
5. **Using different antenna delays on TAG and ANCHOR during calibration**
6. **Not validating at multiple distances**

### Resources

- **DW1000 Datasheet**: Section 7.2.40 (Antenna Delay Register)
- **DW1000 User Manual**: Chapter 12 (Calibration and Testing)
- **Application Note APS006**: "Channel Effects on Communications Range and Timestamp Accuracy"
- **arduino-dw1000 library**: `/lib/DW1000/src/DW1000.cpp` (setAntennaDelay function)

---

**Document Version**: 1.0
**Last Updated**: 2026-01-11
**Author**: SwarmLoc Project
**License**: MIT

