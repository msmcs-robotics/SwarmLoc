#!/bin/bash

# Upload sender only

set -e

PROJECT_DIR="/home/devel/Desktop/SwarmLoc/DWS1000_UWB"
TEST_DIR="${PROJECT_DIR}/tests/test_05_pingpong"

cd "$PROJECT_DIR"

echo "Uploading SENDER to /dev/ttyACM0..."

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
upload_speed = 115200
lib_deps =
    SPI
lib_extra_dirs = lib
build_flags =
    -I lib/DW1000/src
    -std=gnu++11
EOF

mkdir -p src
cp "${TEST_DIR}/test_05_sender.ino" src/main.cpp

echo "Compiling..."
pio run

echo "Uploading..."
pio run --target upload

echo "Done!"
