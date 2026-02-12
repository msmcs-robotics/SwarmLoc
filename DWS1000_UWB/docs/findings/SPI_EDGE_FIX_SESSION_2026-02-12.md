# SPI_EDGE Fix & RX Debugging Session — 2026-02-12

## Summary

Three major discoveries were made for the DWS1000 UWB shields on Arduino Uno:

### 1. SPI_EDGE_BIT Root Cause (SOLVED)

- The DW1000 library's `select()` function (line 140 of DW1000.cpp) sets SPI_EDGE_BIT (bit 10 of SYS_CFG register)
- This changes the DW1000's MISO timing, which is incompatible with the Arduino Uno's AVR SPI hardware
- Result: ALL register reads return 0xFF after library initialization
- Fix: Wrapped with `#if !defined(__AVR__)` so SPI_EDGE is only set on ESP8266/ESP32
- Before fix: 16% SPI success rate (only direct pre-init reads worked)
- After fix: 90-100% in IDLE mode, 75-90% during RX

### 2. IRQ Handler Priority Bug

- The library's `handleInterrupt()` checked `isReceiveFailed()` BEFORE `isReceiveDone()`
- With ~10-20% SPI corruption during RX mode, glitched error bits caused valid frames to be discarded
- Fix: Reordered to check `isReceiveDone()` first
- Also moved `isClockProblem()` to after receive handling (PLL sticky bits caused false errors)

### 3. SPI Reliability During RX Mode

- IDLE mode: 100% SPI reliable (both 2MHz and 16MHz)
- TX mode: ~100% SPI reliable
- RX mode: 75-90% SPI reliable (hardware EMI from active radio)
- Root cause: likely electromagnetic interference from DW1000 radio front-end affecting SPI signal integrity on the shared Arduino Uno bus
- Mitigation: double-read with retry, watchdog timer to restart stuck receiver

## Test Results

- TX: 100% reliable (30/30 packets sent successfully)
- RX: 33% frame detection (10/30 received), with watchdog restarts
- Frame data: len=74, data partially garbled by SPI corruption during read
- Device ACM0 (LDO 0x88) performs better as receiver than ACM1 (LDO 0x28)

## Library Changes Made (DW1000.cpp)

1. Line 140: SPI_EDGE_BIT conditional on non-AVR
2. Line 106: Fast SPI speed reduced from 16MHz to 2MHz for AVR reliability
3. handleInterrupt(): Reordered to prioritize isReceiveDone() over isReceiveFailed()
4. handleInterrupt(): Moved isClockProblem() to end

## Scripts Created

- `scripts/upload_and_capture.sh` — compile + upload + serial capture
- `scripts/capture_only.sh` — reset + serial capture
- `scripts/capture_dual.sh` — two-port simultaneous capture
- `scripts/upload_dual_and_capture.sh` — upload to 2 devices + capture both
- `scripts/COMMANDS_REFERENCE.md` — non-blocking serial monitoring reference

## Next Steps

1. Investigate hardware-level SPI signal integrity (bypass caps, shorter wires)
2. Try SPI_MODE1 or SPI_MODE3 which may have better timing margins
3. Consider reading frame data immediately without going to idle (since idle clears buffer)
4. Add CRC verification of received data at application level
5. Test with ESP32 which should have none of these SPI issues
