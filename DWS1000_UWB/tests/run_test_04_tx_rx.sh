#!/bin/bash
# Test 4: BasicSender + BasicReceiver - Automated (No User Input)
# Post Bug Fix Verification

set -e

PROJECT_ROOT="/home/devel/Desktop/SwarmLoc/DWS1000_UWB"
PORT1="${1:-/dev/ttyACM0}"
PORT2="${2:-/dev/ttyACM1}"
DURATION="${3:-60}"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

mkdir -p "$PROJECT_ROOT/tests/test_outputs"

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}Test 4: BasicSender + BasicReceiver (Post Bug Fix)${NC}"
echo -e "${BLUE}================================================${NC}"
echo "Sender Port: $PORT1"
echo "Receiver Port: $PORT2"
echo "Duration: ${DURATION}s"
echo ""

# Upload Sender
echo -e "${YELLOW}=== Uploading Sender to $PORT1 ===${NC}"
TEMP_DIR="/tmp/dw1000_test04_sender"
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/src" "$TEMP_DIR/lib"
cp "$PROJECT_ROOT/tests/test_02_library_examples/test_03_sender.ino" "$TEMP_DIR/src/main.cpp"
ln -sf "$PROJECT_ROOT/lib/DW1000" "$TEMP_DIR/lib/DW1000"

cat > "$TEMP_DIR/platformio.ini" << EOF
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = $PORT1
monitor_speed = 9600
lib_deps = SPI
EOF

cd "$TEMP_DIR"
pio run -s && pio run -t upload -s
echo -e "${GREEN}✓ Sender uploaded${NC}\n"

# Upload Receiver
echo -e "${YELLOW}=== Uploading Receiver to $PORT2 ===${NC}"
TEMP_DIR="/tmp/dw1000_test04_receiver"
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/src" "$TEMP_DIR/lib"
cp "$PROJECT_ROOT/tests/test_02_library_examples/test_04_receiver.ino" "$TEMP_DIR/src/main.cpp"
ln -sf "$PROJECT_ROOT/lib/DW1000" "$TEMP_DIR/lib/DW1000"

cat > "$TEMP_DIR/platformio.ini" << EOF
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = $PORT2
monitor_speed = 9600
lib_deps = SPI
EOF

cd "$TEMP_DIR"
pio run -s && pio run -t upload -s
echo -e "${GREEN}✓ Receiver uploaded${NC}\n"

# Monitor both
echo -e "${YELLOW}=== Monitoring both devices for ${DURATION}s ===${NC}\n"

OUTPUT_SENDER="$PROJECT_ROOT/tests/test_outputs/test04_sender_$(date +%Y%m%d_%H%M%S).txt"
OUTPUT_RECEIVER="$PROJECT_ROOT/tests/test_outputs/test04_receiver_$(date +%Y%m%d_%H%M%S).txt"

# Monitor sender in background
timeout $DURATION python3 -u - "$PORT1" "Sender" > "$OUTPUT_SENDER" 2>&1 &
PID_SENDER=$!

# Monitor receiver in foreground
timeout $DURATION python3 -u - "$PORT2" "Receiver" > "$OUTPUT_RECEIVER" 2>&1 <<'PYTHON_SCRIPT'
import serial
import time
import sys

port = sys.argv[1]
name = sys.argv[2]

try:
    ser = serial.Serial(port, 9600, timeout=1)
    time.sleep(2)

    start_time = time.time()
    line_count = 0
    rx_count = 0
    tx_count = 0

    print(f"=== {name} Monitor Started ===\n")

    while time.time() - start_time < 60:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                timestamp = time.strftime('[%H:%M:%S]')
                print(f"{timestamp} {line}")
                sys.stdout.flush()
                line_count += 1
                if "Received" in line or "Receive #" in line:
                    rx_count += 1
                if "Transmit" in line:
                    tx_count += 1

    elapsed = time.time() - start_time
    print(f"\n=== {name} Test Complete ===")
    print(f"Duration: {elapsed:.1f}s")
    print(f"Lines: {line_count}")
    print(f"RX events: {rx_count}")
    print(f"TX events: {tx_count}")

    ser.close()

except Exception as e:
    print(f"Error: {e}")
PYTHON_SCRIPT

# Wait for sender monitor to finish
wait $PID_SENDER

# Analyze results
echo ""
echo -e "${BLUE}=== Test 4 Results ===${NC}"
echo ""

echo "Sender output:"
tail -n 5 "$OUTPUT_SENDER"
echo ""

echo "Receiver output:"
tail -n 5 "$OUTPUT_RECEIVER"
echo ""

# Count successes
SENDER_TX=$(grep -c "Transmitted" "$OUTPUT_SENDER" || echo "0")
RECEIVER_RX=$(grep -c "Received\|Receive #" "$OUTPUT_RECEIVER" || echo "0")

echo "Summary:"
echo "  Packets Transmitted: $SENDER_TX"
echo "  Packets Received: $RECEIVER_RX"

if [ "$SENDER_TX" -gt 5 ] && [ "$RECEIVER_RX" -gt 5 ]; then
    echo -e "  ${GREEN}✓ SUCCESS: TX and RX both working (bug fix successful!)${NC}"
else
    echo -e "  ${RED}✗ FAIL: Limited or no communication${NC}"
fi

echo ""
echo "Full outputs saved to:"
echo "  Sender: $OUTPUT_SENDER"
echo "  Receiver: $OUTPUT_RECEIVER"
