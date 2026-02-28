# IRQ Pin Investigation - 2026-02-27

## Objective

Test whether polling the DW1000 IRQ pin (digital read, no SPI) can replace SPI-based status polling to improve RX reliability beyond 33%.

## Background

SPI reads during RX mode are 75-90% reliable due to EMI from the DW1000 radio front-end. The hypothesis: poll the IRQ pin (a simple digital signal) instead of SPI registers during RX mode, then only do SPI reads after the event (in IDLE mode, where SPI is 100% reliable).

## Key Findings

### Finding 1: USB Port Upload Fix (Software-based)

**Problem:** ACM0 on USB Bus 3 Port 5 fails all uploads (stk500_getsync errors).

**Previous fix (2026-01-11):** Move Arduino to a different physical USB port/bus.

**New software fix:** USB device reset via kernel sysfs:
```bash
USB_DEV="3-5"  # from udevadm ID_PATH
echo 0 | sudo tee /sys/bus/usb/devices/$USB_DEV/authorized
sleep 2
echo 1 | sudo tee /sys/bus/usb/devices/$USB_DEV/authorized
sleep 3
```
This re-enumerates the device and fixes the bootloader timing. No physical cable move needed.

**Root cause context (from user):** Both laptop USB ports on the right side share Bus 3. Moving one Arduino to the left-side USB port (different bus) avoids contention. The sysfs reset achieves a similar re-initialization without physical action.

### Finding 2: IRQ Pin Never Asserts for RX Events

**Test:** `test_rx_irq_pin.cpp` — polls `digitalRead(PIN_IRQ)` during RX mode.

**Result:** Zero frames detected via IRQ pin in 60 seconds (30 TX packets sent).

**Diagnostic:** Created `test_rx_diagnostic.cpp` which polls BOTH the IRQ pin AND reads SYS_STATUS via SPI every 500ms.

**Diagnostic results (60s capture):**
| Method | Events | Good Frames | Errors |
|--------|--------|-------------|--------|
| IRQ pin | 1 (startup transient only) | 0 | 0 |
| SPI polling | 13 | 8 | 5 |

Key observations:
- Pin D2 and D8 **always LOW** despite SYS_STATUS showing receive events
- SYS_CFG reads `0x20441200` — HIRQ_POL = ACTIVE_HIGH (correct)
- SYS_MASK reads `0x0007F000` (correct, though intermittently reads 0x0 due to SPI corruption during RX)
- Many SPI-detected "events" are likely false positives from SPI corruption (status reads like 0xFFFFFFFF, 0x950360FE, etc.)

### Finding 3: TX IRQ Callbacks DO Work

The TX test uses `DW1000.attachSentHandler(handleSent)` which relies on the library's ISR attached to the IRQ pin. TX reports 30/30 (100%) success, proving:
- The D8→D2 jumper wire IS connected and functional
- The DW1000's IRQ output CAN assert (at least for TX events)
- The library's interrupt mechanism works for TX

### Finding 4: RX IRQ Might Require Library's Built-in Mechanism

The key difference between working TX IRQ and failing RX IRQ:
- **TX:** Library's `attachSentHandler()` → `interruptOnSent(true)` → proper `_sysmask` management → `writeSystemEventMaskRegister()`
- **RX (our test):** Manual `DW1000.writeBytes(SYS_MASK_REG, ...)` bypassing library's `_sysmask` variable

The library manages `_sysmask` as an internal variable. When we write SYS_MASK directly to hardware, the library doesn't know. If any library function later calls `writeSystemEventMaskRegister()`, it overwrites our mask with its internal `_sysmask` (which is all zeros for RX events).

Checked: `newReceive()` calls `clearReceiveStatus()` which writes SYS_STATUS (not SYS_MASK). `startReceive()` doesn't touch SYS_MASK either. So the mask SHOULD persist, but the diagnostic shows intermittent 0x0 reads.

### Finding 5: Previous Documentation Confirms IRQ Issue (2026-01-12)

From `DWS1000_IRQ_AND_COMMUNICATION_DEBUG.md`:
> "IRQ count stays at 0 despite SYS_STATUS showing activity (RXDFR, RXFCG set)"
> "The IRQ signal isn't routed to D8 header pin"

