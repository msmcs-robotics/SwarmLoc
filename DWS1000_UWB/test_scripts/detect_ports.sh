#!/bin/bash

# DWM3000 UWB - Arduino Port Detection Script
# Detects connected Arduino boards and identifies which port is which

echo "=== Arduino Port Detection ==="
echo ""

# Find all USB serial devices
PORTS=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | sort)

if [ -z "$PORTS" ]; then
    echo "❌ No Arduino boards detected!"
    echo "   Please connect your Arduino Uno boards via USB"
    exit 1
fi

echo "Found USB serial devices:"
for port in $PORTS; do
    echo "  📍 $port"
done
echo ""

# Count devices
NUM_PORTS=$(echo "$PORTS" | wc -l)

if [ "$NUM_PORTS" -lt 2 ]; then
    echo "⚠️  Warning: Only $NUM_PORTS Arduino detected"
    echo "   This project requires 2 Arduino Uno boards"
    echo "   Please connect both devices"
    exit 1
fi

if [ "$NUM_PORTS" -eq 2 ]; then
    echo "✓ Detected 2 Arduino boards (correct setup)"
    PORT_ARRAY=($PORTS)
    INITIATOR_PORT="${PORT_ARRAY[0]}"
    RESPONDER_PORT="${PORT_ARRAY[1]}"

    echo ""
    echo "Suggested port assignment:"
    echo "  Initiator  → $INITIATOR_PORT"
    echo "  Responder  → $RESPONDER_PORT"
    echo ""
    echo "To update platformio.ini automatically:"
    echo "  ./test_scripts/update_ports.sh $INITIATOR_PORT $RESPONDER_PORT"
else
    echo "⚠️  Warning: Detected $NUM_PORTS USB serial devices"
    echo "   Expected 2 Arduino boards, found $NUM_PORTS"
    echo "   Additional devices may be other USB serial adapters"
fi

echo ""
echo "=== Device Information ==="

for port in $PORTS; do
    echo ""
    echo "Port: $port"

    # Try to get device info using udevadm (requires sudo for some systems)
    if command -v udevadm &> /dev/null; then
        INFO=$(udevadm info -q all -n $port 2>/dev/null | grep -E "ID_VENDOR=|ID_MODEL=|ID_SERIAL=" | head -3)
        if [ ! -z "$INFO" ]; then
            echo "$INFO" | sed 's/^/  /'
        else
            echo "  (Device info not available)"
        fi
    fi

    # Check if port is accessible
    if [ -r "$port" ] && [ -w "$port" ]; then
        echo "  ✓ Port is readable and writable"
    else
        echo "  ❌ Permission denied - you may need to add yourself to 'dialout' group:"
        echo "     sudo usermod -a -G dialout $USER"
        echo "     (then logout and login again)"
    fi
done

echo ""
echo "=== Next Steps ==="
echo "1. Verify which Arduino is initiator and which is responder"
echo "2. Update platformio.ini with correct ports if needed"
echo "3. Run: pio run -e initiator -t upload"
echo "4. Run: pio run -e responder -t upload"
echo ""
