#!/bin/bash

# Upload firmware to both Arduino boards

echo "=== Uploading Firmware to Both Devices ==="
echo ""

cd "$(dirname "$0")/.." || exit 1

# Upload responder first (it goes into listening mode)
echo "Step 1/2: Uploading Responder..."
pio run -e responder -t upload

if [ $? -ne 0 ]; then
    echo "❌ Responder upload failed!"
    exit 1
fi

echo ""
echo "✓ Responder uploaded"
echo ""

# Small delay between uploads
sleep 1

# Upload initiator second (it starts sending ranging requests)
echo "Step 2/2: Uploading Initiator..."
pio run -e initiator -t upload

if [ $? -ne 0 ]; then
    echo "❌ Initiator upload failed!"
    exit 1
fi

echo ""
echo "✓ Initiator uploaded"
echo ""
echo "=== Both devices programmed successfully! ==="
echo ""
echo "Next steps:"
echo "  1. Monitor both serial outputs: ./test_scripts/monitor_both.sh"
echo "  2. Or monitor individually:"
echo "     - Initiator: pio device monitor -e initiator"
echo "     - Responder: pio device monitor -e responder"
echo ""
