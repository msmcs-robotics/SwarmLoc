#!/bin/bash
#
# Test 3-4: DW1000 Library TX/RX Test
#
# Tests BasicSender and BasicReceiver examples
# Verifies transmit and receive functionality
#
# Usage:
#   ./run_tx_rx_test.sh           # Auto-detect both ports
#   ./run_tx_rx_test.sh /dev/ttyACM0 /dev/ttyACM1  # Specify ports
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Test 3-4: DW1000 TX/RX Test${NC}"
echo -e "${GREEN}========================================${NC}"
echo

# Auto-detect ports if not specified
PORT1=$1
PORT2=$2

if [ -z "$PORT1" ] || [ -z "$PORT2" ]; then
    echo "Auto-detecting Arduino ports..."
    PORTS=($(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null))

    if [ ${#PORTS[@]} -lt 2 ]; then
        echo -e "${RED}Error: Need 2 Arduino boards, found ${#PORTS[@]}${NC}"
        echo "Please connect both Arduinos and try again"
        exit 1
    fi

    PORT1=${PORTS[0]}
    PORT2=${PORTS[1]}
    echo -e "${GREEN}Found: $PORT1 and $PORT2${NC}"
fi

echo "Port 1 (Sender):   $PORT1"
echo "Port 2 (Receiver): $PORT2"
echo

# Function to compile and upload a test
compile_and_upload() {
    local TEST_NAME=$1
    local TEST_FILE=$2
    local PORT=$3

    echo -e "${YELLOW}=== $TEST_NAME ===${NC}"
    echo

    # Create temporary PlatformIO project
    TEMP_DIR="/tmp/dwm3000_${TEST_NAME}"
    echo "Creating temporary PlatformIO project..."
    rm -rf "$TEMP_DIR"
    mkdir -p "$TEMP_DIR/src"
    mkdir -p "$TEMP_DIR/lib"

    # Copy test file
    echo "Copying test sketch..."
    cp "$TEST_FILE" "$TEMP_DIR/src/main.cpp"

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
    echo -e "${YELLOW}Compiling $TEST_NAME...${NC}"
    cd "$TEMP_DIR"
    pio run -s  # Silent mode for cleaner output

    if [ $? -ne 0 ]; then
        echo -e "${RED}Compilation failed!${NC}"
        exit 1
    fi

    echo -e "${GREEN}✓ Compilation successful${NC}"
    echo

    # Upload
    echo -e "${YELLOW}Uploading to $PORT...${NC}"
    pio run -t upload -s

    if [ $? -ne 0 ]; then
        echo -e "${RED}Upload failed!${NC}"
        exit 1
    fi

    echo -e "${GREEN}✓ Upload successful${NC}"
    echo
}

# Compile and upload both tests
compile_and_upload "Sender" "$SCRIPT_DIR/test_03_sender.ino" "$PORT1"
compile_and_upload "Receiver" "$SCRIPT_DIR/test_04_receiver.ino" "$PORT2"

# Summary
echo -e "${GREEN}========================================${NC}"
echo "Both tests uploaded successfully!"
echo -e "${GREEN}========================================${NC}"
echo
echo "Test Configuration:"
echo "  Sender:   $PORT1"
echo "  Receiver: $PORT2"
echo
echo "Expected Behavior:"
echo "  1. Sender transmits packets every second"
echo "  2. Receiver displays received packets"
echo "  3. Packet count should increment on both"
echo
echo -e "${YELLOW}Next steps:${NC}"
echo "  1. Monitor sender:   pio device monitor -p $PORT1 -b 9600"
echo "  2. Monitor receiver: pio device monitor -p $PORT2 -b 9600"
echo "  3. Or monitor both in separate terminals"
echo
echo "To monitor both at once (requires tmux):"
echo "  ./monitor_both_serial.sh $PORT1 $PORT2"
echo

read -p "Create dual monitor script? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    cat > "$SCRIPT_DIR/monitor_both_serial.sh" << 'EOFSCRIPT'
#!/bin/bash
# Monitor both serial ports in tmux

PORT1=$1
PORT2=$2

if [ -z "$PORT1" ] || [ -z "$PORT2" ]; then
    echo "Usage: $0 <port1> <port2>"
    exit 1
fi

# Check if tmux is installed
if ! command -v tmux &> /dev/null; then
    echo "Error: tmux is not installed"
    echo "Install: sudo apt-get install tmux"
    exit 1
fi

# Create new tmux session
SESSION="uwb_test"

# Kill old session if exists
tmux kill-session -t $SESSION 2>/dev/null

# Create new session with first pane
tmux new-session -d -s $SESSION

# Set pane titles
tmux rename-window -t $SESSION:0 'UWB TX/RX Test'

# Split window vertically
tmux split-window -v -t $SESSION:0

# Monitor sender in top pane
tmux send-keys -t $SESSION:0.0 "echo 'Sender ($PORT1):'; pio device monitor -p $PORT1 -b 9600" C-m

# Monitor receiver in bottom pane
tmux send-keys -t $SESSION:0.1 "echo 'Receiver ($PORT2):'; pio device monitor -p $PORT2 -b 9600" C-m

# Attach to session
tmux attach-session -t $SESSION
EOFSCRIPT

    chmod +x "$SCRIPT_DIR/monitor_both_serial.sh"
    echo -e "${GREEN}✓ Created monitor_both_serial.sh${NC}"
    echo

    read -p "Start dual monitor now? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Starting dual serial monitor..."
        echo "Press Ctrl+B then D to detach (leave running)"
        echo "Press Ctrl+C to exit both monitors"
        sleep 2
        "$SCRIPT_DIR/monitor_both_serial.sh" "$PORT1" "$PORT2"
    fi
fi

echo
echo -e "${GREEN}Test 3-4 complete${NC}"
