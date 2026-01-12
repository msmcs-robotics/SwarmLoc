#!/bin/bash
# Monitor both UWB devices for ranging measurements
# Expected distance: 45.72 cm (18 inches)

echo "======================================"
echo "DW1000 Ranging Test Monitor"
echo "Expected distance: 45.72 cm (18 inches)"
echo "======================================"
echo ""
echo "ANCHOR output (ACM0) | TAG output (ACM1)"
echo "--------------------------------------"

# Create temp files for output
ANCHOR_LOG="/tmp/anchor_output.log"
TAG_LOG="/tmp/tag_output.log"

# Monitor both in background
(stty 115200 < /dev/ttyACM0; cat /dev/ttyACM0 | tee $ANCHOR_LOG | sed 's/^/[ANCHOR] /') &
ANCHOR_PID=$!

(stty 115200 < /dev/ttyACM1; cat /dev/ttyACM1 | tee $TAG_LOG | sed 's/^/[TAG] /') &
TAG_PID=$!

# Wait for Ctrl+C
echo "Press Ctrl+C to stop monitoring..."
trap "kill $ANCHOR_PID $TAG_PID 2>/dev/null; echo ''; echo 'Monitoring stopped.'; echo 'Logs saved to:'; echo '  ANCHOR: $ANCHOR_LOG'; echo '  TAG: $TAG_LOG'; exit 0" INT

wait
