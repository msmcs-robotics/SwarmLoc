#!/bin/bash
# capture_only.sh - Capture serial output WITHOUT uploading (device already programmed)
# Resets the Arduino via DTR to catch boot output.
#
# Usage:
#   ./scripts/capture_only.sh PORT [DURATION] [OUTPUT_FILE] [WAIT_FOR]
#
# Examples:
#   ./scripts/capture_only.sh /dev/ttyACM0 30
#   ./scripts/capture_only.sh /dev/ttyACM1 60 /tmp/rx_output.txt "Test complete"

set -euo pipefail

PORT="${1:?Usage: $0 PORT [DURATION] [OUTPUT_FILE] [WAIT_FOR]}"
DURATION="${2:-30}"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
PORT_NAME="$(basename "$PORT")"
OUTPUT="${3:-/tmp/serial_${PORT_NAME}_${TIMESTAMP}.txt}"
WAIT_FOR="${4:-}"

echo "=== Capture Only (no upload) ==="
echo "Port:     $PORT"
echo "Duration: ${DURATION}s"
echo "Output:   $OUTPUT"
[ -n "$WAIT_FOR" ] && echo "Wait for: \"$WAIT_FOR\""
echo ""

python3 -c "
import serial
import time
import sys

port = '$PORT'
duration = $DURATION
output_file = '$OUTPUT'
wait_for = '$WAIT_FOR'

try:
    ser = serial.Serial(port, 115200, timeout=1)
    # Reset via DTR to catch boot output
    ser.dtr = False
    time.sleep(0.1)
    ser.dtr = True
    time.sleep(2)

    lines = []
    start = time.time()
    no_data_count = 0

    with open(output_file, 'w') as f:
        f.write(f'=== Serial Capture: {port} @ {time.strftime(\"%Y-%m-%d %H:%M:%S\")} ===\n')
        f.write(f'=== Duration: {duration}s max ===\n\n')
        f.flush()

        while (time.time() - start) < duration:
            try:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    lines.append(line)
                    f.write(line + '\n')
                    f.flush()
                    print(line)
                    no_data_count = 0

                    if wait_for and wait_for in line:
                        f.write(f'\n=== Found \"{wait_for}\" - stopping ===\n')
                        print(f'[Found \"{wait_for}\" - stopping]')
                        break
                else:
                    no_data_count += 1
                    if lines and no_data_count > 10:
                        f.write(f'\n=== No data for 10s - stopping ===\n')
                        print('[No data for 10s - stopping]')
                        break
            except Exception as e:
                f.write(f'[Read error: {e}]\n')
                break

        elapsed = time.time() - start
        f.write(f'\n=== Captured {len(lines)} lines in {elapsed:.1f}s ===\n')

    ser.close()
    print(f'\n[Captured {len(lines)} lines to {output_file}]')

except Exception as e:
    print(f'Error: {e}', file=sys.stderr)
    with open(output_file, 'w') as f:
        f.write(f'ERROR: {e}\n')
    sys.exit(1)
"

echo ""
echo "=== Done. Output in: $OUTPUT ==="
