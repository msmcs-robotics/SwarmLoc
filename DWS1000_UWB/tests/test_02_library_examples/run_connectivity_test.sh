#!/bin/bash
#
# Test 2: DW1000 Library Connectivity Test
#
# Tests the arduino-dw1000 library BasicConnectivityTest example
# Verifies library can communicate with DW1000 chip
#
# Expected results:
#   - Device ID should show DW1000 information
#   - Unique ID should be displayed
#   - Network ID and device address should match configuration
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Test 2: DW1000 Library Connectivity${NC}"
echo -e "${GREEN}========================================${NC}"
echo

# Auto-detect port if not specified
PORT=$1
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

# Create temporary PlatformIO project
TEMP_DIR="/tmp/dwm3000_test_02_connectivity"
echo "Creating temporary PlatformIO project..."
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/src"
mkdir -p "$TEMP_DIR/lib"

# Copy test file to src directory
echo "Copying test sketch..."
cp "$SCRIPT_DIR/test_02_connectivity.ino" "$TEMP_DIR/src/main.cpp"

# Create symlink to DW1000 library
echo "Linking DW1000 library..."
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
monitor_filters = default, time
lib_deps =
    SPI
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
echo "Expected output:"
echo "  - 'DW1000 initialized ...'"
echo "  - 'Committed configuration ...'"
echo "  - Device ID: DW1000 [chip info]"
echo "  - Unique ID: [64-bit ID]"
echo "  - Network ID & Device Address: PAN: 0x000A, Short Address: 0x0005"
echo "  - Device mode: [mode info]"
echo
echo "Output should repeat every 10 seconds"
echo
read -p "Open serial monitor? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Starting serial monitor (Ctrl+C to exit)..."
    echo "----------------------------------------"
    pio device monitor
fi

echo
echo -e "${GREEN}Test 2 complete${NC}"
echo
echo "Next steps:"
echo "  1. Verify Device ID shows 'DW1000'"
echo "  2. Note the Unique ID for your module"
echo "  3. Verify Network ID (0x000A) and Address (0x0005)"
echo "  4. Run Test 3: BasicSender/Receiver"
