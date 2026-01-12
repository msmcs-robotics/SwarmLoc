#!/bin/bash
# Test 6: DW1000Ranging TAG + ANCHOR - Automated (No User Input)
# Post Bug Fix Verification - THE CRITICAL TEST

set -e

PROJECT_ROOT="/home/devel/Desktop/SwarmLoc/DWS1000_UWB"
PORT_TAG="${1:-/dev/ttyACM0}"
PORT_ANCHOR="${2:-/dev/ttyACM1}"
DURATION="${3:-120}"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

mkdir -p "$PROJECT_ROOT/tests/test_outputs"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test 6: DW1000Ranging (Post Bug Fix)${NC}"
echo -e "${BLUE}THE CRITICAL TEST${NC}"
echo -e "${BLUE}========================================${NC}"
echo "TAG Port: $PORT_TAG"
echo "ANCHOR Port: $PORT_ANCHOR"
echo "Duration: ${DURATION}s"
echo ""

# Upload TAG
echo -e "${YELLOW}=== Uploading TAG to $PORT_TAG ===${NC}"
TEMP_DIR="/tmp/dw1000_test06_tag"
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/src" "$TEMP_DIR/lib"
cp "$PROJECT_ROOT/tests/test_06_ranging/test_clean.ino" "$TEMP_DIR/src/main.cpp"
sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR false/' "$TEMP_DIR/src/main.cpp"
ln -sf "$PROJECT_ROOT/lib/DW1000" "$TEMP_DIR/lib/DW1000"

cat > "$TEMP_DIR/platformio.ini" << EOF
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = $PORT_TAG
monitor_speed = 115200
lib_deps = SPI
EOF

cd "$TEMP_DIR"
pio run -s && pio run -t upload -s
echo -e "${GREEN}✓ TAG uploaded${NC}\n"

sleep 2

# Upload ANCHOR
echo -e "${YELLOW}=== Uploading ANCHOR to $PORT_ANCHOR ===${NC}"
TEMP_DIR="/tmp/dw1000_test06_anchor"
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/src" "$TEMP_DIR/lib"
cp "$PROJECT_ROOT/tests/test_06_ranging/test_clean.ino" "$TEMP_DIR/src/main.cpp"
sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR true/' "$TEMP_DIR/src/main.cpp"
ln -sf "$PROJECT_ROOT/lib/DW1000" "$TEMP_DIR/lib/DW1000"

cat > "$TEMP_DIR/platformio.ini" << EOF
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = $PORT_ANCHOR
monitor_speed = 115200
lib_deps = SPI
EOF

cd "$TEMP_DIR"
pio run -s && pio run -t upload -s
echo -e "${GREEN}✓ ANCHOR uploaded${NC}\n"

sleep 3

# Monitor both
echo -e "${YELLOW}=== Monitoring TAG and ANCHOR for ${DURATION}s ===${NC}"
echo -e "${YELLOW}Looking for 'Device found' and 'Range:' messages...${NC}\n"

OUTPUT_TAG="$PROJECT_ROOT/tests/test_outputs/test06_tag_$(date +%Y%m%d_%H%M%S).txt"
OUTPUT_ANCHOR="$PROJECT_ROOT/tests/test_outputs/test06_anchor_$(date +%Y%m%d_%H%M%S).txt"

# Monitor TAG in background
timeout $DURATION python3 -u - "$PORT_TAG" "TAG" > "$OUTPUT_TAG" 2>&1 &
PID_TAG=$!

# Monitor ANCHOR in foreground
timeout $DURATION python3 -u - "$PORT_ANCHOR" "ANCHOR" > "$OUTPUT_ANCHOR" 2>&1 <<'PYTHON_SCRIPT'
import serial
import time
import sys
import re

port = sys.argv[1]
name = sys.argv[2]

