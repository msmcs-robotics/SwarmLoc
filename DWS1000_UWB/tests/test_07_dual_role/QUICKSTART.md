# Quick Start Guide: Dual-Role Firmware

**Goal**: Get the dual-role firmware running in 5 minutes.

---

## Prerequisites

- Arduino Uno + DW1000 shield
- USB cable
- Arduino IDE or PlatformIO
- Serial terminal (screen, minicom, or Arduino Serial Monitor)

---

## Step 1: Configure Node Settings (2 minutes)

Open `test_07_dual_role.ino` and edit these lines:

```cpp
// MAKE UNIQUE FOR EACH NODE
const uint8_t NODE_ID = 1;  // Change for each node: 1, 2, 3, 4
const char* NODE_NAME = "Node01";

// MAKE UNIQUE ADDRESSES (change last byte)
const char* MY_ANCHOR_ADDRESS = "82:17:5B:D5:A9:9A:E2:9A";  // 9A → 9B, 9C, 9D
const char* MY_TAG_ADDRESS = "7D:00:22:EA:82:60:3B:9A";     // 9A → 9B, 9C, 9D

// For multiple TAGs, set total count
#define NUM_NODES 4  // Total number of TAG nodes (if using TDMA)
```

**IMPORTANT**: Each node must have **unique addresses**. Change the last byte (9A, 9B, 9C, 9D).

---

## Step 2: Upload Firmware (1 minute)

### Using Arduino IDE:
1. Open `test_07_dual_role.ino`
2. Select **Board**: Arduino Uno
3. Select **Port**: /dev/ttyACM0 (or COM port on Windows)
4. Click **Upload**

### Using Command Line:
```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_07_dual_role
# Upload via Arduino CLI or PlatformIO
```

---

## Step 3: Connect Serial Terminal (1 minute)

**Baud Rate**: 115200

### Option A: Screen (Linux/Mac)
```bash
screen /dev/ttyACM0 115200
```

### Option B: Arduino Serial Monitor
1. Tools → Serial Monitor
2. Set baud rate to **115200**

### Option C: Minicom (Linux)
```bash
minicom -D /dev/ttyACM0 -b 115200
```

---

## Step 4: Verify Boot (1 minute)

You should see:

```
========================================
SwarmLoc Dual-Role Firmware v1.0
========================================
Node: Node01
Node ID: 1
Current role: TAG

Starting as TAG (initiator/mobile)
Address: 7D:00:22:EA:82:60:3B:9A
TAG mode active - searching for anchors...
TDMA enabled - Slot 0 of 4

Ready!
========================================
```

**If you see this, firmware is working!**

---

## Step 5: Test Role Switching (30 seconds)

### Switch to ANCHOR:
1. Type `A` (uppercase or lowercase, both work)
2. Arduino will reset (~5 seconds)
3. Verify new role: `Current role: ANCHOR`

### Switch to TAG:
1. Type `T`
2. Arduino will reset (~5 seconds)
3. Verify new role: `Current role: TAG`

### View Status:
Type `S` to see:
```
========================================
SYSTEM STATUS
========================================
Node Name: Node01
Node ID: 1
Current Role: TAG
Uptime: 45 seconds
Range Count: 0
Free RAM: 452 bytes
========================================
```

---

## Common Commands

| Key | Action | Result |
|-----|--------|--------|
| `A` | Switch to ANCHOR | Reset → ANCHOR mode |
| `T` | Switch to TAG | Reset → TAG mode |
| `S` | Show status | Display current state |
| `R` | Reset | Manual restart |
| `H` | Help | Show command list |

---

## Testing Ranging (Two Nodes)

### Setup:
- **Node 1**: Press `A` → becomes ANCHOR
- **Node 2**: Press `T` → becomes TAG
- **Distance**: Place 1-2 meters apart

### Expected Output:

**Node 1 (ANCHOR):**
```
[DISCOVERY] New tag detected: 0x7D00
[RANGE] Tag 7D00 | Distance: 1.23 m | RX Power: -85.2 dBm
[RANGE] Tag 7D00 | Distance: 1.24 m | RX Power: -85.1 dBm
```

**Node 2 (TAG):**
```
[DISCOVERY] New anchor found: 0x8217
[RANGE] Anchor 8217 | Distance: 1.23 m | RX Power: -84.8 dBm
[RANGE] Anchor 8217 | Distance: 1.24 m | RX Power: -84.7 dBm
```

**If you see ranging messages, SUCCESS!**

---

## Troubleshooting

### No Serial Output
- Check baud rate: **must be 115200**
- Press Arduino reset button
- Try different USB port
- Check USB cable (must support data, not just charging)

### Firmware Won't Compile
- Verify library path: `lib/DW1000/src/` exists
- Check `#include "DW1000Ranging.h"`
- Ensure SPI library is available

### Role Switch Doesn't Work
- Wait full 5 seconds for reset
- Check EEPROM initialization message
- Verify "ROLE SWITCH REQUEST" appears

### No Ranging Messages
- **Apply interrupt bug fix** (see README.md "Known Limitations #5")
- Check distance: nodes should be 0.5-10m apart
- Verify one node is ANCHOR, other is TAG
- Ensure addresses are unique
- Check IRQ pin connection (D2)

---

## Next Steps

1. **Test ranging accuracy**: Measure at 1m, 2m, 3m distances
2. **Test TDMA**: Add 3rd TAG node, verify time-slotted operation
3. **Long-term test**: Run for 1+ hour, check stability
4. **Document results**: Record accuracy, update rate, issues

---

## Configuration Examples

### Single TAG, No TDMA:
```cpp
const uint8_t NODE_ID = 1;
#define NUM_NODES 1  // TDMA disabled
```

### Multiple TAGs (2 nodes):
```cpp
// Node 1:
const uint8_t NODE_ID = 1;
#define NUM_NODES 2

// Node 2:
const uint8_t NODE_ID = 2;
#define NUM_NODES 2
```

### Multiple TAGs (4 nodes):
```cpp
// Configure each node with NODE_ID = 1, 2, 3, or 4
// Set NUM_NODES = 4 in all nodes
```

---

## Files in This Directory

```
test_07_dual_role/
├── test_07_dual_role.ino   # Main firmware (upload this)
├── README.md               # Full documentation
├── DESIGN.md               # Design decisions
├── QUICKSTART.md           # This file
└── test_dual_role.sh       # Automated test script
```

---

## Support

**Issue**: No ranging, interrupt bug suspected
**Fix**: See `test_06_ranging/README.md` for library patch

**Issue**: Memory errors
**Check**: `Free RAM` in status output (should be >400 bytes)

**Issue**: TDMA collisions
**Fix**: Verify unique NODE_ID and correct NUM_NODES

---

## Success Checklist

- [ ] Firmware compiles without errors
- [ ] Serial output appears at 115200 baud
- [ ] Boot message shows node name and role
- [ ] Command 'S' displays status
- [ ] Command 'A' switches to ANCHOR (after reset)
- [ ] Command 'T' switches to TAG (after reset)
- [ ] Two nodes range successfully (1 ANCHOR + 1 TAG)
- [ ] Ranging update rate ≥1 Hz
- [ ] Free RAM ≥400 bytes

**If all checked, your firmware is working correctly!**

---

**Time to complete**: ~5 minutes for single node, ~10 minutes for ranging test

**Next**: Read full README.md for advanced features and testing

---

**End of Quick Start Guide**
