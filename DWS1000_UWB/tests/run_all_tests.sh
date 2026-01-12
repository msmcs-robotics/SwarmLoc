#!/bin/bash
# Comprehensive Test Suite - Post Bug Fix
# Tests all examples to verify DW1000.cpp interrupt bug fix works
# Bug fixed: interruptOnReceiveFailed() - changed LEN_SYS_STATUS to LEN_SYS_MASK (4 lines)

set -e

PROJECT_ROOT="/home/devel/Desktop/SwarmLoc/DWS1000_UWB"
PLATFORMIO_INI="$PROJECT_ROOT/platformio.ini"
TEST_OUTPUT_DIR="$PROJECT_ROOT/tests/test_outputs_$(date +%Y%m%d_%H%M%S)"

# ANSI color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Create output directory
mkdir -p "$TEST_OUTPUT_DIR"

echo -e "${BLUE}=================================================================${NC}"
echo -e "${BLUE}DW1000 COMPREHENSIVE TEST SUITE - POST BUG FIX${NC}"
echo -e "${BLUE}=================================================================${NC}"
echo ""
echo "Bug Fixed: interruptOnReceiveFailed() in DW1000.cpp"
echo "  - Changed LEN_SYS_STATUS to LEN_SYS_MASK (4 lines, 992-996)"
echo ""
echo "Test Output Directory: $TEST_OUTPUT_DIR"
echo ""

# Helper function to compile and upload
compile_and_upload() {
    local test_file=$1
    local port=$2
    local test_name=$3

    echo -e "${YELLOW}>>> Compiling $test_name...${NC}"

    # Create temporary platformio.ini with test file
    cat > "$PLATFORMIO_INI" << EOF
[env:uno]
platform = atmelavr
board = uno
framework = arduino
lib_extra_dirs = lib
src_dir = $test_file
monitor_speed = 115200
upload_port = $port
EOF

    # Compile
    if ! pio run -c "$PLATFORMIO_INI"; then
        echo -e "${RED}FAILED: Compilation failed for $test_name${NC}"
        return 1
    fi

    # Upload
    echo -e "${YELLOW}>>> Uploading to $port...${NC}"
    if ! pio run -c "$PLATFORMIO_INI" --target upload; then
        echo -e "${RED}FAILED: Upload failed for $test_name to $port${NC}"
        return 1
    fi

    echo -e "${GREEN}SUCCESS: $test_name compiled and uploaded to $port${NC}"
    return 0
}

# Helper function to monitor serial output
monitor_serial() {
    local port=$1
    local duration=$2
    local output_file=$3
    local test_name=$4

    echo -e "${YELLOW}>>> Monitoring $test_name on $port for ${duration}s...${NC}"

    timeout ${duration}s python3 - <<PYTHON_SCRIPT > "$output_file" 2>&1 || true
import serial
import time
import sys

try:
    ser = serial.Serial('$port', 115200, timeout=1)
    time.sleep(2)  # Wait for Arduino reset

    start_time = time.time()
    line_count = 0

    while time.time() - start_time < $duration:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                timestamp = time.strftime('[%H:%M:%S]')
                print(f"{timestamp} {line}")
                sys.stdout.flush()
                line_count += 1

    print(f"\\n=== Test completed: {line_count} lines captured in ${duration}s ===")
    ser.close()

except Exception as e:
    print(f"Error: {e}")
    sys.exit(1)
PYTHON_SCRIPT

    local lines=$(wc -l < "$output_file")
    echo -e "${GREEN}Captured $lines lines from $test_name${NC}"
}

# ============================================================================
# TEST 3: BasicSender
# ============================================================================
echo -e "\n${BLUE}=================================================================${NC}"
echo -e "${BLUE}TEST 3: BasicSender${NC}"
echo -e "${BLUE}=================================================================${NC}"

if compile_and_upload "$PROJECT_ROOT/tests/test_02_library_examples" "/dev/ttyACM0" "BasicSender"; then
    sleep 3
    monitor_serial "/dev/ttyACM0" 60 "$TEST_OUTPUT_DIR/test03_sender_output.txt" "BasicSender"
    echo -e "${GREEN}TEST 3 COMPLETE${NC}"
