# USB Hub Issue - Quick Fix

**DIAGNOSIS**: Both Arduinos are connected through a USB hub on Bus 3
- ACM0: Bus 3, Port 6 (works)
- ACM1: Bus 3, Port 5 (fails uploads)

**ROOT CAUSE**: USB hubs cause timing issues with Arduino bootloaders, especially for secondary devices (ACM1).

---

## 🚀 Quick Test (2 minutes)

### Step 1: Unplug ACM1 Arduino

Physically disconnect the Arduino that's currently on `/dev/ttyACM1`.

### Step 2: Plug into DIRECT USB Port

Plug it into a **direct** USB port on your computer:
- ✅ **BEST**: Rear panel USB port (directly on motherboard)
- ✅ **GOOD**: Front panel USB port connected to motherboard header
- ❌ **AVOID**: Any USB hub, docking station, or extender

### Step 3: Wait for Re-enumeration

Wait 5 seconds for the device to re-enumerate. It might appear as:
- `/dev/ttyACM1` (same name)
- `/dev/ttyACM2` (new name)
- Or even `/dev/ttyACM0` if ACM0 was unplugged

Check which port it's on:
```bash
ls -l /dev/ttyACM*
```

### Step 4: Test Upload

```bash
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB

# Update main.cpp to TAG mode
sed -i 's/#define IS_ANCHOR true/#define IS_ANCHOR false/' src/main.cpp

# Try upload (use the actual port number)
pio run --target upload --upload-port /dev/ttyACM1
```

---

## 💡 Expected Result

**If USB hub was the problem**:
- Upload will succeed immediately ✅
- No more "stk500_getsync" errors ✅
- Both Arduinos can be used normally ✅

**If issue persists**:
- Then it's likely a bootloader issue
- Proceed with bootloader recovery (see BOOTLOADER_RECOVERY_ISP.md)

---

## 🔧 Optimal USB Configuration

### For Best Results:
1. **Arduino #1 (ANCHOR)**: Direct USB port (rear panel)
2. **Arduino #2 (TAG)**: Different direct USB port (rear panel)
3. **RPLidar**: Can stay on hub (doesn't need upload)

### Avoid:
- ❌ Connecting both Arduinos to same USB hub
- ❌ Using front panel USB (often hub-based)
- ❌ Using USB extenders or docking stations

---

## 📊 Why USB Hubs Cause This

From research (ACM1_SPECIFIC_TROUBLESHOOTING.md):
- **Power delivery**: Hubs provide less stable power
- **Enumeration delays**: Secondary ports experience 10-20 second delays
- **Signal integrity**: Longer cables introduce noise
- **Bootloader timing**: Arduino bootloader has ~1 second window
- **Hub chip issues**: Some hub chips (SMSC USB2512) are particularly problematic

Arduino officially recommends **avoiding USB hubs for uploads**.

---

## ✅ Success Criteria

After moving to direct USB:
1. `pio run --target upload` completes in < 15 seconds
2. No "not in sync" errors
3. Can upload to both Arduinos without issues
4. Ranging test can proceed normally

---

## 🎯 Next Steps After Fix

Once both Arduinos upload successfully:

1. **Upload ANCHOR to one device**:
   ```bash
   sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR true/' src/main.cpp
   pio run --target upload --upload-port /dev/ttyACM0
   ```

2. **Upload TAG to other device**:
   ```bash
   sed -i 's/#define IS_ANCHOR true/#define IS_ANCHOR false/' src/main.cpp
   pio run --target upload --upload-port /dev/ttyACM1
   ```

3. **Start ranging test**:
   - Open two serial monitors
   - Send start command to both
   - Observe distance measurements!

---

**TRY THIS FIRST** - It's the fastest solution (2 minutes vs 45 minutes for bootloader recovery)
