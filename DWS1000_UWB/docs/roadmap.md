# DWS1000_UWB Project Roadmap

## Project Goal

Develop a **UWB-based communication and ranging system for drone swarms** using DW1000 UWB radios with Arduino Uno, enabling:

1. **Multilateration** for GPS-denied positioning
2. **Collision avoidance** through distance measurement
3. **Simple message passing** for coordination
4. **Communication saturation mitigation** for scalable swarm operation

**Target Accuracy**: ±10-20 cm (realistic for Arduino Uno + DW1000)

## Hardware Configuration - UPDATED 2026-01-08

- **Module**: Qorvo PCL298336 v1.3 (DWM1000 Arduino Shield)
- **Chip**: **DW1000** (NOT DWM3000!) - Device ID: 0xDECA0130
- **MCU**: 2x Arduino Uno (ATmega328P @ 16MHz, 2KB RAM)
- **Connection**: Shield plugs directly into Arduino Uno headers
- **Target Accuracy**: ±10-20 cm (after calibration)
- **Backup Plan**: Migrate to ESP32 if performance insufficient

## Critical Discovery - Hardware Identification ✅

**MAJOR UPDATE**: PCL298336 v1.3 shields contain **DW1000 chips, NOT DWM3000!**

- Test 1 revealed Device ID: `0xDECA0130` (DW1000)
- Expected for DWM3000: `0xDECA0302` (DW3000)
- **Impact**: Original library choice (`#include <DW1000.h>`) was CORRECT!
- **Good News**: DW1000 has better Arduino Uno support than DWM3000
- **Library**: arduino-dw1000 v0.9 installed and verified ✅

See [CRITICAL_HARDWARE_DISCOVERY.md](findings/CRITICAL_HARDWARE_DISCOVERY.md) for details.

## Development Strategy

### Primary Path: Arduino Uno Development (ACCEPTED CONSTRAINTS)

**Goal**: Achieve working TWR on Arduino Uno and document actual performance.

**Accepted Performance:**
- Target accuracy: 20-50 cm (due to CPU limitations)
- This is ACCEPTABLE for this project phase
- Focus on: Getting it working, measuring actual accuracy, documenting findings
- Serial output format: `Distance: X.XX m (XXX cm) ±YY cm`

