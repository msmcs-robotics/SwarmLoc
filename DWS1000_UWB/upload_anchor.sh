#!/bin/bash
# Upload ANCHOR firmware to ACM0

echo "Building ANCHOR firmware..."
cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB

# Switch to ANCHOR mode
sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR true/' src/main.cpp

# Build and upload
echo "Uploading to /dev/ttyACM0 (ANCHOR)..."
pio run -t upload --upload-port /dev/ttyACM0

echo "ANCHOR upload complete"
