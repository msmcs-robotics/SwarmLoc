#!/bin/bash
# Helper script for uploading both firmwares using cable swap method
# Use this when /dev/ttyACM1 has upload issues

cd "$(dirname "$0")"

echo "==================================================================="
echo "DUAL ARDUINO UPLOAD - Cable Swap Method"
echo "==================================================================="
echo ""
echo "This script helps upload firmware to both Arduinos when only"
echo "one USB port is working reliably (/dev/ttyACM0)"
echo ""
echo "Expected distance between devices: 45.72 cm (18 inches)"
echo "==================================================================="

# Step 1: Upload ANCHOR
echo ""
echo "───────────────────────────────────────────────────────────────────"
echo "STEP 1: Upload ANCHOR Firmware"
echo "───────────────────────────────────────────────────────────────────"
echo ""
echo "Instructions:"
echo "  1. Ensure ONE Arduino is connected to /dev/ttyACM0"
echo "  2. This will become the ANCHOR device"
echo ""
read -p "Press Enter when Arduino is connected and ready..."

# Set ANCHOR mode
echo "Setting firmware to ANCHOR mode..."
sed -i 's/#define IS_ANCHOR false/#define IS_ANCHOR true/' src/main.cpp

echo "Uploading ANCHOR firmware..."
pio run --target upload --upload-port /dev/ttyACM0

if [ $? -eq 0 ]; then
    echo "✅ ANCHOR upload successful!"
else
    echo "❌ ANCHOR upload failed!"
    exit 1
fi

# Wait for device to settle
sleep 2

# Step 2: Upload TAG
echo ""
echo "───────────────────────────────────────────────────────────────────"
echo "STEP 2: Upload TAG Firmware"
echo "───────────────────────────────────────────────────────────────────"
echo ""
echo "Instructions:"
echo "  1. UNPLUG the ANCHOR Arduino from /dev/ttyACM0"
echo "  2. PLUG IN the SECOND Arduino to /dev/ttyACM0"
echo "  3. This will become the TAG device"
echo ""
read -p "Press Enter when second Arduino is connected..."

# Set TAG mode
echo "Setting firmware to TAG mode..."
sed -i 's/#define IS_ANCHOR true/#define IS_ANCHOR false/' src/main.cpp

echo "Uploading TAG firmware..."
pio run --target upload --upload-port /dev/ttyACM0

if [ $? -eq 0 ]; then
    echo "✅ TAG upload successful!"
else
    echo "❌ TAG upload failed!"
    exit 1
fi

# Summary
echo ""
echo "==================================================================="
echo "✅ BOTH DEVICES SUCCESSFULLY PROGRAMMED!"
echo "==================================================================="
echo ""
echo "Device Configuration:"
echo "  - ANCHOR: First Arduino uploaded"
echo "  - TAG:    Second Arduino uploaded"
echo ""
echo "Next Steps:"
echo "  1. Connect BOTH Arduinos to USB (any ports for power/serial)"
echo "     - ANCHOR can be on /dev/ttyACM0, /dev/ttyACM1, or /dev/ttyUSB*"
echo "     - TAG can be on /dev/ttyACM0, /dev/ttyACM1, or /dev/ttyUSB*"
echo ""
echo "  2. Position devices 45.72 cm (18 inches) apart"
echo ""
echo "  3. Open TWO serial monitors (115200 baud):"
echo "     Terminal 1: pio device monitor --port /dev/ttyACM0"
echo "     Terminal 2: pio device monitor --port /dev/ttyACM1"
echo ""
echo "  4. Both devices will display:"
echo "     '>>> Send any character to start ranging <<<'"
echo ""
echo "  5. In EACH serial monitor, type any character and press Enter"
echo ""
echo "  6. Ranging will begin! Watch for [RANGE] messages with distance"
echo ""
echo "🎯 Expected readings: ~45-46 cm (before calibration)"
echo "==================================================================="
