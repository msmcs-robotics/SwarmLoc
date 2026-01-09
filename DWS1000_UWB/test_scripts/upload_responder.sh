#!/bin/bash

# Upload firmware to Responder Arduino

echo "=== Uploading Responder Firmware ==="
echo ""

cd "$(dirname "$0")/.." || exit 1

echo "Building and uploading to responder..."
pio run -e responder -t upload

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Responder firmware uploaded successfully!"
    echo ""
    echo "To monitor serial output:"
    echo "  pio device monitor -e responder"
    echo ""
    echo "Or use the combined monitor:"
    echo "  ./test_scripts/monitor_both.sh"
else
    echo ""
    echo "❌ Upload failed!"
    echo "Check that Arduino is connected to the correct port"
    exit 1
fi
