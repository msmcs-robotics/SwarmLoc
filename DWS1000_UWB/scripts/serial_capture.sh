#!/bin/bash
# Serial capture script - captures output to file, then exits cleanly
# Usage: ./serial_capture.sh /dev/ttyACM0 /tmp/output.txt [duration_seconds]

PORT="${1:-/dev/ttyACM0}"
OUTPUT="${2:-/tmp/serial_out.txt}"
DURATION="${3:-30}"

stty -F "$PORT" 115200 raw -echo 2>/dev/null

# Capture with timeout, then exit
timeout "$DURATION" cat "$PORT" > "$OUTPUT" 2>/dev/null

echo "Captured to $OUTPUT"
