#!/bin/bash

# Test 05: MessagePingPong - Test Runner
# Date: 2026-01-11
#
# Purpose: Compile and upload sender/receiver, then monitor communication

set -e  # Exit on error

PROJECT_DIR="/home/devel/Desktop/SwarmLoc/DWS1000_UWB"
TEST_DIR="${PROJECT_DIR}/tests/test_05_pingpong"

# Device ports
SENDER_PORT="/dev/ttyACM0"
RECEIVER_PORT="/dev/ttyACM1"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test 05: MessagePingPong${NC}"
echo -e "${BLUE}Basic DW1000 Communication Test${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# Check if devices are connected
echo -e "${YELLOW}Checking devices...${NC}"
if [ ! -e "$SENDER_PORT" ]; then
    echo -e "${RED}ERROR: Sender device not found at $SENDER_PORT${NC}"
    exit 1
fi

if [ ! -e "$RECEIVER_PORT" ]; then
    echo -e "${RED}ERROR: Receiver device not found at $RECEIVER_PORT${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Sender found at $SENDER_PORT${NC}"
echo -e "${GREEN}✓ Receiver found at $RECEIVER_PORT${NC}"
echo

# Step 1: Compile and upload RECEIVER first
echo -e "${YELLOW}Step 1: Compiling and uploading RECEIVER...${NC}"
cd "$PROJECT_DIR"

# Create temporary platformio.ini for receiver
cat > platformio.ini << EOF
[platformio]
default_envs = uno

[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = ${RECEIVER_PORT}
monitor_port = ${RECEIVER_PORT}
monitor_speed = 9600
lib_deps =
    SPI
lib_extra_dirs = lib
build_flags =
    -I lib/DW1000/src
    -std=gnu++11
EOF

# Copy receiver code to main source
mkdir -p src
cp "${TEST_DIR}/test_05_receiver.ino" src/main.cpp

echo "Compiling receiver..."
pio run

echo "Uploading receiver to ${RECEIVER_PORT}..."
pio run --target upload

echo -e "${GREEN}✓ Receiver uploaded${NC}"
echo

# Step 2: Compile and upload SENDER
echo -e "${YELLOW}Step 2: Compiling and uploading SENDER...${NC}"

# Update platformio.ini for sender
cat > platformio.ini << EOF
[platformio]
default_envs = uno

[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = ${SENDER_PORT}
monitor_port = ${SENDER_PORT}
monitor_speed = 9600
lib_deps =
    SPI
lib_extra_dirs = lib
build_flags =
    -I lib/DW1000/src
    -std=gnu++11
EOF

# Copy sender code to main source
cp "${TEST_DIR}/test_05_sender.ino" src/main.cpp

echo "Compiling sender..."
pio run

echo "Uploading sender to ${SENDER_PORT}..."
pio run --target upload

echo -e "${GREEN}✓ Sender uploaded${NC}"
echo

# Step 3: Monitor both devices
echo -e "${YELLOW}Step 3: Monitoring communication...${NC}"
echo -e "${BLUE}Press Ctrl+C to stop monitoring${NC}"
echo
echo -e "${GREEN}Starting serial monitors in 3 seconds...${NC}"
sleep 3

# Use Python script for dual monitoring
if [ -f "${TEST_DIR}/monitor_pingpong.py" ]; then
    python3 "${TEST_DIR}/monitor_pingpong.py"
else
    echo -e "${RED}ERROR: monitor_pingpong.py not found${NC}"
    echo "Falling back to manual monitoring instructions..."
    echo
    echo "Open two terminals and run:"
    echo "  Terminal 1: pio device monitor -p ${SENDER_PORT} -b 9600"
    echo "  Terminal 2: pio device monitor -p ${RECEIVER_PORT} -b 9600"
fi

echo
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test 05 Complete${NC}"
echo -e "${BLUE}========================================${NC}"
