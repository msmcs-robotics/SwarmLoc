#!/bin/bash

# Test 05: MessagePingPong - Simplified Test Runner
# Compiles and uploads both sender and receiver

set -e

PROJECT_DIR="/home/devel/Desktop/SwarmLoc/DWS1000_UWB"
TEST_DIR="${PROJECT_DIR}/tests/test_05_pingpong"

cd "$PROJECT_DIR"

echo "========================================="
echo "Test 05: MessagePingPong"
echo "Compiling and Uploading"
echo "========================================="
echo

# Step 1: Upload RECEIVER to /dev/ttyACM1
echo "Step 1: Compiling and uploading RECEIVER to /dev/ttyACM1..."

cat > platformio.ini << 'EOF'
[platformio]
default_envs = uno

[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM1
monitor_port = /dev/ttyACM1
monitor_speed = 9600
lib_deps =
    SPI
lib_extra_dirs = lib
build_flags =
    -I lib/DW1000/src
    -std=gnu++11
EOF

mkdir -p src
cp "${TEST_DIR}/test_05_receiver.ino" src/main.cpp

echo "Compiling receiver..."
pio run 2>&1 | tail -20

echo "Uploading receiver..."
pio run --target upload 2>&1 | tail -10

echo "✓ Receiver uploaded to /dev/ttyACM1"
echo

# Wait before next upload
sleep 2

# Step 2: Upload SENDER to /dev/ttyACM0
echo "Step 2: Compiling and uploading SENDER to /dev/ttyACM0..."

cat > platformio.ini << 'EOF'
[platformio]
default_envs = uno

[env:uno]
platform = atmelavr
board = uno
framework = arduino
upload_port = /dev/ttyACM0
monitor_port = /dev/ttyACM0
monitor_speed = 9600
lib_deps =
    SPI
lib_extra_dirs = lib
build_flags =
    -I lib/DW1000/src
    -std=gnu++11
EOF

cp "${TEST_DIR}/test_05_sender.ino" src/main.cpp

echo "Compiling sender..."
pio run 2>&1 | tail -20

echo "Uploading sender..."
pio run --target upload 2>&1 | tail -10

echo "✓ Sender uploaded to /dev/ttyACM0"
echo

echo "========================================="
echo "Upload Complete!"
echo "========================================="
echo
echo "Next step: Monitor both devices with:"
echo "  python3 ${TEST_DIR}/monitor_pingpong.py"
echo
echo "Or manually with:"
echo "  Terminal 1: pio device monitor -p /dev/ttyACM0 -b 9600"
echo "  Terminal 2: pio device monitor -p /dev/ttyACM1 -b 9600"