else
    echo -e "${RED}TEST 3 FAILED${NC}"
fi

# ============================================================================
# TEST 4: BasicReceiver
# ============================================================================
echo -e "\n${BLUE}=================================================================${NC}"
echo -e "${BLUE}TEST 4: BasicReceiver${NC}"
echo -e "${BLUE}=================================================================${NC}"

# First upload sender again to prepare for paired test
compile_and_upload "$PROJECT_ROOT/tests/test_02_library_examples" "/dev/ttyACM0" "BasicSender (for Test 4)"

# Upload receiver
if compile_and_upload "$PROJECT_ROOT/tests/test_02_library_examples" "/dev/ttyACM1" "BasicReceiver"; then
    sleep 3

    # Monitor both simultaneously
    echo -e "${YELLOW}>>> Monitoring both Sender and Receiver for 60s...${NC}"

    # Start sender monitor in background
    (monitor_serial "/dev/ttyACM0" 60 "$TEST_OUTPUT_DIR/test04_sender_output.txt" "Sender") &
    SENDER_PID=$!

    # Start receiver monitor
    monitor_serial "/dev/ttyACM1" 60 "$TEST_OUTPUT_DIR/test04_receiver_output.txt" "Receiver"

    wait $SENDER_PID

    echo -e "${GREEN}TEST 4 COMPLETE${NC}"
else
    echo -e "${RED}TEST 4 FAILED${NC}"
fi

# ============================================================================
# TEST 6: DW1000Ranging TAG/ANCHOR
# ============================================================================
echo -e "\n${BLUE}=================================================================${NC}"
echo -e "${BLUE}TEST 6: DW1000Ranging TAG/ANCHOR${NC}"
echo -e "${BLUE}=================================================================${NC}"

# Create TAG version of test_clean.ino
TAG_DIR="$PROJECT_ROOT/tests/test_06_ranging/tag_temp"
mkdir -p "$TAG_DIR"
sed 's/#define IS_ANCHOR false/#define IS_ANCHOR false/' "$PROJECT_ROOT/tests/test_06_ranging/test_clean.ino" > "$TAG_DIR/main.cpp"

# Create ANCHOR version of test_clean.ino
ANCHOR_DIR="$PROJECT_ROOT/tests/test_06_ranging/anchor_temp"
mkdir -p "$ANCHOR_DIR"
sed 's/#define IS_ANCHOR false/#define IS_ANCHOR true/' "$PROJECT_ROOT/tests/test_06_ranging/test_clean.ino" > "$ANCHOR_DIR/main.cpp"

# Upload TAG
if compile_and_upload "$TAG_DIR" "/dev/ttyACM0" "TAG"; then
    sleep 2

    # Upload ANCHOR
    if compile_and_upload "$ANCHOR_DIR" "/dev/ttyACM1" "ANCHOR"; then
        sleep 3

        # Monitor both for 120 seconds
        echo -e "${YELLOW}>>> Monitoring TAG and ANCHOR for 120s...${NC}"

        # Start TAG monitor in background
        (monitor_serial "/dev/ttyACM0" 120 "$TEST_OUTPUT_DIR/test06_tag_output.txt" "TAG") &
        TAG_PID=$!

        # Start ANCHOR monitor
        monitor_serial "/dev/ttyACM1" 120 "$TEST_OUTPUT_DIR/test06_anchor_output.txt" "ANCHOR"

        wait $TAG_PID

        echo -e "${GREEN}TEST 6 COMPLETE${NC}"
    else
        echo -e "${RED}TEST 6 FAILED - ANCHOR upload failed${NC}"
    fi
else
    echo -e "${RED}TEST 6 FAILED - TAG upload failed${NC}"
fi

# Cleanup temp directories
rm -rf "$TAG_DIR" "$ANCHOR_DIR"

