#!/bin/bash
#
# DWM3000 Test Runner
#
# Compiles and uploads test sketches to Arduino Uno
# Usage: ./run_test.sh <test_number> <port>
#
# Examples:
#   ./run_test.sh 1 /dev/ttyACM0    # Run Test 1 on first Arduino
#   ./run_test.sh 1                 # Run Test 1 on auto-detected port
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  DWM3000 Test Runner${NC}"
echo -e "${GREEN}========================================${NC}"
echo

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <test_number> [port]"
    echo
    echo "Available tests:"
    echo "  1 - Chip ID Read (SPI communication)"
    echo "  2 - GPIO and Reset"
    echo "  3 - Simple Transmit"
    echo "  4 - Simple Receive"
    echo "  5 - Ping-Pong"
    echo
    echo "Examples:"
    echo "  $0 1                    # Auto-detect port"
    echo "  $0 1 /dev/ttyACM0       # Specific port"
    exit 1
fi

TEST_NUM=$1
PORT=$2

# Test directory mapping
declare -A TEST_DIRS
TEST_DIRS[1]="test_01_chip_id"
TEST_DIRS[2]="test_02_gpio_reset"
TEST_DIRS[3]="test_03_simple_tx"
TEST_DIRS[4]="test_04_simple_rx"
TEST_DIRS[5]="test_05_ping_pong"

# Get test directory
TEST_DIR="${TEST_DIRS[$TEST_NUM]}"

if [ -z "$TEST_DIR" ]; then
    echo -e "${RED}Error: Invalid test number: $TEST_NUM${NC}"
    exit 1
fi

TEST_PATH="$SCRIPT_DIR/$TEST_DIR"

if [ ! -d "$TEST_PATH" ]; then
    echo -e "${RED}Error: Test directory not found: $TEST_PATH${NC}"
    exit 1
fi

echo "Test: $TEST_DIR"
echo "Path: $TEST_PATH"
echo

# Auto-detect port if not specified
if [ -z "$PORT" ]; then
    echo "Auto-detecting Arduino port..."
    PORTS=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1)
    if [ -z "$PORTS" ]; then
        echo -e "${RED}Error: No Arduino boards detected${NC}"
        echo "Please connect Arduino and try again"
        exit 1
    fi
    PORT=$PORTS
    echo -e "${GREEN}Found: $PORT${NC}"
fi

echo "Upload port: $PORT"
echo

# Create temporary PlatformIO project for the test
TEMP_DIR="/tmp/dwm3000_test_$TEST_NUM"
echo "Creating temporary PlatformIO project..."
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/src"

# Copy test file to src directory (rename to main.cpp)
echo "Copying test sketch..."
cp "$TEST_PATH"/*.ino "$TEMP_DIR/src/main.cpp"

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
monitor_filters = default, time
lib_deps = SPI
EOF

echo "Configuration created"
echo

# Compile
echo -e "${YELLOW}Compiling...${NC}"
cd "$TEMP_DIR"
pio run

if [ $? -ne 0 ]; then
    echo -e "${RED}Compilation failed!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Compilation successful${NC}"
echo

# Upload
echo -e "${YELLOW}Uploading to $PORT...${NC}"
pio run -t upload

if [ $? -ne 0 ]; then
    echo -e "${RED}Upload failed!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Upload successful${NC}"
echo

# Ask if user wants to monitor
echo -e "${YELLOW}========================================${NC}"
echo "Upload complete!"
echo -e "${YELLOW}========================================${NC}"
echo
read -p "Open serial monitor? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Starting serial monitor (Ctrl+C to exit)..."
    echo "----------------------------------------"
    pio device monitor
fi

echo
echo -e "${GREEN}Test $TEST_NUM complete${NC}"
