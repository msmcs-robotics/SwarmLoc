# DW1000 Antenna Delay Calibration for ±10cm Accuracy - 2026 Edition

**Document Date**: 2026-01-11
**Purpose**: Comprehensive antenna delay calibration guide incorporating latest research and techniques
**Target Accuracy**: ±10cm or better
**Platform**: DW1000/DWM1000 UWB modules

---

## Executive Summary

This document provides research-based guidance for achieving ±10cm ranging accuracy with DW1000 Ultra-Wideband modules through proper antenna delay calibration. Based on current industry standards, academic research, and practical implementations, this guide consolidates best practices from official Qorvo/Decawave documentation, peer-reviewed papers, and open-source projects.

**Key Findings**:
- Default antenna delay: **16384** time units
- Typical calibrated range: **16400-16500** for DWM1000 modules
- Temperature coefficient: **2.15 mm/°C** per device
- Achievable accuracy: **±10cm** (basic), **±2-5cm** (advanced with optimizations)

---

## Table of Contents

1. [Understanding Antenna Delay](#1-understanding-antenna-delay)
2. [Typical Antenna Delay Values](#2-typical-antenna-delay-values)
3. [Step-by-Step Calibration Procedures](#3-step-by-step-calibration-procedures)
4. [Calibration Formulas and Calculations](#4-calibration-formulas-and-calculations)
5. [Best Practices and Tips](#5-best-practices-and-tips)
6. [Advanced Techniques](#6-advanced-techniques)
7. [Tools and Methods](#7-tools-and-methods)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Understanding Antenna Delay

### What is Antenna Delay?

Antenna delay represents the **total propagation delay through the DW1000 device's analog RF circuitry** from the point where digital timestamps are captured to where the RF signal actually leaves/arrives at the antenna.

**Fundamental Equation**:
```
t_Measured = t_ADTX + ToF + t_ADRX
```

Where:
- `ToF` = Time of Flight (actual distance ÷ speed of light)
- `t_ADTX` = Transmit antenna delay
- `t_ADRX` = Receive antenna delay

### Physical Components Contributing to Delay

1. **Internal DW1000 analog delays** - TX/RX chain propagation
2. **PCB trace delays** - Signal path from chip to antenna
3. **Antenna matching network** - Impedance matching circuitry
4. **Connectors and cables** - If present in design
5. **Antenna radiation delay** - Physical antenna structure effects

### Why Calibration is Critical

**Uncalibrated Error Magnitude**:
- Typical antenna delay: ~2.5 nanoseconds (16400-16500 time units)
- Distance representation: ~**77 cm** if not compensated
- Error in ranging: **50-100 cm constant offset**

**Manufacturing Variations**:
- Internal propagation delays vary chip-to-chip
- PCB manufacturing tolerances
- Component batch variations
- Soldering quality differences

**Critical Finding**: The IC manufacturer cannot set these delays in hardware during production, so **users must calibrate in firmware**.

### DW1000 Time Units

**Time Resolution**:
- **1 time unit = 15.65 picoseconds**
- Based on 63.8976 GHz sampling clock (IEEE 802.15.4-2011 standard)

**Distance Conversion**:
- **1 time unit ≈ 4.69 mm** distance error
- **1 meter ≈ 213.14 time units**
- **10 time units ≈ 4.7 cm** distance change

**From DW1000Time.h constants**:
```cpp
static constexpr float TIME_RES = 0.000015650040064103f;  // seconds per tick
static constexpr float DISTANCE_OF_RADIO = 0.0046917639786159f;  // meters per tick
```

---

## 2. Typical Antenna Delay Values

### Industry Standard Values

**Default Starting Value**:
- **16384** time units (common default in documentation)
- **16450** time units (practical starting point used in many implementations)

**Typical Calibrated Ranges**:
- **DWM1000 modules**: 16400-16500 time units
- **Custom PCB designs**: 16300-16600 time units (wider variance)
- **ESP32-UWB modules**: 16450-16465 time units (commonly observed)

### Range Indicators

**Normal Range**: 16400-16500 (for DWM1000)
- Most devices calibrate within this range
- Indicates proper hardware and calibration

**Marginal Range**: 16300-16400 or 16500-16600
- Still acceptable but near edges
- May indicate hardware variations

**Out of Range**: <16300 or >16600
- Potential hardware issue
- Possible measurement error during calibration
- Verify hardware and re-calibrate

### Real-World Examples from Research

**Official Decawave Examples**:
- TX_ANT_DLY: 16436
- RX_ANT_DLY: 16436

**Community Implementations**:
- jremington/UWB-Indoor-Localization: 16455-16470 (typical final values)
- Makerfabs ESP32-UWB: 16460-16465 (after calibration)
- Bitcraze Loco Positioning: 16456 (common converged value)

### Distance Representation

**Antenna Delay 16450**:
- 16450 × 0.00469 m/unit ≈ **77 cm** apparent distance
- This explains why uncalibrated systems show ~50-100 cm offset

**Adjustment Sensitivity**:
- ±1 time unit → ±4.69 mm distance change
- ±10 time units → ±4.7 cm distance change
- ±20 time units → ±9.4 cm distance change

---

## 3. Step-by-Step Calibration Procedures

### Method 1: Two-Device Simple Calibration (Recommended for Beginners)

**Official Qorvo/Decawave Method** (from APS014):

**Calibration Distance**:
- Official recommendation: **7.94 meters**
- Common practice: **1.0 meter** (easier to measure accurately)
- Alternative: **5.0 meters** (compromise)

**Procedure**:

1. **Setup Hardware**
   - Two DW1000 modules (TAG and ANCHOR)
   - Position at known fixed distance (e.g., 1.000 m)
   - Measure center-to-center with accurate tape measure (±1mm)
   - Ensure clear line-of-sight
   - Align antenna orientations (both vertical or both horizontal)

2. **Set Initial Antenna Delay**
   ```cpp
   DW1000.setAntennaDelay(16450);  // Starting value
   ```
   - Apply same value to both TAG and ANCHOR

3. **Collect Measurements**
   - Perform **1000 ranging operations** using Two-Way Ranging
   - Record all distance measurements
   - Calculate average measured distance
   - Calculate standard deviation

4. **Calculate Error**
   ```cpp
   float actual_distance = 1.000;           // meters
   float measured_distance = 1.087;         // average from measurements
   float error = measured_distance - actual_distance;  // +0.087 m
   ```

5. **Calculate Adjustment**
   ```cpp
   float error_per_device = error / 2.0;    // 0.0435 m
   int16_t adjustment = (int16_t)(error_per_device * 213.14);  // +9.27 → +9
   uint16_t new_delay = 16450 + adjustment;  // 16459
   ```

6. **Apply New Delay**
   ```cpp
   DW1000.setAntennaDelay(16459);
   ```
   - Update both TAG and ANCHOR

7. **Verify Calibration**
   - Re-run ranging at same distance
   - Verify error < 5 cm
   - If error still significant, iterate steps 3-7

8. **Multi-Distance Validation**
   - Test at 0.5m, 2.0m, 3.0m, 5.0m
   - Verify constant small error (not distance-dependent)
   - Document final antenna delay value

### Method 2: Binary Search Auto-Calibration (Advanced)

**From Community Implementations** (jremington/UWB-Indoor-Localization):

**Algorithm**:
```cpp
uint16_t binary_search_calibration(float target_distance) {
    uint16_t min_delay = 16300;
    uint16_t max_delay = 16600;
    const float tolerance = 0.01;  // 1 cm

    while (max_delay - min_delay > 1) {
        uint16_t test_delay = (min_delay + max_delay) / 2;
        DW1000.setAntennaDelay(test_delay);

        float measured = average_n_measurements(100);
        float error = measured - target_distance;

        if (abs(error) < tolerance) {
            return test_delay;  // Converged
        }

        if (error > 0) {
            // Measuring too long → increase delay
            min_delay = test_delay;
        } else {
            // Measuring too short → decrease delay
            max_delay = test_delay;
        }
    }

    return (min_delay + max_delay) / 2;
}
```

**Advantages**:
- Fully automatic (no manual iteration)
- Fast convergence (8-10 iterations)
- Deterministic result

**Time Required**: ~2-3 minutes per device

### Method 3: Three-Node Calibration System (Official Advanced Method)

**From APS014 and Academic Research**:

When three nodes are placed at fixed known positions, individual antenna delays can be calculated by solving a system of equations.

**Setup**:
- Three nodes (A, B, C) at known positions
- Measure actual distances: d_AB, d_AC, d_BC (with tape measure)

**Procedure**:

1. Perform TWR between all pairs
2. Record TOF values: TOF_AB, TOF_AC, TOF_BC
3. Solve system of equations:
   ```
   TOF_AB = d_AB/c + AD_A + AD_B
   TOF_AC = d_AC/c + AD_A + AD_C
   TOF_BC = d_BC/c + AD_B + AD_C
   ```
4. Three equations, three unknowns → solve for AD_A, AD_B, AD_C

**Advantages**:
- Provides individual device antenna delays
- More accurate for heterogeneous systems
- Scalable: new devices calibrated against any calibrated device

**Use Case**: Production environments with many devices

---

## 4. Calibration Formulas and Calculations

### Primary Calibration Formula

**Antenna Delay Adjustment**:
```cpp
// Step 1: Calculate error
float error_m = measured_distance - actual_distance;  // meters

// Step 2: Divide by 2 (both devices contribute equally)
float error_per_device = error_m / 2.0;  // meters

// Step 3: Convert to time units
// Conversion factor: 1 meter = 213.14 time units
int16_t delay_adjustment = (int16_t)(error_per_device * 213.14);

// Step 4: Apply adjustment
uint16_t new_delay = current_delay + delay_adjustment;
```

**Sign Convention**:
- If **measuring too long** → **increase** antenna delay
- If **measuring too short** → **decrease** antenna delay

This is counterintuitive but correct: increasing antenna delay adds to timestamp, which gets subtracted in ToF calculation.

### DS-TWR Time-of-Flight Calculation

**Asymmetric Double-Sided Two-Way Ranging** (what DW1000 uses):

```cpp
// Timestamps from ranging protocol
DW1000Time round1 = (timePollAckReceived - timePollSent).wrap();
DW1000Time reply1 = (timePollAckSent - timePollReceived).wrap();
DW1000Time round2 = (timeRangeReceived - timePollAckSent).wrap();
DW1000Time reply2 = (timeRangeSent - timePollAckReceived).wrap();

// Time-of-Flight calculation
DW1000Time tof = (round1 * round2 - reply1 * reply2) /
                 (round1 + round2 + reply1 + reply2);
```

**Why This Formula**:
- Inherently compensates for clock drift
- Partially cancels antenna delays when symmetric
- Most robust TWR approach

### Temperature Compensation Formula

**Temperature Coefficient**: **2.15 mm/°C** per device (official value)

**Compensation Calculation**:
```cpp
float antenna_delay_temp_compensation(float base_delay,
                                      float temp_cal,
                                      float temp_current) {
    // Temperature coefficient: 2.15 mm/°C
    // Convert to time units: 2.15 mm / 4.69 mm per unit = 0.458 time units/°C

    float temp_diff = temp_current - temp_cal;  // °C
    float temp_coeff = 0.458;  // time units per °C

    float adjusted_delay = base_delay + (temp_coeff * temp_diff);
    return adjusted_delay;
}
```

**Example**:
- Calibrated at 20°C with delay 16450
- Operating at 30°C
- Adjustment: 0.458 × (30 - 20) = +4.58 time units
- New delay: 16450 + 5 = 16455

**Impact**:
- 10°C change → 2.15 cm distance error per device
- With TAG + ANCHOR: 10°C → **4.3 cm total error**
- For ±5cm accuracy: temperature must stay within ±12°C of calibration

### Distance to Time Unit Conversion

**Forward Conversion** (distance → time units):
```cpp
uint16_t distance_to_time_units(float distance_m) {
    return (uint16_t)(distance_m * 213.14);
}
```

**Reverse Conversion** (time units → distance):
```cpp
float time_units_to_distance(uint16_t time_units) {
    return time_units * 0.0046917639786159;  // DISTANCE_OF_RADIO constant
}
```

### Error Budget Calculation

**Root Sum Square** for total expected error:
```cpp
float calculate_total_error(float antenna_delay_error,
                           float clock_drift_error,
                           float temp_error,
                           float multipath_error,
                           float interrupt_latency,
                           float quantization_error) {
    // RSS = sqrt(sum of squares)
    float rss = sqrt(
        antenna_delay_error * antenna_delay_error +
        clock_drift_error * clock_drift_error +
        temp_error * temp_error +
        multipath_error * multipath_error +
        interrupt_latency * interrupt_latency +
        quantization_error * quantization_error
    );
    return rss;
}
```

**Example** (Arduino Uno after calibration):
- Antenna delay: ±1 cm (calibrated)
- Clock drift: <1 cm (DS-TWR compensates)
- Temperature: ±2 cm (indoor, no compensation)
- Multipath: ±3 cm (LOS + filtering)
- Interrupt latency: ±2 cm (Arduino Uno limitation)
- Quantization: ±1 cm (hardware)
- **Total**: sqrt(1² + 1² + 2² + 3² + 2² + 1²) = **4.5 cm**

**Conclusion**: ±10cm target is achievable with margin.

---

## 5. Best Practices and Tips

### Calibration Environment

**Optimal Conditions**:
- **Indoor**: Climate-controlled, minimal RF interference
- **Clear space**: 5+ meters with no obstacles
- **Temperature**: Stable (±2°C during calibration)
- **Surfaces**: Non-reflective (avoid metal walls/floors)
- **Orientation**: Antennas aligned (both vertical or horizontal)

**Avoid**:
- Near WiFi routers, microwaves, Bluetooth devices
- Metal structures causing reflections
- Moving people or objects
- Temperature gradients (near windows, HVAC)

### Measurement Best Practices

**Distance Measurement**:
- Use metal tape measure (no sag)
- Measure antenna center to antenna center
- Accuracy: ±1mm minimum
- Verify measurement twice

**Sample Collection**:
- Minimum: **30 measurements**
- Typical: **100 measurements**
- High-precision: **1000 measurements**
- Discard initial 5-10 measurements (settling time)

**Outlier Filtering**:
```cpp
bool is_valid_measurement(float measured, float expected) {
    float error = abs(measured - expected);
    // Reject if error > 50% of expected distance
    return error < (expected * 0.5);
}
```

### Temperature Management

**Record Calibration Temperature**:
```cpp
float CALIBRATION_TEMP = 20.0;  // °C - DOCUMENT THIS
uint16_t BASE_ANTENNA_DELAY = 16450;  // Calibrated at CALIBRATION_TEMP
```

**For Variable Environments**:
- Calibrate at mid-range expected temperature
- Implement runtime temperature compensation
- Use external temperature sensor for better accuracy
- Consider periodic recalibration

**Temperature Sources**:
1. DW1000 internal temperature sensor (if library supports)
2. External sensor (DS18B20, BME280)
3. Environmental monitoring system

### Signal Quality Considerations

**Check Signal Strength**:
```cpp
float rxPower = DW1000.getReceivePower();      // Total received power
float fpPower = DW1000.getFirstPathPower();    // First path power

// Quality thresholds
if (rxPower < -90.0) {
    // Signal too weak - discard measurement
    return false;
}

float power_ratio = fpPower - rxPower;
if (power_ratio < -5.0) {
    // Significant multipath - flag or discard
    return false;
}
```

**Power Level Guidelines**:
- Good: rxPower > -80 dBm
- Acceptable: -80 to -90 dBm
- Poor: < -90 dBm (discard)

### Multi-Distance Validation Protocol

**Test Distances**:
- 0.5 m (near-field test)
- 1.0 m (primary calibration)
- 2.0 m (mid-range)
- 3.0 m (extended)
- 5.0 m (far-range validation)

**Expected Error Pattern**:
- **Good calibration**: Constant small offset (< 5cm) at all distances
- **Needs refinement**: Constant larger offset (adjust antenna delay)
- **Problem**: Error increases with distance (clock drift - unlikely with DS-TWR)

**Example Good Calibration**:
```
Distance    Measured    Error    Std Dev
--------    --------    -----    -------
0.5 m       0.51 m      +1 cm    0.8 cm
1.0 m       1.01 m      +1 cm    1.1 cm
2.0 m       2.01 m      +1 cm    1.5 cm
5.0 m       5.02 m      +2 cm    2.3 cm

Result: EXCELLENT - Constant offset, minimal
```

### Documentation Requirements

**Record for Each Calibrated Device**:
- Device serial number or ID
- Calibration date
- Ambient temperature during calibration
- Calibrated antenna delay value
- Test distance used
- Average error after calibration
- Multi-distance validation results
- Hardware configuration (channel, PRF, mode)

**Example Label**:
```
Device: TAG-001
Antenna Delay: 16459
Cal Date: 2026-01-11
Cal Temp: 20°C
Cal Dist: 1.0m
Error: ±0.8cm
```

### Storage Options

**Option 1: EEPROM** (permanent storage):
```cpp
#include <EEPROM.h>

void saveAntennaDelay(uint16_t delay) {
    EEPROM.write(0, delay & 0xFF);
    EEPROM.write(1, (delay >> 8) & 0xFF);
}

uint16_t loadAntennaDelay() {
    uint16_t delay = EEPROM.read(0);
    delay |= (EEPROM.read(1) << 8);

    // Sanity check
    if (delay < 16300 || delay > 16600) {
        return 16450;  // Use default if invalid
    }
    return delay;
}
```

**Option 2: Hard-coded** (unique sketch per device):
```cpp
#define DEVICE_ID 1

#if DEVICE_ID == 1
    #define ANTENNA_DELAY 16455
#elif DEVICE_ID == 2
    #define ANTENNA_DELAY 16462
#endif
```

### Channel-Specific Calibration

**Antenna Delay Variation by Channel**: ±5-10 time units

**When to Use**:
- High-precision applications (< 5cm accuracy)
- Fixed-channel operation
- Production systems

**Procedure**:
- Calibrate on each channel you plan to use
- Store channel-specific delays
- Apply appropriate delay when switching channels

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

// Apply channel-specific delay
uint8_t channel = 5;
DW1000.setChannel(channel);
DW1000.setAntennaDelay(antenna_delays[channel]);
```

---

## 6. Advanced Techniques

### Temperature Compensation Implementation

**Full Implementation**:
```cpp
// Calibration constants (store these)
const float CALIBRATION_TEMP = 20.0;  // °C
const uint16_t BASE_ANTENNA_DELAY = 16450;

// Runtime temperature compensation
void setup() {
    // Read current temperature
    float current_temp = readTemperature();  // From sensor

    // Calculate temperature difference
    float temp_diff = current_temp - CALIBRATION_TEMP;

    // Apply compensation (0.458 time units per °C)
    float temp_correction = 0.458 * temp_diff;
    uint16_t adjusted_delay = BASE_ANTENNA_DELAY + (int16_t)temp_correction;

    // Set compensated delay
    DW1000.setAntennaDelay(adjusted_delay);

    Serial.print("Temperature: "); Serial.print(current_temp);
    Serial.print("°C, Adjusted delay: "); Serial.println(adjusted_delay);
}
```

### Power-Correlated Bias Correction

**Research Finding**: Range error correlates with received signal power

**Implementation**:
```cpp
float measure_distance_with_power_correction() {
    float raw_distance = performRanging();
    float rxPower = DW1000.getReceivePower();  // dBm

    // Empirical power correction (calibrate per system)
    float power_correction = 0.0;
    if (rxPower < -85.0) {
        power_correction = 0.05;  // +5 cm for weak signals
    } else if (rxPower < -80.0) {
        power_correction = 0.02;  // +2 cm
    }

    return raw_distance - power_correction;
}
```

### NLOS Detection

**Method 1: Power Ratio**
```cpp
bool is_los_measurement() {
    float fpPower = DW1000.getFirstPathPower();
    float rxPower = DW1000.getReceivePower();
    float ratio = fpPower - rxPower;  // dB

    // LOS: ratio should be small (< 3 dB)
    // NLOS: significant multipath, ratio < -5 dB
    return (ratio > -5.0);
}
```

**Method 2: Variance-Based**
```cpp
float previous_distance = 0;

bool is_los_variance_check(float current_distance) {
    float diff = current_distance - previous_distance;
    float variance = diff * diff;
    previous_distance = current_distance;

    // High variance indicates NLOS or movement
    return (variance < 0.01);  // 10cm variance threshold
}
```

### Distance-Dependent Correction

**For sub-5cm accuracy at all distances**:
```cpp
float apply_distance_correction(float measured_distance) {
    // Polynomial correction based on empirical data
    float d = measured_distance;

    // Coefficients determined through multi-distance calibration
    float a1 = -0.005;  // Linear correction
    float a2 = 0.0001;  // Quadratic correction

    float corrected = d + (a1 * d) + (a2 * d * d);
    return corrected;
}
```

### Register Optimization

**Critical DW1000 Register Settings**:

**AGC_TUNE1** (Automatic Gain Control):
- Default: 0x889B
- Optimal for 16MHz PRF: **0x8870**

**DRX_TUNE2** (Digital Receiver Tuning):
- Default: 0x311E0035
- Optimal for default PRF and PAC: **0x311A002D**

**Impact**: Better receiver performance, more stable ranging

**Note**: Check if your DW1000 library already applies these settings.

---

## 7. Tools and Methods

### Hardware Tools

**Required**:
1. **Tape measure**: ±1mm accuracy, 5+ meters, metal (no sag)
2. **Two DW1000 modules**: DWM1000 or compatible
3. **MCU platforms**: Arduino Uno / ESP32
4. **USB cables**: For programming and serial monitoring

**Optional**:
5. **Laser distance meter**: For precise distance measurement
6. **Tripods or stands**: Repeatable positioning
7. **Temperature sensor**: DS18B20 or BME280
8. **Test fixtures**: Production calibration

### Software Tools

**Arduino Libraries**:
- arduino-dw1000 (thotro original)
- arduino-dw1000-ng (F-Army enhanced)

**Calibration Sketches**:
- Manual iterative calibration (basic)
- Binary search auto-calibration (advanced)
- Three-node calibration (production)

**Analysis Tools**:
- Serial plotter (real-time visualization)
- Python/MATLAB scripts (statistical analysis)
- Excel spreadsheet (data logging)

### Reference Implementations

**Open Source Projects**:

1. **jremington/UWB-Indoor-Localization_Arduino**
   - Binary search auto-calibration
   - ESP32_anchor_autocalibrate.ino
   - Achieves ±10cm accuracy

2. **lowfet/AntennaDelayCalibration**
   - MATLAB-based calibration tools
   - Multiple calibration methods
   - Statistical analysis

3. **Makerfabs ESP32-UWB**
   - Practical calibration examples
   - Real-world validation
   - Community-tested

### Official Documentation

**Qorvo/Decawave Resources**:

1. **APS014**: "Antenna Delay Calibration of DW1000-Based Products and Systems"
   - Official calibration procedure
   - Three-node method
   - Temperature considerations

2. **DW1000 User Manual**:
   - Section 7.2.40: Antenna Delay Register
   - Section 8.3.1: Calibration Method
   - Chapter 12: Calibration and Testing

3. **Application Notes**:
   - APS006: Channel Effects on Range
   - Metrics for NLOS Estimation

### Measurement Equipment

**Distance Measurement**:
- Optical: Laser rangefinder (±1mm)
- Mechanical: Precision rail system
- Manual: Quality tape measure (±1mm)

**Environmental Monitoring**:
- Temperature: ±0.5°C accuracy
- Humidity: Optional (affects RF slightly)
- Air pressure: Optional (advanced)

### Production Test Fixtures

**For Manufacturing**:
- Precision positioning rails
- Automated test stations
- Golden reference units
- Pass/fail criteria
- Database logging

---

## 8. Troubleshooting

### Issue 1: Very Large Error (>1 meter)

**Symptoms**:
- Measured distance much longer than actual
- Example: 1.0m reads as 1.8m

**Possible Causes**:
1. Antenna delay not set (defaults to 0)
2. Wrong device addresses
3. ToF calculation error

**Solutions**:
```cpp
// Verify antenna delay is set BEFORE commitConfiguration()
DW1000.setAntennaDelay(16450);
DW1000.commitConfiguration();

// Verify different device addresses
DW1000.setDeviceAddress(1);  // ANCHOR
DW1000.setDeviceAddress(2);  // TAG

// Verify same network ID
DW1000.setNetworkId(10);
```

### Issue 2: Unstable Measurements (High Variance)

**Symptoms**:
- Standard deviation > 10cm
- Measurements jump wildly

**Possible Causes**:
1. RF interference
2. Multipath reflections
3. Poor antenna orientation
4. Low signal power

**Solutions**:
```cpp
// Check signal quality
float rxPower = DW1000.getReceivePower();
if (rxPower < -90.0) {
    // Signal too weak
}

// Filter by signal quality
float fpPower = DW1000.getFirstPathPower();
float ratio = fpPower - rxPower;
if (ratio < -5.0) {
    // Multipath detected
}

// Try different channel
DW1000.setChannel(5);  // Less interference
```

### Issue 3: Distance-Proportional Error

**Symptoms**:
- Error increases linearly with distance
- Example: 1m → +2cm, 5m → +10cm

**Possible Causes**:
1. Clock drift (rare with DS-TWR)
2. Crystal oscillator issue
3. Incorrect ToF formula

**Solutions**:
- Verify using asymmetric DS-TWR
- Check crystal oscillator (should be 38.4 MHz ±10 ppm)
- Try different modules (may be hardware defect)

### Issue 4: Calibration Won't Converge

**Symptoms**:
- Adjusting antenna delay doesn't improve error
- Error changes unpredictably

**Possible Causes**:
1. Incorrect distance measurement
2. Antennas misaligned
3. Environmental interference

**Solutions**:
- Verify tape measure accuracy
- Measure from antenna center, not board edge
- Try different calibration distance
- Ensure both devices have same antenna delay
- Test in different location

### Issue 5: Temperature Drift

**Symptoms**:
- Calibration accurate at 20°C
- Reads +5cm error at 30°C

**Solution**:
- Implement temperature compensation (Section 6)
- Calibrate at operating temperature
- Use temperature sensor for runtime adjustment

### Issue 6: Antenna Delay Out of Range

**Symptoms**:
- Calibrated value < 16300 or > 16600

**Possible Causes**:
1. Wrong distance measurement
2. Hardware defect
3. Wrong RF configuration

**Solutions**:
- Double-check actual distance
- Verify antenna connection
- Try different RF mode
- Contact manufacturer if persistent

### Issue 7: Channel-Dependent Variation

**Symptoms**:
- Calibrated on Channel 5
- Error when using Channel 2

**Solution**:
- Antenna delay varies by channel (±5-10 units)
- Calibrate on the channel you'll use
- Or maintain channel-specific delays

### Issue 8: Asymmetric Device Delays

**Symptoms**:
- TAG A + ANCHOR B → calibrated 16460
- TAG A + ANCHOR C → calibrated 16490

**Explanation**:
- Each device has unique antenna delay
- Pairwise calibration finds average

**Solution**:
- Use three-node method for individual delays
- Or accept averaged values (sufficient for most applications)

---

## Expected Antenna Delay Values - Quick Reference

| Hardware | Typical Range | Default | Notes |
|----------|---------------|---------|-------|
| DWM1000 | 16400-16500 | 16384 | Most common modules |
| ESP32-UWB | 16450-16465 | 16450 | Makerfabs modules |
| Custom PCB | 16300-16600 | 16450 | Wider variance |
| Decawave EVK | 16430-16440 | 16436 | Official eval kit |

---

## Accuracy Expectations by Platform

| Platform | Short (<3m) | Medium (3-5m) | Long (5-10m) |
|----------|-------------|---------------|--------------|
| Arduino Uno | ±5-10 cm | ±10-15 cm | ±15-20 cm |
| ESP32 | ±2-5 cm | ±5-10 cm | ±10-15 cm |
| ESP32 + Opt | ±2-3 cm | ±3-5 cm | ±5-10 cm |

*Opt = Temperature compensation + Signal filtering + NLOS detection

---

## Summary Checklist

### Basic Calibration (±10cm)
- [ ] Position devices at known distance (1.0m)
- [ ] Set initial antenna delay (16450)
- [ ] Collect 100+ measurements
- [ ] Calculate average error
- [ ] Adjust antenna delay using formula
- [ ] Verify error < 5cm
- [ ] Test at multiple distances
- [ ] Document calibrated value

### Advanced Calibration (±5cm)
- [ ] Basic calibration complete
- [ ] Implement temperature compensation
- [ ] Add signal quality filtering
- [ ] Enable NLOS detection
- [ ] Optimize register settings
- [ ] Multi-distance validation
- [ ] Distance-dependent correction (optional)

### Production Calibration
- [ ] Three-node calibration method
- [ ] Individual device delays
- [ ] Factory test fixture
- [ ] Automated calibration
- [ ] Quality control criteria
- [ ] Database logging
- [ ] Device labeling

---

## Key Formulas - Quick Reference

**Antenna Delay Adjustment**:
```
new_delay = current_delay + (error_m / 2.0) * 213.14
```

**Temperature Compensation**:
```
adjusted_delay = base_delay + 0.458 × (temp_current - temp_cal)
```

**Distance Conversion**:
```
meters = time_units × 0.00469
time_units = meters × 213.14
```

---

## Sources

**Primary References**:
1. Qorvo APS014 - Antenna Delay Calibration of DW1000 Products
2. DW1000 User Manual (v2.0+)
3. IEEE 802.15.4a UWB PHY Standard

**Academic Research**:
4. "Calibration and Uncertainty Characterization for UWB TWR" (arXiv:2210.05888)
5. "Antenna Delay Calibration of UWB Nodes" (IEEE Xplore)
6. "Data-Driven Antenna Delay Calibration" (ResearchGate 2024)

**Community Resources**:
7. jremington/UWB-Indoor-Localization_Arduino (GitHub)
8. Qorvo Tech Forum - Antenna Delay Discussions
9. Makerfabs ESP32-UWB Calibration Guide

---

**Document Version**: 1.0
**Last Updated**: 2026-01-11
**Author**: SwarmLoc Project
**License**: MIT

This document synthesizes current best practices from official documentation, academic research, and practical implementations to provide comprehensive guidance for achieving ±10cm or better ranging accuracy with DW1000 UWB modules.
