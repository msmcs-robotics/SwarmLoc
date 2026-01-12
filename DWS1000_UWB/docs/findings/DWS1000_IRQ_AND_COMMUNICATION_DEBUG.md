# DWS1000 IRQ and Communication Debug Findings

**Date:** 2026-01-12
**Hardware:** Arduino Uno + DWS1000 Shield (PCL298336)
**Library:** thotro/arduino-dw1000 v0.9

---

## Summary

After extensive testing, we confirmed:
1. **SPI communication works** - Device ID "DECA" reads correctly on both devices
2. **IRQ signal is NOT reaching Arduino pin D2** - Even with jumper wire D8→D2
3. **RF communication IS happening** - Receiver detects frames from transmitter
4. **Received data is corrupted/garbled** - "PING" transmits but receives as garbage

---

## Test 1: Diagnostic Test with IRQ Monitoring

### Setup
- Uploaded diagnostic firmware to ACM0
- Monitors IRQ pin state and interrupt count
- Reads SYS_STATUS register to see DW1000 activity

### Results
```
Device ID: DECA - model: 1, version: 3, revision: 0
[PASS] SPI working - DW1000 detected
EUI: FF:FF:FF:FF:00:00:00:00
IRQ pin (D2) initial state: LOW

[2s] IRQ count: 0 | IRQ pin: LOW | No new interrupts
SYS_STATUS: 0xC05B3900E6
- CPLOCK: Clock PLL locked
- RXDFR: Frame received
- RXFCG: Good CRC
```

### Analysis
- **IRQ count stays at 0** despite SYS_STATUS showing activity (RXDFR, RXFCG set)
- The DW1000 is receiving frames but the IRQ signal never reaches Arduino
- Jumper wire D8→D2 doesn't help because the IRQ signal isn't routed to D8 header pin

### Conclusion
The DWS1000 shield's IRQ routing is problematic. The IRQ pin from the DWM1000 module may:
- Not be connected to D8 at all
- Be connected but require additional configuration
- Have a hardware issue on the shield

---

## Test 2: Polling Mode (No Interrupts)

### Setup
- Created polling-based firmware that reads SYS_STATUS directly
- Transmitter sends "PING", waits for "PONG"
- Receiver listens, responds with "PONG"

### Results
```
TRANSMITTER (ACM1):
[TX #1] Sending PING... SENT!
[RX] Waiting for PONG... GOT: 't<z6	A?
[TX #2] Sending PING... SENT!
[RX] Waiting for PONG... GOT: 't<z6
...
TX: 25 | RX: 0 | Errors: 25

RECEIVER (ACM0):
[RX #1] Received: Uá'}8zA=
[RX #2] Received: Uá'}8z
...
TX: 0 | RX: 8 | Errors: 7
```

### Analysis
- **Transmitter IS sending** - TX frame sent status confirms transmission
- **Receiver IS receiving** - SYS_STATUS shows RXFCG (good CRC)
- **Bidirectional RF works** - Transmitter receives something back after sending
- **Data is corrupted** - "PING" (0x50 0x49 0x4E 0x47) becomes garbage

### Possible Causes
1. **Timing issue** - Data read before frame fully received
2. **Buffer offset** - MAC header present, data at wrong offset
3. **Frame filtering mismatch** - Different frame types expected
4. **SPI speed too fast** - Data corruption during read
5. **Power supply** - Voltage drop causing bit errors

---

## Test 3: Raw Data Inspection

### Received Hex Bytes
Sample received data (expected "PING" = 50 49 4E 47):
```
55 E1 27 7D 38 7A 41 3D  (garbage)
50 E1 27 7D 38 7A 41 38  (starts with 0x50='P', rest garbage)
74 3C 7A 36 09 41 3F     (completely different)
```

### Analysis
- First byte sometimes matches (0x50 = 'P')
- Rest of data is inconsistent
- Suggests timing or buffer issue rather than complete failure

---

## Hardware Configuration

### Confirmed Pin Mapping (from testing + forums)

| Signal | Library Default | DWS1000 Shield | Status |
|--------|-----------------|----------------|--------|
| RST    | D9              | **D7**         | Working (reset works) |
| IRQ    | D2              | **D8**         | NOT WORKING |
| SS/CS  | D10             | D10            | Working (SPI works) |
| MOSI   | D11             | D11            | Working |
| MISO   | D12             | D12            | Working |
| SCK    | D13             | D13            | Working |

### Jumper Wire Attempt
- Added jumper from D8 to D2 on Arduino headers
- **Result: No effect** - IRQ count remains 0
- IRQ signal from DWM1000 module likely not routed to D8 header

