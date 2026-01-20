# Serial Monitoring Commands Reference

## Problem
Serial monitoring with `cat` or `timeout cat` often hangs or misses output.

## Solution: Use these patterns

### 1. Quick read with proper setup (PREFERRED)
```bash
# Set baud rate, reset device via DTR, capture output
stty -F /dev/ttyACM0 115200 raw -echo && \
  python3 -c "import serial; s=serial.Serial('/dev/ttyACM0', 115200); s.dtr=False; s.dtr=True" 2>/dev/null; \
  timeout 30 head -n 100 < /dev/ttyACM0
```

### 2. Python one-liner (most reliable)
```bash
timeout 30 python3 -c "
import serial, time
s = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
s.dtr = False; time.sleep(0.1); s.dtr = True  # Reset
time.sleep(0.5)  # Wait for boot
lines = 0
while lines < 100:
    line = s.readline().decode('utf-8', errors='ignore').strip()
    if line: print(line); lines += 1
"
```

### 3. Capture to file then read
```bash
# Capture in background
stty -F /dev/ttyACM0 115200 raw -echo
timeout 30 cat /dev/ttyACM0 > /tmp/serial_out.txt &
PID=$!
sleep 2  # Let it capture
# Reset device to trigger output
python3 -c "import serial; s=serial.Serial('/dev/ttyACM0', 115200); s.dtr=False; s.dtr=True" 2>/dev/null
wait $PID 2>/dev/null
cat /tmp/serial_out.txt
```

### 4. Screen (interactive - avoid in scripts)
```bash
screen /dev/ttyACM0 115200
# Ctrl+A then K to kill
```

### 5. PlatformIO monitor (best for development)
```bash
pio device monitor -p /dev/ttyACM0 -b 115200
# Ctrl+C to exit
```

## Key Points
- Always set baud rate with `stty` first
- Use `-hupcl` to prevent reset on open (or omit to trigger reset)
- Use `timeout` to prevent hanging
- Use `head -n N` to limit lines
- Reset device with DTR toggle to capture from boot
- Python serial is most reliable for automation

## Device Ports
- DEV0 (device 1): /dev/ttyACM0
- DEV1 (device 2): /dev/ttyACM1
