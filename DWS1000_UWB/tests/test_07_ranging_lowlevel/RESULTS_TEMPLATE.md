# Test 07 Results: Low-Level Ranging Examples

## Test Information
- **Date**: [YYYY-MM-DD]
- **Time**: [HH:MM]
- **Tester**: [Name]
- **Test Duration**: [X] minutes
- **Hardware**: 2x Arduino Uno with DW1000 modules

## Configuration Summary

### TAG Configuration
- Device Address: 2
- Network ID: 10
- Serial Port: [e.g., /dev/ttyACM0]
- Firmware: src_tag/main.cpp (RangingTag.ino)

### ANCHOR Configuration
- Device Address: 1
- Network ID: 10
- Serial Port: [e.g., /dev/ttyACM1]
- Firmware: src_anchor/main.cpp (RangingAnchor.ino)

## Compilation Results

### TAG Compilation
- [ ] Success
- [ ] Failed

**Compiler Output:**
```
[Paste compilation output here]
```

**Issues:** [Describe any compilation errors or warnings]

### ANCHOR Compilation
- [ ] Success
- [ ] Failed

**Compiler Output:**
```
[Paste compilation output here]
```

**Issues:** [Describe any compilation errors or warnings]

## Upload Results

### TAG Upload
- [ ] Success
- [ ] Failed

**Issues:** [Describe any upload errors]

### ANCHOR Upload
- [ ] Success
- [ ] Failed

**Issues:** [Describe any upload errors]

## Initialization Results

### TAG Initialization
- [ ] DW1000 initialized successfully
- [ ] Device ID displayed: DECA0130
- [ ] Unique ID displayed: [record value]
- [ ] Network ID & Address: 10:2
- [ ] Device mode displayed

**TAG Serial Output (first 50 lines):**
```
[Paste TAG initialization output here]
```

### ANCHOR Initialization
- [ ] DW1000 initialized successfully
- [ ] Device ID displayed: DECA0130
- [ ] Unique ID displayed: [record value]
- [ ] Network ID & Address: 10:1
- [ ] Device mode displayed

**ANCHOR Serial Output (first 50 lines):**
```
[Paste ANCHOR initialization output here]
```

## Ranging Results

### Overall Status
- [ ] Ranging works - continuous measurements displayed
- [ ] Ranging partially works - intermittent measurements
- [ ] Ranging doesn't work - no measurements
- [ ] Ranging fails - RANGE_FAILED messages

### Ranging Performance

**Monitoring Duration:** [X] minutes

**Sample ANCHOR Output:**
```
[Paste 20-30 lines of ranging output here, showing:]
Range: X.XX m    RX power: -XX.XX dBm    Sampling: X.XX Hz
Range: X.XX m    RX power: -XX.XX dBm    Sampling: X.XX Hz
...
```

### Quantitative Measurements

**Distance Measurements:**
- Actual physical distance: ________ m
- Minimum measured: ________ m
- Maximum measured: ________ m
- Average measured: ________ m
- Standard deviation: ________ m
- Accuracy (avg vs actual): ________ m (________ % error)

**Signal Quality:**
- Average RX power: ________ dBm
- RX power range: ________ to ________ dBm

**Performance:**
- Average sampling rate: ________ Hz
- Min sampling rate: ________ Hz
- Max sampling rate: ________ Hz
- Total successful ranges: ________
- Total failed ranges: ________
- Success rate: ________ %

### Stability Analysis

**Range Stability:**
- [ ] Very stable (±1-2 cm variation)
- [ ] Stable (±5-10 cm variation)
- [ ] Moderate (±10-20 cm variation)
- [ ] Unstable (±20-50 cm variation)
- [ ] Very unstable (>50 cm variation)

**Notes on stability:**
[Describe any patterns, trends, or anomalies in the measurements]

## Issues Observed

### Protocol Issues
- [ ] No protocol issues
- [ ] Occasional RANGE_FAILED messages
- [ ] Frequent RANGE_FAILED messages
- [ ] Protocol timeouts (no output for >1 second)
- [ ] Devices not communicating at all

