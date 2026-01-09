#!/bin/bash

# Automated ranging test with data collection

echo "=== UWB Ranging Test ==="
echo ""

cd "$(dirname "$0")/.." || exit 1

# Test parameters
DURATION=${1:-60}  # Default 60 seconds
OUTPUT_DIR="test_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_FILE="$OUTPUT_DIR/ranging_test_$TIMESTAMP.log"

mkdir -p "$OUTPUT_DIR"

echo "Test Configuration:"
echo "  Duration: $DURATION seconds"
echo "  Output: $OUTPUT_FILE"
echo ""

# Check if devices are connected
./test_scripts/detect_ports.sh > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "❌ Arduino boards not detected"
    exit 1
fi

echo "Starting test in 3 seconds..."
sleep 3

echo ""
echo "=== Test Started at $(date) ===" | tee "$OUTPUT_FILE"
echo "" | tee -a "$OUTPUT_FILE"

# Monitor initiator output for ranging data
echo "Collecting ranging data for $DURATION seconds..."
echo "(Press Ctrl+C to stop early)" echo ""

timeout $DURATION pio device monitor -e initiator 2>&1 | tee -a "$OUTPUT_FILE"

echo "" | tee -a "$OUTPUT_FILE"
echo "=== Test Ended at $(date) ===" | tee -a "$OUTPUT_FILE"
echo "" | tee -a "$OUTPUT_FILE"

# Simple analysis
echo "=== Basic Analysis ===" | tee -a "$OUTPUT_FILE"

DISTANCE_COUNT=$(grep -c "Distance:" "$OUTPUT_FILE" 2>/dev/null || echo "0")
ERROR_COUNT=$(grep -c "Error" "$OUTPUT_FILE" 2>/dev/null || echo "0")

echo "  Distance measurements: $DISTANCE_COUNT" | tee -a "$OUTPUT_FILE"
echo "  Errors: $ERROR_COUNT" | tee -a "$OUTPUT_FILE"

if [ "$DISTANCE_COUNT" -gt 0 ]; then
    echo "" | tee -a "$OUTPUT_FILE"
    echo "Sample distance readings:" | tee -a "$OUTPUT_FILE"
    grep "Distance:" "$OUTPUT_FILE" | tail -10 | sed 's/^/  /' | tee -a "$OUTPUT_FILE"
fi

echo "" | tee -a "$OUTPUT_FILE"
echo "Full results saved to: $OUTPUT_FILE"
echo ""
echo "To analyze results:"
echo "  cat $OUTPUT_FILE"
echo "  grep 'Distance:' $OUTPUT_FILE | less"
echo ""