However, this contradicts Finding 3 (TX IRQ works via D8→D2). The contradiction suggests:
- The 2026-01-12 conclusion may have been premature (SPI_EDGE_BIT wasn't fixed yet, so all reads were 0xFF at that time)
- The IRQ pin may work for some event types but not others
- There may be a timing or configuration issue specific to RX events

## Next Steps

1. ~~Try library's built-in RX callbacks~~ — DONE, zero results
2. ~~Optimize SPI polling~~ — DONE, revealed real PHY header errors

### Finding 6: Library RX Callbacks Also Don't Work (v5 test)

Used `attachReceivedHandler()` / `attachReceiveFailedHandler()` — exact same mechanism that makes TX callbacks work (30/30). Result: zero frames in 60s, IRQ pin always LOW. SYS_MASK correctly set to 0x5F088 by library.

**Conclusion: IRQ-based RX is definitively not viable on this DWS1000 shield hardware.**

### Finding 7: SPI Polling Shows Real PHY Header Errors (v7b test)

With RXAUTR disabled and verified IDLE reads, SPI polling reveals the DW1000 IS detecting preambles:

| Status | Meaning | Count |
|--------|---------|-------|
| 0x2001701 | RXPRD + RXSFDD + LDEDONE + **RXPHE** | ~10 |
| 0x2041301 | RXPRD + RXSFDD + **RXPHE** + **LDEERR** | ~7 |
| 0xFFFFFFFF | SPI corruption (false positive) | ~10 |

- **57% preamble/SFD detection** (17/30 packets)
- **0% successful frame decode** — every frame fails at PHY header
- The DW1000 hears the transmitter but headers are corrupted

### Finding 8: Signal Saturation Hypothesis

At <1m distance with 110kbps long-range mode (MODE_LONGDATA_RANGE_LOWPOWER = 2048 preamble, designed for 100s of meters), the receiver may be **saturated by excessive signal strength**. Evidence:
- RXPHE (PHY header error) on every frame
- RXSFDTO (SFD timeout) set — receiver detects preamble but header is corrupted
- Long-range mode has highest receiver gain/sensitivity — not ideal for close range

### Finding 9: setDefaults() in Loop is a No-Op (Not the Bug We Thought)

Investigated whether `DW1000.setDefaults()` called inside the TX/RX loop was overriding mode and RXAUTR settings. **Result: It's a no-op.**

`setDefaults()` (DW1000.cpp line 1288) checks `_deviceMode`:
- `TX_MODE` → empty block, does nothing
- `RX_MODE` → empty block, does nothing
- `IDLE_MODE` → resets everything (mode, RXAUTR, frame filter, etc.)

Since `newReceive()` sets `_deviceMode = RX_MODE` and `newTransmit()` sets `_deviceMode = TX_MODE`, calling `setDefaults()` after either of these does **nothing**. The mode and RXAUTR settings from `commitConfiguration()` in setup are preserved.

**Implication:** The 6.8Mbps test was NOT a mode mismatch — both TX and RX were actually at 6.8Mbps. PHY header errors occur at both 110kbps and 6.8Mbps.

### Finding 10: PIN_RST = 9 is WRONG for DWS1000 Shield (Critical!)

**Discovery:** All test code uses `PIN_RST = 9` (the generic library default), but the DWS1000 shield routes the DW1000's RST pin to **Arduino D7** (not D9).

**Impact of wrong pin:**
1. `DW1000.select(PIN_SS)` calls `reset()` internally
2. `reset()` toggles `_rst` (pin 9) LOW then INPUT for hardware reset
3. Pin 9 is NOT connected to DW1000 RST on DWS1000 shield
4. **The DW1000 never receives a hardware reset!**
5. Since `_rst != 0xFF`, `softReset()` (SPI-based) is also skipped

**Potential consequences of no reset:**
- Internal state machines in unknown states from power-on
- Receiver calibration values may not load properly
- PLL and LDE initialization may be incomplete
- Could explain persistent RXPHE on every frame despite good preamble detection
- Could also explain CLKPLL_LL (PLL losing lock) during RX

**Fix:** Change `PIN_RST = 7` in all code. Our own documentation from 2026-01-11 (`DWS1000_PINOUT_AND_FIX.md`) correctly specified pin 7 but it was never applied to the test code.

**Source:** `docs/findings/DWS1000_PINOUT_AND_FIX.md` line 63:
```cpp
const uint8_t PIN_RST = 7;   // DWS1000 uses pin 7 for reset
```

## Next Steps

1. **Test with PIN_RST = 7** — `test_rx_v8_reset_fix.cpp` created with correct reset pin
2. **If RXPHE resolves:** Proceed to TWR ranging
3. **If still failing:** Investigate PLL stability, try different channel/PRF settings

## Files Created This Session

- `tests/test_rx_irq_pin.cpp` — IRQ pin polling approach (zero results)
- `tests/test_rx_diagnostic.cpp` — Dual IRQ+SPI diagnostic
- `tests/test_rx_library_irq.cpp` — Library callback approach (zero results)
- `tests/test_rx_poll_optimized.cpp` — Double-read verified polling
- `tests/test_rx_simple_poll.cpp` — Window-based IDLE polling (revealed PHY header errors)
- `tests/test_rx_v8_reset_fix.cpp` — Correct PIN_RST=7 + clean loop
- `docs/known_issues.md` — Quick reference for recurring problems
- `docs/findings/IRQ_PIN_INVESTIGATION_2026-02-27.md` — This document