---

## Recommendations

### Immediate Next Steps

1. **Check shield schematic** - Find actual IRQ routing on DWS1000 PCB
2. **Probe with oscilloscope** - Check if IRQ signal exists anywhere on shield
3. **Try direct wire** - Connect DWM1000 pin 22 (IRQ) directly to Arduino D2
4. **Try alternative library** - arduino-dw1000-ng or esp32-dw1000-lite

### For Data Corruption Issue

1. **Add delay after startReceive()** - Give time for frame to arrive
2. **Clear RX buffer before read** - Ensure no stale data
3. **Check frame length** - Verify getDataLength() returns correct value
4. **Try slower SPI** - Reduce SPI speed for debugging

### Hardware Modifications

1. **Solder jumper** - If D8 has IRQ, bridge to D2 on PCB
2. **Bodge wire** - Connect DWM1000 IRQ pad directly to D2
3. **Add pull-down** - 10K resistor from IRQ to GND

---

## Alternative Libraries Research

| Library | Status | Notes |
|---------|--------|-------|
| arduino-dw1000-ng | Archived Nov 2023 | Better ranging, ESP32 issues |
| esp32-dw1000-lite | Active | Polling mode, no IRQ needed |
| jremington/UWB-Indoor | Active (221 stars) | Most maintained fork |
| Decawave official | Not Arduino | Polling mode support |

See [DW1000_LIBRARY_ALTERNATIVES.md](DW1000_LIBRARY_ALTERNATIVES.md) for details.

---

## Files Created During Debug

- `src/main.cpp` - Current polling mode test
- `src/main_ranging.cpp.bak` - Original ranging firmware
- `src/diagnostic_test.cpp.bak` - Full diagnostic test
- `tests/test_pingpong_sender.cpp` - Ping-pong sender
- `tests/test_pingpong_receiver.cpp` - Ping-pong receiver

---

## Session Log

```
2026-01-12 08:00 - Started debug session
2026-01-12 08:15 - Confirmed IRQ not firing (count=0)
2026-01-12 08:30 - Created polling mode test
2026-01-12 08:45 - Confirmed RF working but data corrupted
2026-01-12 09:00 - Documented findings
```

---

## IMPORTANT DISCOVERY: Library Defaults May Be Correct

### New Finding (Late Session)

After reviewing the user's **archive code** (`archive/initiator/initiator.ino`) and the DW1000-ng library examples, we discovered:

**ALL working code uses PIN_RST=9, PIN_IRQ=2 (library defaults) - NOT D7/D8!**

This contradicts the Qorvo forum post that suggested D7/D8.

### Evidence

1. **archive/initiator.ino** (user's previous working code):
   ```cpp
   const uint8_t PIN_RST = 9;
   const uint8_t PIN_IRQ = 2;
   const uint8_t PIN_SS = SS;
   ```

2. **archive/responder.ino** (user's previous working code):
   ```cpp
   const uint8_t PIN_RST = 9;
   const uint8_t PIN_IRQ = 2;
   const uint8_t PIN_SS = SS;
   ```

3. **DW1000-ng library examples** (all use same defaults):
   ```cpp
   const uint8_t PIN_RST = 9;
   const uint8_t PIN_IRQ = 2;
   const uint8_t PIN_SS = SS;
   ```

### Updated Hypothesis

The DWS1000 shield may actually route IRQ to D2 and RST to D9 (library defaults), not D8/D7 as the forum suggested. The forum post may have been:
- For a different hardware revision
- Incorrect
- For a different shield variant

### Next Test: Remove Jumper Wire

**ACTION REQUIRED:**
1. Remove the jumper wire D8→D2 from both Arduinos
2. Upload the new firmware (using PIN_RST=9, PIN_IRQ=2)
3. Test if interrupts now work

The new firmware in `src/main.cpp` has been updated to:
- Use library default pins (RST=9, IRQ=2, SS=10)
- Use interrupt-based operation (not polling)
- Read data inside ISR to prevent corruption
- Display clear diagnostics about whether IRQ is working

---

## Sources

- [Qorvo Forum: Arduino Uno with DWS1000](https://forum.qorvo.com/t/arduino-uno-with-dws1000/10212)
- [arduino-dw1000 GitHub Issues](https://github.com/thotro/arduino-dw1000/issues)
- [DW1000 User Manual v2.17](https://www.sunnywale.com/uploadfile/2021/1230/DW1000%20User%20Manual_Awin.pdf)
