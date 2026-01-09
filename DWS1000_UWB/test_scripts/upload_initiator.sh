#!/bin/bash

# Upload firmware to Initiator Arduino

echo "=== Uploading Initiator Firmware ==="
echo ""

cd "$(dirname "$0")/.." || exit 1

echo "Building and uploading to initiator..."
pio run -e initiator -t upload

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Initiator firmware uploaded successfully!"
    echo ""
    echo "To monitor serial output:"
    echo "  pio device monitor -e initiator"
    echo ""
    echo "Or use the combined monitor:"
    echo "  ./test_scripts/monitor_both.sh"
else
    echo ""
    echo "❌ Upload failed!"
    echo "Check that Arduino is connected to the correct port"
    exit 1
fi
