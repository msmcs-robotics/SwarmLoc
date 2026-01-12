#!/bin/bash
# Compilation and upload script for Test 07: Low-Level Ranging Examples

set -e  # Exit on error

echo "========================================================================"
echo "Test 07: Low-Level Ranging (RangingTag and RangingAnchor)"
echo "========================================================================"
echo ""
echo "This test uses the original DW1000 library examples for two-way ranging."
echo ""
echo "Configuration:"
echo "  - TAG:    Device Address 2, Network ID 10"
echo "  - ANCHOR: Device Address 1, Network ID 10"
echo "  - Mode:   MODE_LONGDATA_RANGE_LOWPOWER"
echo "  - Reply Delay: 3000 microseconds"
echo "  - Algorithm: Asymmetric two-way ranging"
echo ""
echo "========================================================================"

# Change to test directory
cd "$(dirname "$0")"

echo ""
echo "Step 1: Compile TAG"
echo "--------------------------------------------------------------------"
pio run -e tag

if [ $? -ne 0 ]; then
    echo "ERROR: TAG compilation failed!"
    exit 1
fi

echo ""
echo "Step 2: Compile ANCHOR"
echo "--------------------------------------------------------------------"
pio run -e anchor

if [ $? -ne 0 ]; then
    echo "ERROR: ANCHOR compilation failed!"
    exit 1
fi

echo ""
echo "========================================================================"
echo "Compilation successful!"
echo "========================================================================"
echo ""
echo "Step 3: Upload firmware"
echo "--------------------------------------------------------------------"
echo ""
echo "Available devices:"
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "No devices found on /dev/ttyACM* or /dev/ttyUSB*"
echo ""

read -p "Enter TAG port (default: /dev/ttyACM0): " TAG_PORT
TAG_PORT=${TAG_PORT:-/dev/ttyACM0}

read -p "Enter ANCHOR port (default: /dev/ttyACM1): " ANCHOR_PORT
ANCHOR_PORT=${ANCHOR_PORT:-/dev/ttyACM1}

echo ""
echo "Uploading TAG to $TAG_PORT..."
pio run -e tag --target upload --upload-port $TAG_PORT

if [ $? -ne 0 ]; then
    echo "ERROR: TAG upload failed!"
    exit 1
fi

echo ""
echo "Uploading ANCHOR to $ANCHOR_PORT..."
pio run -e anchor --target upload --upload-port $ANCHOR_PORT

if [ $? -ne 0 ]; then
    echo "ERROR: ANCHOR upload failed!"
    exit 1
fi

echo ""
echo "========================================================================"
echo "Upload successful!"
echo "========================================================================"
echo ""
echo "Step 4: Monitor serial output"
echo "--------------------------------------------------------------------"
echo ""
echo "The ANCHOR will display ranging results on its serial output."
echo "The TAG will poll for ranges continuously."
echo ""
echo "Expected output from ANCHOR:"
echo "  Range: X.XX m    RX power: -XX.XX dBm    Sampling: X.XX Hz"
echo ""
echo "To monitor:"
echo "  Terminal 1 (TAG):    pio device monitor -e tag"
echo "  Terminal 2 (ANCHOR): pio device monitor -e anchor"
echo ""
echo "Or use screen/minicom:"
echo "  screen $TAG_PORT 115200"
echo "  screen $ANCHOR_PORT 115200"
echo ""
read -p "Press Enter to start monitoring ANCHOR (Ctrl+C to exit)..."

pio device monitor -e anchor
