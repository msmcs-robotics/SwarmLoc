#!/bin/bash
# Quick ranging test - capture output from both devices

echo "=================================="
echo "Quick DW1000 Ranging Test"
echo "=================================="
echo ""
echo "Both devices are already uploaded:"
echo "  /dev/ttyACM0 = TAG"
echo "  /dev/ttyACM1 = ANCHOR"
echo ""
echo "They are waiting for start command (send newline)"
echo ""

# Install pyserial if needed
python3 -c "import serial" 2>/dev/null || {
    echo "Installing pyserial..."
    pip3 install pyserial --user -q
}

# Create Python monitor script
cat > /tmp/quick_monitor.py << 'PYEOF'
import serial
import sys
import time
from datetime import datetime

def test_serial(port, name):
    print(f"\n{'='*60}")
    print(f"Testing {name} on {port}")
    print(f"{'='*60}")

    try:
        ser = serial.Serial(port, 115200, timeout=2)
        time.sleep(1)

        # Send start command
        print(f"[{name}] Sending start command...")
        ser.write(b'\n')
        time.sleep(0.5)

        # Read for 15 seconds
        start_time = time.time()
        line_count = 0
        range_count = 0

        print(f"[{name}] Reading output for 15 seconds...")
        print(f"[{name}] " + "-"*50)

        while (time.time() - start_time) < 15:
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                        print(f"[{name}] [{timestamp}] {line}")
                        line_count += 1
                        if "Range:" in line or "range:" in line:
                            range_count += 1
                except:
                    pass
            time.sleep(0.01)

        ser.close()

        print(f"[{name}] " + "-"*50)
        print(f"[{name}] Total lines: {line_count}, Range measurements: {range_count}")
        return line_count, range_count

    except Exception as e:
        print(f"[{name}] ERROR: {e}")
        return 0, 0

print("\nStarting ranging test...")
print("Testing TAG first, then ANCHOR")

tag_lines, tag_ranges = test_serial('/dev/ttyACM0', 'TAG   ')
time.sleep(2)
anchor_lines, anchor_ranges = test_serial('/dev/ttyACM1', 'ANCHOR')

print(f"\n{'='*60}")
print("TEST SUMMARY")
print(f"{'='*60}")
print(f"TAG:    {tag_lines} lines, {tag_ranges} range measurements")
print(f"ANCHOR: {anchor_lines} lines, {anchor_ranges} range measurements")
print("")

if tag_ranges > 0 or anchor_ranges > 0:
    print("✓ SUCCESS: Ranging measurements detected!")
else:
    print("⚠ WARNING: No range measurements detected")
    print("  Devices may need to be reset or moved closer together")

PYEOF

# Run the test
python3 /tmp/quick_monitor.py

echo ""
echo "Test complete! Check output above for ranging data."
