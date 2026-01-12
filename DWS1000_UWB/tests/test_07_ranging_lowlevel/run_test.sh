#!/bin/bash
# Test runner for low-level ranging examples (RangingTag and RangingAnchor)

echo "================================================"
echo "Test 07: Low-Level Ranging (Tag/Anchor)"
echo "================================================"
echo ""
echo "Configuration:"
echo "  - Tag: Device Address 2, Network ID 10"
echo "  - Anchor: Device Address 1, Network ID 10"
echo "  - Mode: MODE_LONGDATA_RANGE_LOWPOWER"
echo "  - Reply Delay: 3000 microseconds"
echo "  - Protocol: Two-way ranging with asymmetric computation"
echo ""
echo "================================================"
echo "Step 1: Compile and upload TAG (test_07a_tag.ino)"
echo "================================================"
read -p "Press Enter to compile TAG..."

arduino-cli compile --fqbn arduino:avr:nano tests/test_07_ranging_lowlevel/test_07a_tag.ino

if [ $? -ne 0 ]; then
    echo "ERROR: Tag compilation failed!"
    exit 1
fi

echo ""
read -p "Connect TAG Arduino and press Enter to upload..."
read -p "Enter TAG Arduino port (e.g., /dev/ttyUSB0): " TAG_PORT

arduino-cli upload -p $TAG_PORT --fqbn arduino:avr:nano tests/test_07_ranging_lowlevel/test_07a_tag.ino

if [ $? -ne 0 ]; then
    echo "ERROR: Tag upload failed!"
    exit 1
fi

echo ""
echo "================================================"
echo "Step 2: Compile and upload ANCHOR (test_07b_anchor.ino)"
echo "================================================"
read -p "Press Enter to compile ANCHOR..."

arduino-cli compile --fqbn arduino:avr:nano tests/test_07_ranging_lowlevel/test_07b_anchor.ino

if [ $? -ne 0 ]; then
    echo "ERROR: Anchor compilation failed!"
    exit 1
fi

echo ""
read -p "Connect ANCHOR Arduino and press Enter to upload..."
read -p "Enter ANCHOR Arduino port (e.g., /dev/ttyUSB1): " ANCHOR_PORT

arduino-cli upload -p $ANCHOR_PORT --fqbn arduino:avr:nano tests/test_07_ranging_lowlevel/test_07b_anchor.ino

if [ $? -ne 0 ]; then
    echo "ERROR: Anchor upload failed!"
    exit 1
fi

echo ""
echo "================================================"
echo "Step 3: Monitor Serial Output"
echo "================================================"
echo "TAG on port: $TAG_PORT"
echo "ANCHOR on port: $ANCHOR_PORT"
echo ""
echo "Opening two serial monitors..."
echo "Monitor for 2-3 minutes to observe ranging behavior"
echo ""
echo "To monitor:"
echo "  Terminal 1: arduino-cli monitor -p $TAG_PORT -c baudrate=115200"
echo "  Terminal 2: arduino-cli monitor -p $ANCHOR_PORT -c baudrate=115200"
echo ""
read -p "Press Enter to start monitoring ANCHOR (this terminal will show ANCHOR output)..."

# Monitor the anchor (which shows the ranging results)
arduino-cli monitor -p $ANCHOR_PORT -c baudrate=115200
