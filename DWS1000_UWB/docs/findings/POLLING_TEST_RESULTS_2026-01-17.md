# Polling Mode Test Results - 2026-01-17

## Summary

Polling mode tests bypassing library interrupts reveal:
1. **TX works** - TXFRS status bit sets correctly (polled)
2. **RX detects frames** - RXFCG status bit sets, indicating good CRC
3. **BUT data is corrupted** - Frame length reports 1021 bytes instead of 8, data is garbage

## Test Configuration

- TX (ACM0): Sends "PING1234" (8 bytes) every 2 seconds
- RX (ACM1): Listens and responds with "PONG5678"
- Mode: 110 kb/s, PRF 16 MHz, Preamble 2048, Channel 5
- Pin config: RST=9, IRQ=2, SS=10 (library defaults)

## Observations

### RX Side Output
```
[RX #1] len=1021 hex=FF FF FF FF FF FF FF FF FF FF FF FF FF FF 98 00
[RX #4] len=1021 hex=55 C3 A1 EE E1 27 7D 38 7A D8 ED B6 15 08 41 3D
[RX #7] len=1021 hex=55 C3 A1 EE E1 27 7D 38 7A D8 ED B6 15 08 41 3D
```

### Key Issues

1. **Frame length 1021**: Expected 8-10 bytes, got 1021 (close to max 1023). The RX_FINFO register is being read but returns corrupt value.

2. **Repeating garbage pattern**: The pattern "55 C3 A1 EE E1 27 7D 38 7A..." keeps appearing. This is NOT the "PING1234" we're sending.

3. **RX detects frames during TX reset**: Frames detected before TX was even initialized - suggests noise or spurious triggers.

4. **TX intermittent failures**: Some TX poll for TXFRS succeeds, some timeout.

## Potential Causes

### Hardware Issues (Most Likely)
- [ ] **Antenna not connected** - U.FL connector may be loose
- [ ] **Shield not seated properly** - SPI pins may have poor contact
- [ ] **Damaged antenna/module** - Previous handling

### RF Configuration
- [ ] Channel mismatch (unlikely - both show same config)
- [ ] SFD timeout too short
- [ ] Preamble detection threshold

### Library Issues
- [ ] Data being overwritten before read
- [ ] Timing issues in RX buffer read

## Recommended Actions

1. **Physical inspection**:
   - Check U.FL antenna connectors on both shields
   - Reseat both shields on Arduino headers
   - Try swapping shields between Arduinos

2. **Antenna test**:
   - Verify antennas are attached and tight
   - Try bringing modules very close together (< 10cm)

3. **SPI signal quality**:
   - Check for loose jumper wires
   - Remove D8→D2 jumper if still installed

## Files Created

- `tests/polling_tx.cpp` - Transmitter polling test
- `tests/polling_rx.cpp` - Receiver polling test (with direct register reads)

## Next Steps

**User action required**: Please physically inspect the DWS1000 shields:
1. Are the U.FL antenna connectors tight?
2. Are the shields fully seated on the Arduino headers?
3. Is the D8→D2 jumper wire still installed? (should be removed for default pin config)
