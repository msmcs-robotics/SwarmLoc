#!/bin/bash
# Upload TAG firmware to ACM1
# Handles serial interference from ANCHOR

echo "Building TAG firmware..."
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB

# Switch to TAG mode
sed -i 's/#define IS_ANCHOR true/#define IS_ANCHOR false/' src/main.cpp

# Build
pio run

# Try uploading with retries
echo "Uploading to /dev/ttyACM1 (TAG)..."
for i in {1..5}; do
    echo "Upload attempt $i..."
    timeout 30 pio run -t upload --upload-port /dev/ttyACM1 2>&1 | grep -E "SUCCESS|Error" && break
    sleep 2
done

echo "TAG upload complete"
