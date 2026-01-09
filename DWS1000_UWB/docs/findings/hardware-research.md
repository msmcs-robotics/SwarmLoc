# Hardware Research Findings: Qorvo PCL298336

## Date: 2026-01-08

## Executive Summary

The Qorvo PCL298336 v1.3 is an Arduino shield for UWB ranging. Based on documentation review, there is **critical ambiguity** about whether this module uses DWM1000 or DWM3000 chip, which determines which library must be used.

## Module Identification

### What We Know
- **Product Code**: PCL298336 v1.3
- **Brand**: Qorvo
- **Form Factor**: Arduino shield (white color)
- **Interface**: Direct plug-in to Arduino Uno headers
- **Purpose**: UWB Time-of-Flight ranging

### DWM1000 vs DWM3000 Ambiguity

**Evidence for DWM1000:**
- Current code uses `#include <DW1000.h>` library
- Project folder named "DWS1000_UWB"
- Initial documentation references DWS1000

**Evidence for DWM3000:**
- Documentation states: "The Qorvo PCL298336 module shield (which appears to be the DWM3000EVB Arduino shield)"
- PCL298336 may be newer generation
- Documentation mentions library compatibility issues

**ACTION REQUIRED**: Physical inspection of the chip markings on the PCL298336 board is needed to definitively identify which chip is present.

## Pinout Configuration

### Standard Arduino Shield Pins
Based on typical DWM1000/DWM3000 shield configuration:

| Function | Arduino Uno Pin | Notes |
|----------|-----------------|-------|
| MOSI | D11 | SPI Master Out Slave In |
| MISO | D12 | SPI Master In Slave Out |
| SCK | D13 | SPI Clock |
| CS/SS | D10 | Chip Select (may be D7 on some versions) |
| RST | D9 | Hardware Reset |
| IRQ | D2 | Interrupt Request |
| VCC | 3.3V | Power supply |
| GND | GND | Ground |

**Note**: Since this is a shield, all connections are automatic when plugged into Arduino Uno.

## Library Options

### Option 1: DW1000 Library (if module is DWM1000-based)
- **Repository**: https://github.com/thotro/arduino-dw1000
- **Status**: Mature, widely used
- **Compatibility**: DWM1000, DWS1000 modules
- **Current code status**: Already using this library

### Option 2: DWM3000 Library (if module is DWM3000-based)
- **Repository**: https://github.com/foldedtoad/dwm3000
- **Status**: Port of Qorvo/Decawave's DWM3000 code
- **Compatibility**: DWM3000, DWM3000EVB modules
- **Warning**: Documentation notes user reports of difficulty with this library

## Technical Specifications

### Common Specifications (DWM1000/DWM3000)
- **Frequency Range**: 3.5 - 6.5 GHz
- **Channels**: 6 UWB channels
- **Standard**: IEEE 802.15.4-2011 UWB
- **Interface**: SPI
- **Supply Voltage**: 3.3V
- **Target Accuracy**: 5-10 cm (ToF)

### Typical Accuracy Performance
- **Ideal conditions**: ±10 cm (3σ)
- **Typical indoor**: ±20-30 cm
- **With calibration**: ±5-10 cm achievable
- **Limiting factors**:
  - Clock drift between devices
  - Multipath interference
  - Antenna delay variations
  - Temperature effects
  - RF environment

## Two-Way Ranging (TWR) Protocol

### Standard DS-TWR (Double-Sided Two-Way Ranging)

```
Initiator          Responder
    |                  |
    |------ POLL ----->|  T1 (TX)    T2 (RX)
    |                  |
    |<--- POLL_ACK ----|  T4 (RX)    T3 (TX)
    |                  |
    |----- RANGE ----->|  T5 (TX)    T6 (RX)
    |                  |
    |<-- RANGE_REPORT -|  T8 (RX)    T7 (TX)
    |                  |

Time of Flight = [(T4-T1) - (T3-T2)] / 2
```

### Required Timestamps
1. **T1**: Initiator transmits POLL
2. **T2**: Responder receives POLL
3. **T3**: Responder transmits POLL_ACK
4. **T4**: Initiator receives POLL_ACK
5. **T5**: Initiator transmits RANGE (with T1, T4)
6. **T6**: Responder receives RANGE
7. **T7**: Responder transmits RANGE_REPORT (with T2, T3, T6)
8. **T8**: Initiator receives RANGE_REPORT

### Distance Calculation Formula

```
Round1 = T4 - T1  (time on initiator)
Reply1 = T3 - T2  (time on responder)
Round2 = T8 - T5  (time on initiator)
Reply2 = T7 - T6  (time on responder)

ToF = (Round1 * Round2 - Reply1 * Reply2) / (Round1 + Round2 + Reply1 + Reply2)
Distance = ToF * SPEED_OF_LIGHT
```

### Conversion Constants
- Speed of light in air: ~299,702,547 m/s
- DW1000 time unit: 15.65 ps (1/(499.2 MHz * 128))
- Distance per time unit: ~0.0046917 m
- Conversion factor in code: `0.0154689` (appears incorrect, should verify)

## Message Frame Structure

