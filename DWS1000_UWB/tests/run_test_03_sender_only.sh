#!/bin/bash
# Test 3: BasicSender Only - Automated (No User Input Required)
# Post Bug Fix Verification

set -e

PROJECT_ROOT="/home/devel/Desktop/SwarmLoc/DWS1000_UWB"
PORT="${1:-/dev/ttyACM0}"
DURATION="${2:-60}"
OUTPUT_FILE="$PROJECT_ROOT/tests/test_outputs/test03_sender_$(date +%Y%m%d_%H%M%S).txt"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

mkdir -p "$PROJECT_ROOT/tests/test_outputs"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test 3: BasicSender (Post Bug Fix)${NC}"
echo -e "${BLUE}========================================${NC}"
echo "Port: $PORT"
echo "Duration: ${DURATION}s"
echo "Output: $OUTPUT_FILE"
echo ""

# Create temp project
TEMP_DIR="/tmp/dw1000_test03_sender"
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/src" "$TEMP_DIR/lib"

# Copy files
cp "$PROJECT_ROOT/tests/test_02_library_examples/test_03_sender.ino" "$TEMP_DIR/src/main.cpp"
ln -sf "$PROJECT_ROOT/lib/DW1000" "$TEMP_DIR/lib/DW1000"

# Create platformio.ini
cat > "$TEMP_DIR/platformio.ini" << EOF
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = $PORT
monitor_speed = 9600
lib_deps = SPI
EOF

# Compile
echo -e "${YELLOW}Compiling...${NC}"
cd "$TEMP_DIR"
pio run -s

echo -e "${GREEN}✓ Compiled${NC}"
echo ""

# Upload
echo -e "${YELLOW}Uploading to $PORT...${NC}"
pio run -t upload -s

echo -e "${GREEN}✓ Uploaded${NC}"
echo ""

# Monitor
echo -e "${YELLOW}Monitoring for ${DURATION}s...${NC}"
echo ""

timeout $DURATION python3 -u - "$PORT" <<'PYTHON_SCRIPT' 2>&1 | tee "$OUTPUT_FILE"
import serial
import time
import sys

port = sys.argv[1]
try:
    ser = serial.Serial(port, 9600, timeout=1)
    time.sleep(2)

    start_time = time.time()
    line_count = 0
    tx_count = 0

    print("=== TEST 3: BasicSender Started ===\n")

    while time.time() - start_time < 60:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                timestamp = time.strftime('[%H:%M:%S]')
                print(f"{timestamp} {line}")
                sys.stdout.flush()
                line_count += 1
                if "Transmitting packet" in line or "Transmitted" in line:
                    tx_count += 1

    elapsed = time.time() - start_time
    print(f"\n=== Test Complete ===")
    print(f"Duration: {elapsed:.1f}s")
    print(f"Lines captured: {line_count}")
    print(f"TX-related lines: {tx_count}")
    if tx_count > 2:
        print("✓ SUCCESS: Sender transmitted multiple packets (bug fix working!)")
    else:
        print("✗ FAIL: Sender only sent initial packet (bug may still exist)")

    ser.close()

except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
PYTHON_SCRIPT

echo ""
echo -e "${GREEN}Test 3 Complete${NC}"
echo "Output saved to: $OUTPUT_FILE"