**Details:**
[Describe any protocol-level issues]

### Hardware Issues
- [ ] No hardware issues
- [ ] Intermittent connectivity
- [ ] Device resets/reboots
- [ ] Antenna issues
- [ ] Power supply issues

**Details:**
[Describe any hardware-related problems]

### Environmental Issues
- [ ] No environmental issues
- [ ] Interference detected (varying signal strength)
- [ ] Obstruction effects observed
- [ ] Temperature/humidity effects

**Details:**
[Describe environmental factors affecting the test]

## Comparison with Test 06 (DW1000Ranging Library)

### Test 06 Status (for reference)
- [ ] Test 06 worked successfully
- [ ] Test 06 had issues
- [ ] Test 06 not yet conducted

### Comparison (if both tests completed)

| Metric | Test 06 (DW1000Ranging) | Test 07 (Low-Level) | Notes |
|--------|-------------------------|---------------------|-------|
| Initialization | [Success/Fail] | [Success/Fail] | |
| Ranging Works | [Yes/No] | [Yes/No] | |
| Sampling Rate | [X.XX Hz] | [X.XX Hz] | |
| Range Accuracy | [±X cm] | [±X cm] | |
| Signal Strength | [-XX dBm] | [-XX dBm] | |
| Stability | [Good/Poor] | [Good/Poor] | |

**Analysis:**
[Compare the two approaches. Which works better? Why might there be differences?]

## Additional Tests Conducted

### Distance Variation Test
- [ ] Conducted
- [ ] Not conducted

**Results:**
| Distance | Measured | Error | RX Power | Notes |
|----------|----------|-------|----------|-------|
| 0.5 m | | | | |
| 1.0 m | | | | |
| 2.0 m | | | | |
| 5.0 m | | | | |
| 10.0 m | | | | |

### Obstruction Test
- [ ] Conducted
- [ ] Not conducted

**Results:**
[Describe effects of different obstructions]

### Interference Test
- [ ] Conducted
- [ ] Not conducted

**Results:**
[Describe effects of interference sources]

## Conclusions

### Test Success
- [ ] Complete success - ranging works as expected
- [ ] Partial success - ranging works but with issues
- [ ] Failure - ranging doesn't work

### Key Findings
1. [First major finding]
2. [Second major finding]
3. [Third major finding]
...

### Root Cause Analysis (if issues found)
**Problem:** [Describe main issue]
**Likely Cause:** [Your analysis of why it's happening]
**Evidence:** [What data supports this conclusion]
**Proposed Solution:** [How to fix it]

### Recommendations

#### Immediate Next Steps
1. [First action to take]
2. [Second action to take]
3. [Third action to take]

#### Configuration Changes
- [ ] No changes needed
- [ ] Change reply delay to: ________ μs
- [ ] Change mode to: ________
- [ ] Adjust network configuration
- [ ] Other: ________

#### Hardware Changes
- [ ] No changes needed
- [ ] Check/replace antenna
- [ ] Verify wiring connections
- [ ] Use external power supply
- [ ] Other: ________

### Comparison with DW1000Ranging Library
[If test_06 was completed, discuss which approach works better and why]

## Appendix

### Full Serial Logs

**TAG Full Log:** [Attach or link to complete log file]
```
[Or paste full TAG output here if reasonable length]
```

**ANCHOR Full Log:** [Attach or link to complete log file]
```
[Or paste full ANCHOR output here if reasonable length]
```

### Photos/Screenshots
[Include any relevant photos of:
- Test setup
- Serial monitor output
- Hardware configuration
- Device placement]

### Configuration Files Used
- platformio.ini: [Link or paste]
- src_tag/main.cpp: [Link to file]
- src_anchor/main.cpp: [Link to file]

### Environment Details
- Room: [Description of test environment]
- Temperature: ________ °C
- Humidity: ________ %
- Other devices nearby: [List any potential interference sources]
- Device separation: ________ m
- Line of sight: [ ] Yes [ ] No
- Obstructions: [Describe any obstructions]

---

**Test completed by:** [Name]
**Date:** [YYYY-MM-DD]
**Signature:** _______________