### IEEE 802.15.4 Frame Format
```
| Frame Control (2) | Sequence (1) | PAN ID (2) | Dest Addr (2) | Src Addr (2) | Payload (var) | CRC (2) |
```

Current code uses:
```cpp
byte pollMsg[] = {0x61, 0x88, 0, 0xCA, 0xDE, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00};
//                  FC1   FC2  Seq  PAN-L PAN-H Dst-L Dst-H Src-L Src-H  Payload...
```

## Power and Performance Modes

### Available Modes (DW1000 library)
- `MODE_LONGDATA_RANGE_LOWPOWER` - Current code uses this
- `MODE_SHORTDATA_FAST_LOWPOWER`
- `MODE_LONGDATA_FAST_LOWPOWER`
- `MODE_SHORTDATA_FAST_ACCURACY`
- `MODE_LONGDATA_FAST_ACCURACY`
- `MODE_LONGDATA_RANGE_ACCURACY`

### Mode Selection Trade-offs
- **Range**: Longer preamble, better sensitivity, slower
- **Speed**: Shorter preamble, faster data rate, less range
- **Power**: Lower transmit power, reduced range
- **Accuracy**: Higher data rate, better timing resolution

## Initialization Sequence

### Recommended Startup Process
```cpp
// 1. Hardware Reset (essential)
pinMode(PIN_RST, OUTPUT);
digitalWrite(PIN_RST, LOW);
delay(10);
digitalWrite(PIN_RST, HIGH);
delay(10);

// 2. SPI Initialization
DW1000.begin(PIN_IRQ, PIN_RST);
DW1000.select(PIN_SS);

// 3. Verify chip communication
// Read device ID register to confirm communication
// Expected: 0xDECA0130 (DWM1000) or different for DWM3000

// 4. Configuration
DW1000.newConfiguration();
DW1000.setDefaults();
DW1000.setDeviceAddress(deviceID);
DW1000.setNetworkId(networkID);
DW1000.enableMode(selectedMode);

// Optional but recommended:
// DW1000.setAntennaDelay(antennaDelay);
// DW1000.setChannel(channel);
// DW1000.setPreambleLength(length);

DW1000.commitConfiguration();

// 5. Setup interrupt handler
attachInterrupt(digitalPinToInterrupt(PIN_IRQ), handleInterrupt, RISING);

// 6. Start operation
// Initiator: Start first ranging
// Responder: Enter receive mode
```

## Known Issues and Solutions

### Issue 1: Library Compatibility
- **Problem**: Wrong library for actual chip
- **Detection**: Device ID mismatch, no communication
- **Solution**: Read chip markings, use correct library

### Issue 2: Antenna Delay Calibration
- **Problem**: Systematic distance offset
- **Solution**: Measure known distance, adjust antenna delay value
- **Typical range**: 16400-16500 for DWM1000

### Issue 3: Clock Drift
- **Problem**: Distance errors over time
- **Solution**: Use Double-Sided TWR (already implemented)
- **Mitigation**: Regular re-ranging, temperature compensation

### Issue 4: SPI Communication Failures
- **Problem**: Intermittent communication
- **Causes**:
  - Incorrect CS pin
  - SPI speed too high
  - Power supply noise
- **Solution**: Add decoupling capacitors, verify SPI settings

## Recommended Testing Procedure

### Phase 1: Basic Communication
1. Verify chip ID register read
2. Confirm interrupt fires on chip events
3. Test basic transmit/receive
4. Monitor SPI traffic with logic analyzer (if available)

### Phase 2: Simple Ranging
1. Implement single-sided TWR first (simpler)
2. Verify timestamp capture
3. Test at known distances (1m, 2m, 5m)
4. Calculate systematic offset

### Phase 3: Calibration
1. Measure 10 known distances
2. Calculate antenna delay correction
3. Apply calibration
4. Re-test accuracy

### Phase 4: Double-Sided TWR
1. Implement full DS-TWR protocol
2. Test clock drift compensation
3. Measure accuracy at various distances
4. Test in different environments

## Resources

### Official Documentation
- DWM1000 Datasheet: Search for "DW1000 User Manual"
- DWM3000 Datasheet: Available from Qorvo website
- IEEE 802.15.4-2011 Standard: UWB PHY/MAC specification

### GitHub Repositories
- arduino-dw1000: https://github.com/thotro/arduino-dw1000
- dwm3000: https://github.com/foldedtoad/dwm3000
- Example projects: Search GitHub for "DWM1000 ranging arduino"

### Purchase Links
- [DWS1000 on DigiKey](https://www.digikey.com/en/products/detail/qorvo/DWS1000/12088519)
- [DWM3000EVB on Qorvo](https://www.qorvo.com/products/p/DWM3000EVB)

## Critical Next Steps

1. **VERIFY CHIP TYPE**: Physically inspect PCL298336 to determine if DWM1000 or DWM3000
2. **Test current code**: Upload existing code to see if basic communication works
3. **Monitor serial output**: Check if initialization succeeds
4. **Library decision**: Based on chip type, confirm or change library
5. **Complete TWR implementation**: Fix timestamp capture and protocol