try:
    ser = serial.Serial(port, 115200, timeout=1)
    time.sleep(2)

    start_time = time.time()
    line_count = 0
    device_found = False
    range_count = 0
    distances = []

    print(f"=== {name} Monitor Started ===\n")

    while time.time() - start_time < 120:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                timestamp = time.strftime('[%H:%M:%S]')
                print(f"{timestamp} {line}")
                sys.stdout.flush()
                line_count += 1

                if "Device found" in line:
                    device_found = True
                    print(f"{timestamp} *** DEVICE DISCOVERY SUCCESSFUL! ***")

                if "Range:" in line:
                    range_count += 1
                    # Extract distance
                    match = re.search(r'Range:\s+([\d.]+)\s+m', line)
                    if match:
                        dist = float(match.group(1))
                        distances.append(dist)
                        if range_count == 1:
                            print(f"{timestamp} *** FIRST RANGING MEASUREMENT! ***")

    elapsed = time.time() - start_time
    print(f"\n=== {name} Test Complete ===")
    print(f"Duration: {elapsed:.1f}s")
    print(f"Lines: {line_count}")
    print(f"Device found: {device_found}")
    print(f"Range measurements: {range_count}")

    if distances:
        avg_dist = sum(distances) / len(distances)
        print(f"Average distance: {avg_dist:.2f} m")
        print(f"Distance range: {min(distances):.2f} - {max(distances):.2f} m")

    if device_found and range_count > 10:
        print("\n*** SUCCESS: Ranging protocol fully functional! ***")
    elif device_found:
        print("\n*** PARTIAL: Device found but limited ranging ***")
    else:
        print("\n*** FAIL: No device discovery (bug fix may not have worked) ***")

    ser.close()

except Exception as e:
    print(f"Error: {e}")
PYTHON_SCRIPT

# Wait for TAG monitor to finish
wait $PID_TAG

# Analyze results
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test 6 Results - CRITICAL ANALYSIS${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check for success indicators
TAG_DEVICE_FOUND=$(grep -c "Device found" "$OUTPUT_TAG" || echo "0")
TAG_RANGE_COUNT=$(grep -c "Range:" "$OUTPUT_TAG" || echo "0")
ANCHOR_DEVICE_FOUND=$(grep -c "Device found" "$OUTPUT_ANCHOR" || echo "0")
ANCHOR_RANGE_COUNT=$(grep -c "Range:" "$OUTPUT_ANCHOR" || echo "0")

echo "TAG:"
echo "  Device found events: $TAG_DEVICE_FOUND"
echo "  Range measurements: $TAG_RANGE_COUNT"
echo ""

echo "ANCHOR:"
echo "  Device found events: $ANCHOR_DEVICE_FOUND"
echo "  Range measurements: $ANCHOR_RANGE_COUNT"
echo ""

# Determine success
TOTAL_RANGES=$((TAG_RANGE_COUNT + ANCHOR_RANGE_COUNT))

if [ "$TOTAL_RANGES" -gt 20 ]; then
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}✓✓✓ SUCCESS ✓✓✓${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}Bug fix SUCCESSFUL!${NC}"
    echo -e "${GREEN}Ranging protocol fully functional!${NC}"
    echo -e "${GREEN}Interrupts are firing correctly!${NC}"
    echo -e "${GREEN}========================================${NC}"
elif [ "$TOTAL_RANGES" -gt 0 ]; then
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}⚠ PARTIAL SUCCESS ⚠${NC}"
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Some ranging working but unstable${NC}"
    echo -e "${YELLOW}May need further debugging${NC}"
    echo -e "${YELLOW}========================================${NC}"
else
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}✗✗✗ FAILURE ✗✗✗${NC}"
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}No ranging measurements${NC}"
    echo -e "${RED}Bug fix may not have worked${NC}"
    echo -e "${RED}Check library compilation${NC}"
    echo -e "${RED}========================================${NC}"
fi

echo ""
echo "Full outputs saved to:"
echo "  TAG: $OUTPUT_TAG"
echo "  ANCHOR: $OUTPUT_ANCHOR"
echo ""
echo "Review outputs with:"
echo "  cat $OUTPUT_TAG"
echo "  cat $OUTPUT_ANCHOR"
