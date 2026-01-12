#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Test 6: DW1000 Ranging Test${NC}"
echo -e "${GREEN}========================================${NC}"
echo

# Auto-detect Arduino ports or use provided ones
PORT1=${1:-$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1)}
PORT2=${2:-$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | tail -1)}

if [ -z "$PORT1" ] || [ -z "$PORT2" ]; then
    echo -e "${RED}Error: Could not detect two Arduino ports${NC}"
    echo "Usage: $0 [ANCHOR_PORT] [TAG_PORT]"
    echo "Example: $0 /dev/ttyACM0 /dev/ttyACM1"
    exit 1
fi

echo "Port 1 (Anchor):   $PORT1"
echo "Port 2 (Tag):      $PORT2"
echo

compile_and_upload() {
    local TEST_NAME=$1
    local TEST_FILE=$2
    local PORT=$3

    echo -e "${YELLOW}=== $TEST_NAME ===${NC}"
    echo

    # Create temporary PlatformIO project
    TEMP_DIR="/tmp/dwm3000_$TEST_NAME"
    echo "Creating temporary PlatformIO project..."
    rm -rf "$TEMP_DIR"
    mkdir -p "$TEMP_DIR/src"
    mkdir -p "$TEMP_DIR/lib"

    # Copy test sketch
    echo "Copying test sketch..."
    cp "$TEST_FILE" "$TEMP_DIR/src/main.cpp"

    # Link DW1000 library
    echo "Linking DW1000 library..."
    ln -s "$PROJECT_ROOT/lib/DW1000" "$TEMP_DIR/lib/DW1000"

    # Create platformio.ini
    cat > "$TEMP_DIR/platformio.ini" <<EOF
[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = $PORT
monitor_port = $PORT
monitor_speed = 115200
lib_deps =
    SPI
EOF
    echo "Configuration created"
    echo

    # Compile
    echo -e "${YELLOW}Compiling $TEST_NAME...${NC}"
    cd "$TEMP_DIR"
    if pio run -s; then
        echo -e "${GREEN}✓ Compilation successful${NC}"
    else
        echo -e "${RED}✗ Compilation failed${NC}"
        return 1
    fi

    echo
    echo -e "${YELLOW}Uploading to $PORT...${NC}"
    if pio run -t upload -s; then
        echo -e "${GREEN}✓ Upload successful${NC}"
    else
        echo -e "${RED}✗ Upload failed${NC}"
        return 1
    fi
    echo
}

# Compile and upload anchor
compile_and_upload "Anchor" "$SCRIPT_DIR/test_06_anchor.ino" "$PORT1"

# Compile and upload tag
compile_and_upload "Tag" "$SCRIPT_DIR/test_06_tag.ino" "$PORT2"

echo -e "${GREEN}========================================${NC}"
echo "Both tests uploaded successfully!"
echo -e "${GREEN}========================================${NC}"
echo
echo "Test Configuration:"
echo "  Anchor: $PORT1"
echo "  Tag:    $PORT2"
echo
echo "Expected Behavior:"
echo "  1. Anchor starts and waits for tag"
echo "  2. Tag initiates ranging with anchor"
echo "  3. Distance measurements displayed on tag serial"
echo "  4. Both show RX power and ranging info"
echo
echo -e "${YELLOW}Next steps:${NC}"
echo "  1. Monitor anchor:  python3 /tmp/read_serial.py $PORT1 60"
echo "  2. Monitor tag:     python3 /tmp/read_serial.py $PORT2 60"
echo
echo "To monitor both, run in separate terminals or use:"
echo "  python3 /tmp/read_serial.py $PORT1 60 > anchor.log &"
echo "  python3 /tmp/read_serial.py $PORT2 60 > tag.log &"
echo "  wait"
echo "  cat anchor.log"
echo "  cat tag.log"