**Known Challenges:**
- Arduino Uno is underpowered (16MHz vs ESP32's 240MHz)
- Community reports TWR not working on ATmega328P
- We will debug and fix the TWR implementation
- Document lessons learned in findings folder

**Success Criteria:**
- ✓ Achieve basic SPI communication with DWM3000
- ✓ Successfully transmit and receive messages
- ✓ Implement full TWR protocol with timestamp capture
- ✓ Calculate distances and display on serial monitor
- ✓ Document achieved accuracy (even if 20-50cm)
- ✓ Show measurement ± tolerance in serial output

**This is the PRIMARY focus** - we will make Arduino Uno work!

## Drone Swarm Requirements - NEW 2026-01-08

### Use Case: UWB-Enabled Autonomous Drone Swarms

**Operational Scenarios:**

1. **GPS-Denied Positioning (Multilateration)**
   - Drones use UWB ranging to multiple anchors or peer drones
   - Calculate relative positions using trilateration/multilateration
   - Maintain formation without GPS signal
   - Typical environments: Indoor, urban canyons, under bridges

2. **Collision Avoidance**
   - Continuous distance monitoring between drones
   - Maintain minimum safe separation (e.g., 2-3 meters)
   - Alert or automatic maneuvering when threshold breached
   - Critical for dense swarm operations

3. **Swarm Coordination via Messaging**
   - Simple command/status messages between drones
   - Examples: "follow me", "hold position", "return to base"
   - Low-bandwidth requirements (< 1 KB/s per drone)
   - Not simultaneous with ranging - time-multiplexed

4. **Communication Saturation Mitigation**
   - Prevent RF congestion in dense swarms
   - Hardware-level: Channel selection, power control
   - Firmware-level: TDMA, CSMA/CA, priority scheduling
   - Goal: Support 5-10 drones in swarm with Arduino Uno

### Key Technical Questions (Under Investigation)

**Q1: Dual-Role Architecture?**

- Should each drone run firmware supporting BOTH initiator and responder roles?
- Or should drones have dedicated roles (anchors vs tags)?
- **Status**: Research agents investigating ⏳

**Q2: Message + Ranging Combination?**

- Can DW1000 alternate between messaging mode and ranging mode?
- Protocol for time-division between operations?
- **Status**: To be tested in upcoming phases

**Q3: Network Topology?**

- Star topology (one coordinator, N drones)?
- Mesh topology (peer-to-peer)?
- Hybrid approach?
- **Status**: Research agents investigating ⏳

**Q4: Scalability Limits?**

- How many drones can Arduino Uno + DW1000 support?
- When should we migrate to ESP32 for larger swarms?
- **Status**: To be determined through testing

### Implementation Strategy

#### Phase 1: Basic Building Blocks (Current Focus)

1. ✅ Verify basic TX/RX communication (Tests 3-4)
2. ✅ Verify TWR ranging capability (Tests 6-7)
3. ⏳ Test message passing with simple payloads
4. ⏳ Test ranging accuracy and calibration

#### Phase 2: Combined Operation

1. Implement time-division protocol (alternate messaging/ranging)
2. Test message reliability vs ranging accuracy trade-offs
3. Measure update rates for both operations
4. Document performance constraints

#### Phase 3: Multi-Node Testing

1. Test with 3 nodes (minimum for multilateration)
2. Implement TDMA or CSMA protocol
3. Measure communication saturation points
4. Optimize for swarm scalability

#### Phase 4: Drone Integration (Future)

1. Interface with flight controller
2. Real-time position estimation
3. Collision avoidance algorithm
4. Field testing with actual drones

### Research Findings (In Progress)

Comprehensive research findings will be documented in:

- `docs/findings/drone_swarm_architecture.md` (dual-role design)
- `docs/findings/communication_saturation.md` (mitigation strategies)
- `docs/findings/multilateration_implementation.md` (positioning algorithms)

**Research Status**: Agents currently investigating ⏳

### Future: ESP32 Migration

ESP32 migration is documented for future use but NOT the current focus.
See separate ESP32 migration guide in findings folder.

**Migration Requirements:**
- Purchase: 2x ESP32 DevKit boards ($10-20 total)
- Wiring: Manual connections from shield pins to ESP32 GPIOs
- Library: Use proven Fhilb/DW3000_Arduino library
- Wiring diagrams will be provided when needed

**Why ESP32:**
- Proven working TWR implementations
- 240MHz CPU handles timing requirements
- Active community support
- Achieves target 10cm accuracy

## Project Structure

### Single PlatformIO Project with Multiple Environments

```
DWS1000_UWB/
├── platformio.ini           # Multi-environment configuration
├── docs/
│   ├── roadmap.md          # This file
│   ├── findings/
│   │   ├── code-review.md
│   │   ├── hardware-research.md
│   │   └── web-research.md
│   └── wiring/             # ESP32 wiring diagrams (future)
├── src/
│   ├── initiator/
│   │   └── main.cpp       # Initiator code
│   └── responder/
│       └── main.cpp       # Responder code
├── lib/                    # Libraries
├── test_scripts/
│   ├── upload_initiator.sh
│   ├── upload_responder.sh
│   ├── monitor_both.sh
│   └── test_ranging.sh
└── README.md
```

**PlatformIO Configuration Strategy:**

```ini
[platformio]
default_envs = initiator

[env:initiator]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyUSB0  # Auto-detected
monitor_port = /dev/ttyUSB0
monitor_speed = 9600
lib_deps =
    ; DWM3000 library for ATmega328P

[env:responder]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyUSB1  # Auto-detected
monitor_port = /dev/ttyUSB1
monitor_speed = 9600
lib_deps =
    ; DWM3000 library for ATmega328P
```

**Benefits:**
- Single project with two environments
- Easy switching: `pio run -e initiator` or `pio run -e responder`
- Automated port detection
- Shared library management
- Separate serial monitoring per device

## Development Phases

### Phase 1: Environment Setup ✓ (In Progress)

**Goals:**
- [x] Comprehensive code review
- [x] Hardware research and identification
- [x] Web research on libraries and implementations
- [x] Create documentation structure
- [x] Create project roadmap
- [ ] Set up PlatformIO project structure
- [ ] Detect Arduino USB ports
- [ ] Configure platformio.ini

**Deliverables:**
- Documentation in `docs/` folder
- PlatformIO project structure
- Automated test scripts

### Phase 2: Library Integration and Basic Communication

**Goals:**
- Source and integrate DWM3000 library for ATmega328P
- Verify SPI communication with DWM3000 chips
- Read chip ID and verify it's DWM3000
- Test basic transmit functionality
- Test basic receive functionality
- Implement interrupt handling

**Success Criteria:**
- Both devices initialize successfully
- Can read DWM3000 chip ID register
- Initiator can transmit packets
- Responder can receive packets
- Interrupts fire correctly
- Serial debug output shows communication

**Risk Mitigation:**
- If library not found, port from existing sources
- If SPI fails, verify pin connections and speed settings
- Add extensive debug logging

### Phase 3: Two-Way Ranging Protocol Implementation

**Goals:**
- Implement DS-TWR message protocol (4-message exchange)
- Implement timestamp capture on TX/RX events
- Parse timestamps from DWM3000 registers
- Exchange timestamps between devices
- Implement state machine for protocol sequencing

**TWR Protocol:**
```
Initiator          Responder
    |                  |
    |------ POLL ----->|  T1 (TX)    T2 (RX)
    |                  |
    |<--- POLL_ACK ----|  T4 (RX)    T3 (TX)
    |                  |
    |----- RANGE ----->|  T5 (TX)    T6 (RX)
    |   (T1,T4,T5)     |
    |                  |
    |<-- RANGE_RPT ----|  T8 (RX)    T7 (TX)
    |   (T2,T3,T6,T7)  |
    |                  |
   Calculate distance using all timestamps
```

**Success Criteria:**
- All 4 messages exchange successfully
- All 8 timestamps captured correctly
- No lost messages in protocol sequence
- Stable state machine operation

**Known Challenges:**
- Arduino Uno may struggle with timing precision
- Interrupt latency may affect timestamp accuracy
- Memory constraints for storing timestamps
- This is where previous attempts failed

### Phase 4: Distance Calculation and Calibration

**Goals:**
- Implement ToF calculation from timestamps
- Convert ToF to distance in meters/centimeters
- Apply antenna delay calibration
- Filter outliers and average results
- Tune for accuracy

**Distance Calculation:**
```cpp
// Round trip times
Round1 = T4 - T1;  // Time at initiator
Reply1 = T3 - T2;  // Time at responder
Round2 = T8 - T5;  // Time at initiator
Reply2 = T7 - T6;  // Time at responder

// Time of Flight (cancels clock drift)
ToF = (Round1 * Round2 - Reply1 * Reply2) /
      (Round1 + Round2 + Reply1 + Reply2);

// Distance
float distance_m = ToF * DWT_TIME_UNITS * SPEED_OF_LIGHT;
float distance_cm = distance_m * 100.0;
```

**Constants:**
- DWT_TIME_UNITS = 1.0 / (499.2e6 * 128) = 15.65 ps
- SPEED_OF_LIGHT = 299702547 m/s
- Distance per tick ≈ 0.004691754 m

**Calibration:**
1. Test at known distances (1m, 2m, 3m, 5m)
2. Measure systematic offset
3. Calculate antenna delay correction
4. Apply calibration factor
5. Verify accuracy across range

**Success Criteria:**
- Distance calculations produce non-zero values
- Results are in reasonable range (not negative, not absurdly large)
- Repeated measurements at same distance show consistency
- After calibration: 5-10 cm accuracy at 1-5 meter range

**Uncertainty Analysis:**
- Expected accuracy: ±10 cm (3σ) ideal
- Arduino Uno limitations may reduce to ±20-30 cm
- Document actual achieved accuracy
- Identify limiting factors

### Phase 5: Testing and Optimization

**Goals:**
- Create automated test scripts
- Test at multiple distances
- Test in different environments
- Optimize ranging update rate
- Optimize power consumption
- Create diagnostic tools

**Test Procedures:**
1. **Basic Communication Test**
   - Verify initialization
   - Check message exchange
   - Monitor error rates

2. **Ranging Accuracy Test**
   - Measure at 0.5m, 1m, 2m, 3m, 5m, 10m
   - Record 100 samples at each distance
   - Calculate mean, std deviation, max error
   - Plot results

3. **Environmental Tests**
   - Indoor LOS (Line of Sight)
   - Indoor NLOS (furniture obstacles)
   - Outdoor LOS
   - Different orientations

4. **Stress Tests**
   - Continuous ranging for 1 hour
   - Check for memory leaks
   - Monitor stability
   - Thermal effects

**Automation Scripts:**
```bash
test_scripts/
├── upload_initiator.sh      # Upload to first Arduino
├── upload_responder.sh      # Upload to second Arduino
├── monitor_both.sh          # Dual serial monitor
├── test_ranging.sh          # Automated ranging test
└── collect_data.sh          # Data logging for analysis
```

**Success Criteria:**
- Automated upload to correct devices
- Dual serial monitoring works
- Can collect ranging data to CSV
- Achieve target accuracy (or document limitations)

### Phase 6: Documentation and Delivery

**Goals:**
- Document final accuracy achieved
- Create usage guide
- Document limitations and issues
- Provide calibration instructions
- Create wiring diagrams (if migrated to ESP32)

**Deliverables:**
- Final accuracy report
- User manual
- Calibration procedure
- Known issues and workarounds
- Future improvement suggestions

## Decision Points and Off-Ramps

### Decision Point 1: After Phase 2

**Evaluation Criteria:**
- Can we establish basic SPI communication?
- Can we read DWM3000 chip ID?
- Can devices exchange simple messages?

**Possible Outcomes:**
- ✅ **Success**: Proceed to Phase 3
- ⚠️ **Partial Success**: Debug issues, continue if promising
- ❌ **Failure**: Library fundamentally incompatible
  - **Action**: Research alternative libraries or migrate to ESP32

### Decision Point 2: After Phase 3

**Evaluation Criteria:**
- Can we capture timestamps reliably?
- Does TWR protocol complete successfully?
- Are timestamps sensible values?

**Possible Outcomes:**
- ✅ **Success**: Proceed to Phase 4
- ⚠️ **Partial Success**: Some timestamps work, others fail
  - **Action**: Debug timestamp capture, optimize interrupt handling
- ❌ **Failure**: Cannot capture timestamps or complete protocol
  - **Action**: Evaluate if Arduino Uno limitations are insurmountable
  - **Consider**: Migrate to ESP32

### Decision Point 3: After Phase 4

**Evaluation Criteria:**
- Do distance calculations produce reasonable values?
- What accuracy can we achieve?
- Is it acceptable for the application?

**Possible Outcomes:**
- ✅ **Success (±5-10cm)**: Excellent! Proceed to Phase 5
- ⚠️ **Partial Success (±20-50cm)**: Acceptable for some uses
  - **Action**: Document limitations, offer ESP32 migration path
- ❌ **Failure**: Random values or no distance measurement
  - **Action**: Strong recommendation to migrate to ESP32

## ESP32 Migration Path (Backup Plan)

### When to Migrate

Migrate to ESP32 if:
1. Arduino Uno cannot establish TWR communication (Phase 3 fails)
2. Timestamp capture is unreliable or impossible
3. Distance calculations are unusable (>1m error)
4. Development is stalled for >2 weeks on Arduino Uno

### Migration Requirements

**Hardware:**
- 2x ESP32 DevKit boards (ESP32-WROOM-32 or similar)
- Cost: ~$5-10 per board = $10-20 total
- Breadboards and jumper wires

**Wiring: PCL298336 Shield to ESP32**

Since PCL298336 is an Arduino shield, it won't plug directly into ESP32. Manual wiring required:

```
DWM3000 Shield Pin → ESP32 Pin
─────────────────────────────────
MOSI (D11)        → GPIO 23 (VSPI MOSI)
MISO (D12)        → GPIO 19 (VSPI MISO)
SCK  (D13)        → GPIO 18 (VSPI CLK)
CS   (D10)        → GPIO 5  (VSPI CS)
IRQ  (D2)         → GPIO 4  (interrupt capable)
RST  (D9)         → GPIO 16
3.3V              → 3.3V
GND               → GND
```

**Important Notes:**
- ESP32 is 3.3V native (safe for DWM3000)
- Shield must NOT be powered from Arduino 5V rail
- Use shield's onboard 3.3V DC-DC converter
- Jumper configuration may be needed

**Detailed wiring diagrams will be created when/if needed.**

**Software:**
- Library: [Fhilb/DW3000_Arduino](https://github.com/Fhilb/DW3000_Arduino)
- Platform: ESP32 in PlatformIO
- Examples: Use proven ping/pong ranging examples

**Migration Time Estimate:**
- Hardware procurement: 1-3 days
- Wiring and setup: 1-2 hours
- Code adaptation: 2-4 hours
- Testing and calibration: 4-8 hours
- **Total: ~1 week from decision to working system**

**Expected Outcome:**
- 10cm accuracy achievable
- Proven, stable TWR operation
- Fast ranging updates (10-20 Hz)
- Active community support

## Automation and Testing Tools

### Bash Scripts to Create

#### 1. `test_scripts/detect_ports.sh`

```bash
#!/bin/bash
# Detect and list Arduino USB ports
# Output: /dev/ttyUSB0, /dev/ttyUSB1, etc.
```

**Purpose:**
- Automatically find connected Arduino boards
- Identify which port is which device
- Update platformio.ini with correct ports

#### 2. `test_scripts/upload_initiator.sh`

```bash
#!/bin/bash
# Upload initiator firmware to first Arduino
pio run -e initiator -t upload
```

#### 3. `test_scripts/upload_responder.sh`

```bash
#!/bin/bash
# Upload responder firmware to second Arduino
pio run -e responder -t upload
```

#### 4. `test_scripts/upload_both.sh`

```bash
#!/bin/bash
# Upload both firmwares in sequence
./upload_responder.sh && ./upload_initiator.sh
```

#### 5. `test_scripts/monitor_both.sh`

```bash
#!/bin/bash
# Open two terminal windows with serial monitors
# One for each Arduino
tmux new-session -d -s uwb_monitor
tmux split-window -h
tmux send-keys -t 0 'pio device monitor -e initiator' C-m
tmux send-keys -t 1 'pio device monitor -e responder' C-m
tmux attach -t uwb_monitor
```

**Purpose:**
- Monitor both devices simultaneously
- See communication in real-time
- Debug protocol issues

#### 6. `test_scripts/test_ranging.sh`

```bash
#!/bin/bash
# Automated ranging test
# 1. Upload firmware to both devices
# 2. Wait for initialization
# 3. Collect ranging data for N seconds
# 4. Save to CSV file with timestamp
```

**Purpose:**
- Repeatable testing
- Data collection for analysis
- Track accuracy improvements over development

#### 7. `test_scripts/calibrate.sh`

```bash
#!/bin/bash
# Guided calibration procedure
# Prompt user to place devices at known distances
# Collect measurements and calculate calibration factors
```

### Serial Monitor Output Format

**Initiator Output:**
```
[INIT] DWM3000 Initiator v1.0
[INIT] Chip ID: 0xDECA0302
[INIT] Configuration complete
[TWR] Sending POLL... T1=1234567890
[TWR] Received POLL_ACK T4=1234580000
[TWR] Sending RANGE with (T1, T4, T5)
[TWR] Received RANGE_REPORT with (T2, T3, T6, T7)
[CALC] ToF = 12345 ticks
[RESULT] Distance: 1.23 m (123 cm) ±10cm
[TWR] Next ranging in 1000ms...
```

**Responder Output:**
```
[INIT] DWM3000 Responder v1.0
[INIT] Chip ID: 0xDECA0302
[INIT] Listening for POLL messages...
[TWR] Received POLL T2=1234570000
[TWR] Sending POLL_ACK T3=1234572000
[TWR] Received RANGE with (T1, T4, T5)
[TWR] Sending RANGE_REPORT with (T2, T3, T6, T7)
[STATUS] Ranging complete, back to listening
```

**Data Logging Format (CSV):**
```
timestamp,distance_m,distance_cm,tof_ticks,t1,t2,t3,t4,t5,t6,t7,t8
2026-01-08T10:30:01.123,1.234,123.4,12345,100,200,300,400,500,600,700,800
```

## Accuracy Goals and Uncertainty

### Target Accuracy
- **Goal**: 5-10 cm (±0.05-0.10 m)
- **Ideal**: ±10 cm (3σ confidence)
- **Acceptable**: ±20 cm on Arduino Uno (due to limitations)
- **ESP32**: ±10 cm achievable

### Factors Affecting Accuracy

**1. Hardware Limitations**
- Arduino Uno: 16MHz CPU, slower interrupt handling
- ESP32: 240MHz CPU, better timing precision
- DWM3000: ~15.65ps time resolution (inherent precision)

**2. Environmental Factors**
- Multipath reflections (indoor environments)
- Temperature effects on clock drift
- Antenna orientation and polarization
- RF interference

**3. Calibration**
- Antenna delay (systematic offset)
- Clock drift between devices
- Distance-dependent errors

### Measurement Uncertainty Analysis

**Components of Uncertainty:**

1. **Timestamp Quantization**: ±15.65 ps → ±0.0047 m (negligible)
2. **Clock Drift**: Canceled by DS-TWR (double-sided)
3. **Interrupt Latency**:
   - Arduino Uno: ~4-10 µs → ±1-3 m (SIGNIFICANT!)
   - ESP32: ~1-2 µs → ±0.3-0.6 m
4. **Antenna Delay**: ±50-500 ps → ±0.015-0.15 m (calibratable)
5. **Multipath**: ±10-50 cm (environment dependent)

**Total Expected Uncertainty:**
- Arduino Uno: ±20-50 cm (interrupt latency dominates)
- ESP32: ±5-10 cm (achievable with calibration)

**Accuracy Improvement Strategies:**
1. Minimize interrupt latency (critical for Uno)
2. Calibrate antenna delay at known distance
3. Average multiple measurements
4. Filter outliers (>3σ from mean)
5. Temperature compensation (advanced)

## Library Options and Integration

### Option 1: emineminof/DWM3000-ATMega328p (Primary)

**Repository**: https://github.com/emineminof/DWM3000-ATMega328p

**Status:**
- Early development (5 commits)
- Basic TX/RX working
- TWR NOT working (confirmed)

**Integration:**
1. Clone repository
2. Extract library code
3. Add to PlatformIO `lib/` folder
4. Debug TWR implementation
5. Fix timestamp capture issues

**Pros:**
- Specifically for ATmega328P
- Starting point exists
- Community knowledge

**Cons:**
- Incomplete/broken TWR
- Requires significant debugging
- No guarantee of success

### Option 2: Port from Official Qorvo SDK

**Source**: Qorvo DW3xxx SDK v1.1.1

**Approach:**
1. Download official SDK
2. Extract DWM3000 driver code
3. Port to Arduino framework
4. Adapt for ATmega328P constraints
5. Implement TWR examples

**Pros:**
- Official, correct implementation
- Complete feature set
- Well documented

**Cons:**
- Major porting effort (weeks)
- May exceed Arduino Uno capabilities
- Complex codebase

### Option 3: Adapt from ESP32 Library

**Source**: Fhilb/DW3000_Arduino

**Approach:**
1. Study working ESP32 implementation
2. Identify timing-critical sections
3. Adapt for slower Arduino Uno
4. Optimize for 16MHz operation

**Pros:**
- Proven TWR implementation
- Arduino-friendly API
- Good reference

**Cons:**
- Designed for 240MHz CPU
- May need significant modifications
- Timing assumptions may not translate

**Recommended**: Start with Option 1, supplement with knowledge from Option 3.

## Success Metrics

### Phase 2 Success
- [ ] SPI communication established
- [ ] Chip ID read successfully: `0xDECA0302` (expected for DW3110)
- [ ] Devices can exchange messages
- [ ] Interrupts working
- [ ] Serial debug output functional

### Phase 3 Success
- [ ] POLL message sent and received
- [ ] POLL_ACK returned successfully
- [ ] RANGE message exchanged
- [ ] RANGE_REPORT received
- [ ] All 8 timestamps captured
- [ ] No protocol timeouts or failures

### Phase 4 Success
- [ ] ToF calculated from timestamps
- [ ] Distance values in reasonable range (0-100m)
- [ ] Repeated measurements show consistency (σ < 50cm)
- [ ] Known distance test shows accuracy <50cm
- [ ] After calibration: accuracy <20cm (Uno) or <10cm (ESP32)

### Phase 5 Success
- [ ] Automated testing scripts work
- [ ] Can collect data to CSV
- [ ] Tested at 5+ different distances
- [ ] Documented accuracy: X cm ± Y cm
- [ ] Stable operation for >1 hour
- [ ] No crashes or memory issues

## Risk Assessment

### High Risk Items

1. **Arduino Uno TWR Compatibility**
   - **Risk**: TWR may not work reliably on 16MHz CPU
   - **Mitigation**: Extensive debugging, optimize interrupt handlers
   - **Fallback**: Migrate to ESP32

2. **Library Availability**
   - **Risk**: No mature Arduino Uno library exists
   - **Mitigation**: Port from existing sources, contribute to community
   - **Fallback**: Use official SDK on supported platform

3. **Timestamp Accuracy**
   - **Risk**: Interrupt latency corrupts timestamps
   - **Mitigation**: Minimize ISR code, use hardware timestamps
   - **Fallback**: ESP32 has better timing

### Medium Risk Items

4. **PlatformIO Configuration**
   - **Risk**: Complex multi-environment setup
   - **Mitigation**: Good documentation, test thoroughly
   - **Fallback**: Separate projects if needed

5. **Serial Port Auto-detection**
   - **Risk**: Ports may swap between boots
   - **Mitigation**: Robust detection script, manual override option
   - **Fallback**: Manual port specification

6. **Calibration Complexity**
   - **Risk**: Difficult to achieve target accuracy
   - **Mitigation**: Systematic calibration procedure
   - **Fallback**: Document achieved accuracy, adjust expectations

## Timeline Estimate

### Optimistic (Everything Works)
- Phase 1: 1 day (mostly complete)
- Phase 2: 2-3 days
- Phase 3: 3-5 days
- Phase 4: 2-3 days
- Phase 5: 2-3 days
- **Total: 10-15 days**

### Realistic (Some Issues)
- Phase 1: 1 day ✓
- Phase 2: 3-5 days (library integration challenges)
- Phase 3: 5-10 days (TWR debugging)
- Phase 4: 3-5 days (calibration iteration)
- Phase 5: 3-5 days (testing and optimization)
- **Total: 15-26 days (2-4 weeks)**

### Pessimistic (Arduino Uno Inadequate → ESP32 Migration)
- Phase 1-3 on Uno: 10-15 days (unsuccessful)
- Decision to migrate: 1 day
- ESP32 procurement: 2-3 days
- ESP32 implementation: 5-7 days
- **Total: 18-26 days (3-4 weeks)**

**Recommendation**: Plan for realistic timeline, be prepared for ESP32 migration.

## Resources and References

### Documentation Created
- [Code Review Findings](findings/code-review.md)
- [Hardware Research](findings/hardware-research.md)
- [Web Research](findings/web-research.md)

### External Resources
- [Qorvo DWM3000 Product Page](https://www.qorvo.com/products/p/DWM3000)
- [DWM3000EVB Arduino Shield](https://www.qorvo.com/products/p/DWM3000EVB)
- [DWM3000 Datasheet](https://download.mikroe.com/documents/datasheets/DWM3000_datasheet.pdf)
- [ATmega328P DWM3000 Port](https://github.com/emineminof/DWM3000-ATMega328p)
- [ESP32 DW3000 Library](https://github.com/Fhilb/DW3000_Arduino)
- [CircuitDigest Tutorial](https://circuitdigest.com/microcontrollers-projects/diy-indoor-uwb-positioning-system-using-esp32-and-qorvo-dwm3000)

### Community Support
- [Qorvo Tech Forum - UWB](https://forum.qorvo.com/c/ultra-wideband/13)
- [Arduino Forum - DWM3000 Group](https://forum.arduino.cc/t/dwm-3000-collaborative-group/897672)

## Appendix: Technical Details

### DWM3000 Register Map (Key Registers)

```
DEV_ID      0x00  Device ID (should read 0xDECA0302)
SYS_CFG     0x04  System configuration
TX_FCTRL    0x08  TX frame control
RX_FCTRL    0x0C  RX frame control
SYS_TIME    0x06  System time counter
TX_TIME     0x0A  Transmit timestamp
RX_TIME     0x13  Receive timestamp
SYS_STATUS  0x0F  System status register
```

### Message Frame Structure

```cpp
struct uwb_frame {
    uint16_t frame_control;    // 0x41C8 for data frame
    uint8_t  sequence;         // Incremental sequence number
    uint16_t pan_id;           // PAN ID (e.g., 0xDECA)
    uint16_t dest_addr;        // Destination address
    uint16_t src_addr;         // Source address
    uint8_t  msg_type;         // POLL, POLL_ACK, RANGE, RANGE_REPORT
    uint8_t  payload[N];       // Timestamp data, etc.
    uint16_t fcs;              // Frame check sequence (auto)
};
```

### Timestamp Format

DWM3000 timestamps are 40-bit values:
```cpp
uint64_t timestamp;  // Lower 40 bits are valid
// Units: 15.65 picoseconds per tick
// Resolution: ~15.65 ps
// Range: ~17.2 seconds before rollover
```

### Distance Calculation Code Template

```cpp
float calculate_distance(uint64_t t1, uint64_t t2, uint64_t t3, uint64_t t4,
                         uint64_t t5, uint64_t t6, uint64_t t7, uint64_t t8) {
    // Double-Sided Two-Way Ranging formula
    int64_t round1 = t4 - t1;
    int64_t reply1 = t3 - t2;
    int64_t round2 = t8 - t5;
    int64_t reply2 = t7 - t6;

    // Time of Flight (handles clock drift)
    int64_t tof_ticks = ((round1 * round2) - (reply1 * reply2)) /
                        (round1 + round2 + reply1 + reply2);

    // Convert to distance
    const double DWT_TIME_UNITS = 1.0 / (499.2e6 * 128.0);  // ~15.65 ps
    const double SPEED_OF_LIGHT = 299702547.0;               // m/s

    double tof_seconds = tof_ticks * DWT_TIME_UNITS;
    double distance_meters = tof_seconds * SPEED_OF_LIGHT;
    float distance_cm = distance_meters * 100.0;

    return distance_cm;
}
```

---

**Document Version**: 1.0
**Last Updated**: 2026-01-08
**Status**: Ready for Phase 1 completion and Phase 2 start
