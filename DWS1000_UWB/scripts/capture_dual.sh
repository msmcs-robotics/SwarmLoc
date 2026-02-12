#!/bin/bash
# capture_dual.sh - Capture from TWO serial ports simultaneously
# Each port gets its own output file. Both are reset and captured in parallel.
#
# Usage:
#   ./scripts/capture_dual.sh PORT1 PORT2 [DURATION] [PREFIX]
#
# Examples:
#   ./scripts/capture_dual.sh /dev/ttyACM0 /dev/ttyACM1 30
#   ./scripts/capture_dual.sh /dev/ttyACM0 /dev/ttyACM1 60 tx_rx_test

set -euo pipefail

PORT1="${1:?Usage: $0 PORT1 PORT2 [DURATION] [PREFIX]}"
PORT2="${2:?Usage: $0 PORT1 PORT2 [DURATION] [PREFIX]}"
DURATION="${3:-30}"
PREFIX="${4:-dual}"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"

OUT1="/tmp/${PREFIX}_$(basename $PORT1)_${TIMESTAMP}.txt"
OUT2="/tmp/${PREFIX}_$(basename $PORT2)_${TIMESTAMP}.txt"

echo "=== Dual Serial Capture ==="
echo "Port 1:   $PORT1 -> $OUT1"
echo "Port 2:   $PORT2 -> $OUT2"
echo "Duration: ${DURATION}s"
echo ""

# Capture function using Python for reliability
capture_port() {
    local PORT="$1"
    local OUTPUT="$2"
    local DUR="$3"

    python3 -c "
import serial
import time

port = '$PORT'
duration = $DUR
output_file = '$OUTPUT'

try:
    ser = serial.Serial(port, 115200, timeout=1)
    ser.dtr = False
    time.sleep(0.1)
    ser.dtr = True
    time.sleep(2)

    lines = []
    start = time.time()
    no_data_count = 0

    with open(output_file, 'w') as f:
        f.write(f'=== {port} @ {time.strftime(\"%Y-%m-%d %H:%M:%S\")} ===\n\n')
        f.flush()

        while (time.time() - start) < duration:
            try:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    lines.append(line)
                    f.write(line + '\n')
                    f.flush()
                    no_data_count = 0
                else:
                    no_data_count += 1
                    if lines and no_data_count > 10:
                        break
            except:
                break

        elapsed = time.time() - start
        f.write(f'\n=== {len(lines)} lines in {elapsed:.1f}s ===\n')

    ser.close()
except Exception as e:
    with open(output_file, 'w') as f:
        f.write(f'ERROR: {e}\n')
"
}

# Run both captures in parallel
capture_port "$PORT1" "$OUT1" "$DURATION" &
PID1=$!

capture_port "$PORT2" "$OUT2" "$DURATION" &
PID2=$!

echo "Capturing... (PIDs: $PID1, $PID2)"

# Wait for both
wait $PID1 2>/dev/null
wait $PID2 2>/dev/null

echo ""
echo "=== Capture Complete ==="
echo ""
echo "--- PORT 1: $PORT1 ---"
cat "$OUT1"
echo ""
echo "--- PORT 2: $PORT2 ---"
cat "$OUT2"
echo ""
echo "Output files:"
echo "  $OUT1"
echo "  $OUT2"
