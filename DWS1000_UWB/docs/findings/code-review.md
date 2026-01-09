# DWS1000 UWB Code Review Findings

## Date: 2026-01-08

## Hardware Setup
- **Module**: Qorvo PCL298336 v1.3 (Arduino Shield/Hat)
- **Color**: White
- **MCU**: 2x Arduino Uno
- **Connection**: Direct shield connection to Arduino Uno headers

## Current Code Analysis

### Overview
The current implementation consists of two Arduino sketches:
- `initiator.ino` - Sends ranging requests and calculates distance
- `responder.ino` - Responds to ranging requests

### Critical Issues Identified

#### 1. INCOMPLETE TWR IMPLEMENTATION
**Severity**: CRITICAL

The initiator's Two-Way Ranging (TWR) implementation is incomplete:

**Location**: [initiator.ino:113-127](../initiator/initiator.ino#L113-L127)

```cpp
float calculateDistance() {
  int64_t round1 = (timePollAckReceived - timePollSent);
  int64_t reply1 = (timePollAckSent - timePollReceived);
  int64_t round2 = (timeRangeReceived - timePollAckSent);
  int64_t reply2 = (timeRangeSent - timePollAckReceived);
  // ...
}
```

**Problems**:
- Timestamp variables are declared but NEVER populated from actual DW1000 readings
- No code to retrieve timestamps from the DW1000 chip after TX/RX events
- Variables remain at default values (0), making distance calculation meaningless
- Missing the 3-message exchange required for TWR (Poll, Poll_Ack, Range)

#### 2. MISSING INTERRUPT HANDLERS
**Severity**: CRITICAL

**Location**: [initiator.ino:96-99](../initiator/initiator.ino#L96-L99)

The interrupt handler only calls `DW1000.handleInterrupt()` but doesn't:
- Set the `receivedAck` flag when poll acknowledgment arrives
- Set the `error` flag on communication errors
- Read received data or timestamps
- Distinguish between different message types

**Impact**: The main loop waits for `receivedAck` which is never set to true, so ranging never progresses.

#### 3. INCOMPLETE MESSAGE PROTOCOL
**Severity**: HIGH

**Current Implementation**:
- Initiator sends Poll message (line 106-110)
- Responder sends Poll_Ack (line 102-110)
- Missing: Initiator should send Range message with timestamps
- Missing: Responder should send Range_Report with final timestamp

**Standard TWR requires**:
1. Initiator → Responder: POLL (capture T1)
2. Responder → Initiator: POLL_ACK (capture T2, T3)
3. Initiator → Responder: RANGE (capture T4, includes T1, T4)
4. Responder → Initiator: RANGE_REPORT (includes T2, T3)
5. Initiator calculates distance using all 4 timestamps

#### 4. NO TIMESTAMP CAPTURE
**Severity**: CRITICAL

**Missing functionality**:
- `DW1000.getTransmitTimestamp()` called at line 110 but result not properly used
- No `DW1000.getReceiveTimestamp()` calls
- No code to extract timestamps from received messages
- Timestamps must be captured immediately after TX/RX events in interrupt handler

#### 5. HARDCODED MESSAGE FRAMES
**Severity**: MEDIUM

Messages use raw byte arrays without structure:
```cpp
byte pollMsg[] = {0x61, 0x88, 0, 0xCA, 0xDE, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00};
```

**Issues**:
- Magic numbers with no documentation
- No structure for IEEE 802.15.4 frame format
- Difficult to modify or debug
- No payload section for timestamp data

#### 6. RESPONDER LOGIC TOO SIMPLE
**Severity**: HIGH

**Location**: [responder.ino:82-93](../responder/responder.ino#L82-L93)

The responder:
- Doesn't verify message type or source address
- Doesn't capture receive/transmit timestamps
- Doesn't participate in the full TWR protocol
- Missing range message handling

#### 7. LIBRARY COMPATIBILITY UNKNOWN
**Severity**: HIGH

**Current library**: `DW1000.h` (likely thotro/arduino-dw1000)

**Concerns**:
- Overview document mentions PCL298336 might be DWM3000-based
- DW1000 vs DW3000 are different chips requiring different libraries
- Need to verify actual chip on the board
- May need to switch to DWM3000 library if applicable

#### 8. NO ERROR HANDLING OR DIAGNOSTICS
**Severity**: MEDIUM

**Missing**:
- SPI communication verification
- Chip ID verification at startup
- Timeout handling for messages
- Detailed error codes/messages
- Status LED indicators
- Diagnostic output for debugging

#### 9. TIMING CONCERNS
**Severity**: MEDIUM

**Issues**:
- `delay(1000)` between ranging attempts (line 82) - too long for real-time applications
- No timeout mechanism if response never arrives
- `delay(100)` in responder might miss incoming messages
- Should use non-blocking timing

#### 10. CONFIGURATION AMBIGUITY
**Severity**: LOW

**Location**: [initiator.ino:54-59](../initiator/initiator.ino#L54-L59)

```cpp
DW1000.setDeviceAddress(1);
DW1000.setNetworkId(10);
DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
```

- Mode selection not explained
- No antenna delay calibration
- No channel/PRF configuration visible
- Power settings not optimized

## Root Cause Analysis

### Why Communication Appears to Work But No Measurements

1. **Initialization succeeds**: Both devices initialize and configure the DW1000 chip correctly
2. **Messages transmit**: Poll and Poll_Ack messages are sent
3. **Reception fails silently**: Interrupt handler doesn't properly process received messages
4. **Flags never set**: `receivedAck` never becomes true, so distance calculation never runs
5. **Incomplete protocol**: Even if it ran, timestamps are all zero

### Why Distance Measurement Fails

1. **No timestamp capture**: Critical timing data never retrieved from chip
2. **Incomplete TWR**: Only 2 of 4 required message exchanges implemented
3. **Wrong formula application**: Formula uses uninitialized variables

## Recommendations

### Immediate Actions

1. **Verify Hardware**: Confirm if PCL298336 is DWM1000 or DWM3000 based
2. **Library Selection**: Use correct library for the actual chip
3. **Implement Complete TWR**: Follow standard Double-Sided Two-Way Ranging protocol
4. **Fix Interrupt Handlers**: Properly handle RX/TX done interrupts
5. **Capture Timestamps**: Read and store all 4 required timestamps

### Code Architecture

1. **Message Structure**: Define proper IEEE 802.15.4 frame structures
2. **State Machine**: Implement proper state machine for TWR protocol
3. **Error Handling**: Add comprehensive error detection and recovery
4. **Diagnostics**: Add verbose debug output mode
5. **Testing**: Create unit tests for message parsing and timing calculations

### PlatformIO Migration Benefits

1. **Better dependency management**: Specify exact library versions
2. **Multiple environments**: Easy testing with different configurations
3. **Build flags**: Enable/disable debug output
4. **Serial monitoring**: Built-in serial monitor with proper formatting
5. **OTA updates**: Potential for wireless updates

## Expected Accuracy

Based on DWM1000/DWM3000 specifications:
- **Ideal conditions**: ±10 cm (3σ)
- **Typical conditions**: ±20-30 cm
- **With antenna delay calibration**: ±5-10 cm achievable
- **Limiting factors**: Clock drift, multipath, antenna delay, temperature

## Next Steps

1. ✓ Create documentation structure
2. Research PCL298336 specifics (in progress)
3. Create roadmap document
4. Design proper TWR implementation
5. Migrate to PlatformIO with correct libraries
6. Implement complete protocol
7. Add calibration procedures
8. Test and validate measurements
