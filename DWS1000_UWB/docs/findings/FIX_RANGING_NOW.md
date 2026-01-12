# QUICK FIX: Get DWS1000 Ranging Working

**Date:** 2026-01-11
**Issue:** Devices initialize but no ranging/no devices found

## Root Cause Found

The **DWS1000 shield (PCL298336)** routes the IRQ signal to **Arduino pin D8**, but Arduino Uno only supports hardware interrupts on **pins D2 and D3**.

The DW1000 library uses interrupts to detect received messages. Without a working interrupt, the devices can transmit but never know when they receive a response.

## The Fix (5 Minutes)

### Step 1: Add Jumper Wire

You need ONE jumper wire per Arduino:

```
Connect: D8 ──────────── D2
         (shield IRQ)    (interrupt pin)
```

Use a short female-female jumper wire or a piece of solid core wire.

### Step 2: Verify Connections

After adding the jumper, your wiring should be:

| Signal | Arduino Pin | Notes |
|--------|-------------|-------|
| RST    | D7          | Reset (DWS1000 shield hardwired) |
| IRQ    | D8 → D2     | Jumper wire added! |
| SS/CS  | D10         | SPI chip select |
| MOSI   | D11         | SPI data out |
| MISO   | D12         | SPI data in |
| SCK    | D13         | SPI clock |

### Step 3: Upload New Firmware

The main.cpp has been updated with the correct pin configuration:
- `PIN_RST = 7` (was 9)
- `PIN_IRQ = 2` (connects via jumper from D8)
- `PIN_SS = 10` (unchanged)

Upload to BOTH Arduinos:
```bash
# For ANCHOR (edit main.cpp: IS_ANCHOR = true)
pio run -t upload --upload-port /dev/ttyACM0

# For TAG (edit main.cpp: IS_ANCHOR = false)
pio run -t upload --upload-port /dev/ttyACM1
```

### Step 4: Test Ranging

1. Open two terminals:
```bash
# Terminal 1 - ANCHOR
pio device monitor --port /dev/ttyACM0 --baud 115200

# Terminal 2 - TAG
pio device monitor --port /dev/ttyACM1 --baud 115200
```

2. Send 's' to ANCHOR first, then to TAG
3. You should now see:
   - `[DEVICE] Found: XXXX` messages
   - `[RANGE] X.XX m (XX.XX cm)` measurements

## Expected Output

### Before Fix (current symptoms):
```
ANCHOR ready - Listening for TAGs
(nothing happens for minutes...)
```

### After Fix:
```
ANCHOR ready - Listening for TAGs
[DEVICE] Found: 3B9C
[RANGE] 0.86 m (86.36 cm) | Error: +0.00 cm | from 3B9C
[RANGE] 0.87 m (87.12 cm) | Error: +0.76 cm | from 3B9C
...
```

## Why This Works

1. **Before:** IRQ on D8 = no interrupt capability on Uno
2. **After:** IRQ jumpered to D2 = proper hardware interrupt

The DW1000 chip signals the Arduino when it receives a message via the IRQ pin. Without a working interrupt, the Arduino never processes incoming messages, so the ranging protocol can't complete.

## If Still Not Working

Check these in order:

1. **Device ID shows "DECA"?**
   - If yes: SPI working, interrupt is the issue
   - If "FFFF" or "0000": SPI not working, check shield seating

2. **Both devices show "DECA"?**
   - Both need working SPI

3. **Jumper wire secure?**
   - Wiggle test - should be solid connection

4. **Same USB hub?**
   - Avoid using same hub for both Arduinos (timing issues)

5. **Antenna connected?**
   - U.FL connector should click into place

## Files Changed

- `src/main.cpp` - Updated pin configuration
- `docs/findings/DWS1000_PINOUT_AND_FIX.md` - Detailed pinout documentation
- `docs/findings/FIX_RANGING_NOW.md` - This file
