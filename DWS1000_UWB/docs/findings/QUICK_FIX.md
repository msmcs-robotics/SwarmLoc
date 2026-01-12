# DW1000 Interrupt Bug - Quick Fix Guide

## Problem
BasicSender and BasicReceiver examples don't work - interrupts never fire.

## Root Cause
Bug in `DW1000.cpp` line 993-996: Uses wrong constant (`LEN_SYS_STATUS` instead of `LEN_SYS_MASK`), causing buffer overrun that corrupts interrupt mask register.

## Fix

**File to edit:** `DWS1000_UWB/lib/DW1000/src/DW1000.cpp`

**Find lines 992-997:**
```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
	setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);
	setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT, val);
	setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT, val);
	setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);
}
```

**Change to:**
```cpp
void DW1000Class::interruptOnReceiveFailed(boolean val) {
	setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);
	setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT, val);
	setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT, val);
	setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
}
```

**What changed:** Replace `LEN_SYS_STATUS` with `LEN_SYS_MASK` in all 4 lines.

## Apply Patch

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/lib/DW1000/src
patch < /home/devel/Desktop/SwarmLoc/docs/findings/interrupt_bug_fix.patch
```

## Test

1. Re-upload BasicSender to one Arduino
2. Re-upload BasicReceiver to another Arduino
3. Open Serial Monitor on both (9600 baud)
4. Sender should continuously transmit packets #0, #1, #2...
5. Receiver should continuously receive and display packets

## Expected Output

**Sender:**
```
Transmitting packet ... #0
ARDUINO delay sent [ms] ... 10
Processed packet ... #0
Sent timestamp ... 123456789
Transmitting packet ... #1
...
```

**Receiver:**
```
Received message ... #1
Data is ... Hello DW1000, it's #0
FP power is [dBm] ... -85.2
RX power is [dBm] ... -82.1
Received message ... #2
...
```

## Alternative: Manual Fix in Arduino IDE

1. Open Arduino IDE
2. Navigate to library folder: `Documents/Arduino/libraries/DW1000/src/` (or wherever your libraries are)
3. Open `DW1000.cpp`
4. Press Ctrl+F to find `interruptOnReceiveFailed`
5. Change the 4 instances of `LEN_SYS_STATUS` to `LEN_SYS_MASK`
6. Save file
7. Restart Arduino IDE
8. Re-upload your sketches

## Why This Works

- `LEN_SYS_STATUS` = 5 bytes
- `LEN_SYS_MASK` = 4 bytes
- Both share same bit definitions, but different lengths
- Using wrong length corrupts memory and interrupt configuration
- Fix ensures correct buffer size is used

## Full Documentation

See `interrupt_debugging.md` for complete analysis, alternative solutions, and troubleshooting.