# ============================================================================
# TEST 5: MessagePingPong
# ============================================================================
echo -e "\n${BLUE}=================================================================${NC}"
echo -e "${BLUE}TEST 5: MessagePingPong${NC}"
echo -e "${BLUE}=================================================================${NC}"

if compile_and_upload "$PROJECT_ROOT/tests/test_05_pingpong" "/dev/ttyACM0" "PingPong_Sender"; then
    sleep 2

    if compile_and_upload "$PROJECT_ROOT/tests/test_05_pingpong" "/dev/ttyACM1" "PingPong_Receiver"; then
        sleep 3

        # Monitor both for 60 seconds
        echo -e "${YELLOW}>>> Monitoring PingPong for 60s...${NC}"

        # Start sender monitor in background
        (monitor_serial "/dev/ttyACM0" 60 "$TEST_OUTPUT_DIR/test05_sender_output.txt" "PingPong_Sender") &
        SENDER_PID=$!

        # Start receiver monitor
        monitor_serial "/dev/ttyACM1" 60 "$TEST_OUTPUT_DIR/test05_receiver_output.txt" "PingPong_Receiver"

        wait $SENDER_PID

        echo -e "${GREEN}TEST 5 COMPLETE${NC}"
    else
        echo -e "${RED}TEST 5 FAILED - Receiver upload failed${NC}"
    fi
else
    echo -e "${RED}TEST 5 FAILED - Sender upload failed${NC}"
fi

# ============================================================================
# GENERATE SUMMARY
# ============================================================================
echo -e "\n${BLUE}=================================================================${NC}"
echo -e "${BLUE}TEST SUMMARY${NC}"
echo -e "${BLUE}=================================================================${NC}"

cat > "$TEST_OUTPUT_DIR/SUMMARY.md" << 'SUMMARY'
# Test Results Summary - Post Bug Fix

## Bug Fix Applied
**File**: DW1000.cpp
**Function**: interruptOnReceiveFailed()
**Lines**: 992-996
**Change**: LEN_SYS_STATUS → LEN_SYS_MASK (4 instances)

## Tests Executed

### Test 3: BasicSender
**Status**: [To be filled]
**File**: test03_sender_output.txt
**Observations**:
- [ ] Device initialized
- [ ] Started transmitting
- [ ] Packets sent continuously
- [ ] No errors

### Test 4: BasicReceiver + BasicSender
**Status**: [To be filled]
**Files**: test04_sender_output.txt, test04_receiver_output.txt
**Observations**:
- [ ] Both devices initialized
- [ ] Sender transmitting
- [ ] Receiver receiving packets
- [ ] Packet count matches

### Test 6: DW1000Ranging TAG/ANCHOR
**Status**: [To be filled]
**Files**: test06_tag_output.txt, test06_anchor_output.txt
**Observations**:
- [ ] Both devices initialized
- [ ] Device discovery ("Device found" messages)
- [ ] Range measurements appearing
- [ ] Distance values reasonable

### Test 5: MessagePingPong
**Status**: [To be filled]
**Files**: test05_sender_output.txt, test05_receiver_output.txt
**Observations**:
- [ ] Bidirectional communication
- [ ] Ping-pong exchange working
- [ ] No communication gaps

## Before/After Comparison

### Before Bug Fix
- ❌ Interrupts not firing
- ❌ No RX callbacks
- ❌ No TX completion callbacks
- ❌ Devices hung after initialization
- ❌ No ranging measurements

### After Bug Fix
- [ ] Interrupts firing correctly
- [ ] RX callbacks working
- [ ] TX callbacks working
- [ ] Normal operation
- [ ] Ranging measurements successful

## Conclusion
[To be filled after analyzing all outputs]
SUMMARY

echo -e "${GREEN}All tests completed!${NC}"
echo ""
echo "Test outputs saved to: $TEST_OUTPUT_DIR"
echo ""
echo "Files created:"
ls -lh "$TEST_OUTPUT_DIR"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Review output files in $TEST_OUTPUT_DIR"
echo "2. Fill in SUMMARY.md with observations"
echo "3. Update TEST_RESULTS.md with comprehensive results"
echo ""
