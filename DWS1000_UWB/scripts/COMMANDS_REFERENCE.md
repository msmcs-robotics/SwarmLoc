# Commands Reference - DWS1000 UWB Serial Monitoring & Upload

## THE GOLDEN RULE: NEVER USE BLOCKING SERIAL READS

**DO NOT:**
- `cat /dev/ttyACM0` (blocks forever)
- `timeout 30 cat /dev/ttyACM0` (often misses boot output, unreliable)
- `stty ... && cat ...` (blocks, loses data)
- `pio device monitor` (interactive, blocks agent)

**ALWAYS USE the scripts below** which capture to files and exit cleanly.

---

## Quick Reference

### Upload and Capture (most common)
```bash
# Compile current src/main.cpp, upload, reset, capture output
./scripts/upload_and_capture.sh /dev/ttyACM0 30

# With custom output file
./scripts/upload_and_capture.sh /dev/ttyACM0 30 /tmp/my_test.txt

# Stop early when specific string appears
./scripts/upload_and_capture.sh /dev/ttyACM0 60 /tmp/out.txt "Test complete"
```

### Capture Only (device already programmed)
```bash
# Reset and capture - no recompile/upload
./scripts/capture_only.sh /dev/ttyACM0 30

# With early exit on marker
./scripts/capture_only.sh /dev/ttyACM0 60 /tmp/out.txt "SUMMARY"
```

### Dual Capture (two devices)
```bash
# Capture from both ports simultaneously
./scripts/capture_dual.sh /dev/ttyACM0 /dev/ttyACM1 30
```

### Upload Two Different Firmwares + Capture
```bash
# Upload TX to ACM0, RX to ACM1, capture both
./scripts/upload_dual_and_capture.sh \
    tests/test_tx.cpp /dev/ttyACM0 \
    tests/test_rx.cpp /dev/ttyACM1 \
    30
```

---

## Non-Blocking Pattern for Claude Agent

When using these from the Bash tool, use `run_in_background: true` for long captures:

```
# Start capture in background
Bash(run_in_background=true): ./scripts/capture_only.sh /dev/ttyACM0 45 /tmp/output.txt

# ... do other work ...

# Check output later
Read: /tmp/output.txt
```

For shorter tests (< 30s), running directly is fine since the scripts have proper timeouts and early-exit markers.

---

## Low-Level Commands (when scripts aren't suitable)

### PlatformIO
```bash
# Compile
pio run 2>&1 | tail -5

# Upload to specific port
pio run -t upload --upload-port /dev/ttyACM0 2>&1 | tail -5

# Clean build
pio run -t clean
```

### Port Detection
```bash
# List available serial ports
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null

# Check what's connected
lsusb | grep -i arduino
```

### Python Serial (inline, for custom one-offs)
```bash
python3 -c "
import serial, time
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
ser.dtr = False; time.sleep(0.1); ser.dtr = True; time.sleep(2)
lines = []
start = time.time()
while time.time() - start < 15:
    line = ser.readline().decode('utf-8', errors='ignore').strip()
    if line:
        lines.append(line)
        print(line)
ser.close()
"
```

---

## File Locations

| File | Purpose |
|------|---------|
| `scripts/upload_and_capture.sh` | Compile + upload + capture serial |
| `scripts/capture_only.sh` | Reset + capture (no upload) |
| `scripts/capture_dual.sh` | Capture two ports simultaneously |
| `scripts/upload_dual_and_capture.sh` | Upload different firmware to 2 devices + capture |
| `scripts/capture_serial.py` | Low-level Python capture (legacy) |
| `scripts/test_utils.sh` | General test utilities (detect, monitor, compile, upload) |

## Typical Workflows

### Test new firmware on one device
```bash
# Edit src/main.cpp, then:
./scripts/upload_and_capture.sh /dev/ttyACM0 30 /tmp/test.txt "Test complete"
```

### TX/RX test between two devices
```bash
# Prepare test files in tests/, then:
./scripts/upload_dual_and_capture.sh \
    tests/test_tx_with_ldo.cpp /dev/ttyACM0 \
    tests/test_rx_with_ldo.cpp /dev/ttyACM1 \
    30
```

### Re-run capture on already-programmed device
```bash
./scripts/capture_only.sh /dev/ttyACM0 20 /tmp/rerun.txt
```

### Monitor device periodically (background)
```bash
# Run in background, check /tmp/bg_output.txt periodically
./scripts/capture_only.sh /dev/ttyACM0 120 /tmp/bg_output.txt &
# Later: cat /tmp/bg_output.txt
```
