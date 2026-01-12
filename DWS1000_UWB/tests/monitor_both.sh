#!/bin/bash
# Monitor both Arduino devices simultaneously
# TAG on ACM1, ANCHOR on ACM0

OUTPUT_FILE="/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_outputs/ranging_verification_2026-01-11.txt"

echo "==================================================================" > "$OUTPUT_FILE"
echo "DW1000 Ranging Test - Hardware Verification" >> "$OUTPUT_FILE"
echo "Date: $(date)" >> "$OUTPUT_FILE"
echo "Test Duration: 180 seconds (3 minutes)" >> "$OUTPUT_FILE"
echo "==================================================================" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "Starting monitors for both devices..." >> "$OUTPUT_FILE"
echo "TAG: /dev/ttyACM1" >> "$OUTPUT_FILE"
echo "ANCHOR: /dev/ttyACM0" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Function to monitor a device
monitor_device() {
    local PORT=$1
    local NAME=$2
    local DURATION=$3

    echo "[$NAME] Starting monitor on $PORT" >> "$OUTPUT_FILE"
    timeout $DURATION stty -F $PORT 115200 raw -echo
    timeout $DURATION cat $PORT | while IFS= read -r line; do
        echo "$(date +%H:%M:%S) [$NAME] $line" >> "$OUTPUT_FILE"
    done &
}

# Monitor both devices
monitor_device "/dev/ttyACM1" "TAG" "180"
monitor_device "/dev/ttyACM0" "ANCHOR" "180"

# Wait for both to finish
wait

echo "" >> "$OUTPUT_FILE"
echo "==================================================================" >> "$OUTPUT_FILE"
echo "Test completed at: $(date)" >> "$OUTPUT_FILE"
echo "==================================================================" >> "$OUTPUT_FILE"

echo "Monitoring complete. Output saved to: $OUTPUT_FILE"
