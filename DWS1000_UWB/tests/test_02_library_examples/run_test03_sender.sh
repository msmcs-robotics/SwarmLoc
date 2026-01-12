#!/bin/bash
# Test 3: BasicSender - Post Bug Fix
# Upload and monitor for 60 seconds

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
PORT=${1:-/dev/ttyACM0}
OUTPUT_FILE="$SCRIPT_DIR/test03_sender_output_$(date +%Y%m%d_%H%M%S).txt"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Test 3: BasicSender (Post Bug Fix)  ${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Port: $PORT"
echo "Output: $OUTPUT_FILE"
echo ""

# Create temporary PlatformIO project
TEMP_DIR="/tmp/dwm3000_sender_test"
echo "Creating temporary PlatformIO project..."
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/src"
mkdir -p "$TEMP_DIR/lib"

# Copy test file
cp "$SCRIPT_DIR/test_03_sender.ino" "$TEMP_DIR/src/main.cpp"

# Create symlink to DW1000 library
ln -s "$PROJECT_ROOT/lib/DW1000" "$TEMP_DIR/lib/DW1000"

# Create platformio.ini
cat > "$TEMP_DIR/platformio.ini" << EOF
[platformio]
default_envs = uno

[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = $PORT
monitor_port = $PORT
monitor_speed = 9600
lib_deps = SPI
EOF

# Compile
echo -e "${YELLOW}Compiling...${NC}"
cd "$TEMP_DIR"
pio run

if [ $? -ne 0 ]; then
    echo -e "${RED}Compilation failed!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Compilation successful${NC}"
echo ""

# Upload
echo -e "${YELLOW}Uploading to $PORT...${NC}"
pio run -t upload

if [ $? -ne 0 ]; then
    echo -e "${RED}Upload failed!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Upload successful${NC}"
echo ""

# Monitor for 60 seconds
echo -e "${YELLOW}Monitoring for 60 seconds...${NC}"
echo ""

timeout 60 python3 - <<'PYTHON' | tee "$OUTPUT_FILE"
import serial
import time
import sys

try:
    ser = serial.Serial(sys.argv[1], 9600, timeout=1)
    time.sleep(2)  # Wait for Arduino reset

    start_time = time.time()
    line_count = 0

    print("=== BasicSender Test Started ===")
    print("")

    while time.time() - start_time < 60:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                timestamp = time.strftime('[%H:%M:%S]')
                print(f"{timestamp} {line}")
                sys.stdout.flush()
                line_count += 1

    print("")
    print(f"=== Test Complete: {line_count} lines captured in 60s ===")
    ser.close()

except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
PYTHON "$PORT"

echo ""
echo -e "${GREEN}Test 3 complete!${NC}"
echo "Output saved to: $OUTPUT_FILE"
echo ""
