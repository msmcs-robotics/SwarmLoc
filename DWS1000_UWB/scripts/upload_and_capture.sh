#!/bin/bash
# upload_and_capture.sh - Compile, upload, reset, capture serial output to a file
# Designed to be NON-BLOCKING friendly: outputs go to a file you can read later.
#
# Usage:
#   ./scripts/upload_and_capture.sh PORT [DURATION] [OUTPUT_FILE] [WAIT_FOR]
#
# Examples:
#   ./scripts/upload_and_capture.sh /dev/ttyACM0 30
#   ./scripts/upload_and_capture.sh /dev/ttyACM0 45 /tmp/test_output.txt
#   ./scripts/upload_and_capture.sh /dev/ttyACM0 60 /tmp/out.txt "Test complete"
#
# Arguments:
#   PORT       - Serial port (e.g. /dev/ttyACM0)
#   DURATION   - Max capture duration in seconds (default: 30)
#   OUTPUT     - Output file path (default: /tmp/serial_PORT_TIMESTAMP.txt)
#   WAIT_FOR   - Optional string to wait for (exits early when seen)

set -euo pipefail

PORT="${1:?Usage: $0 PORT [DURATION] [OUTPUT_FILE] [WAIT_FOR]}"
DURATION="${2:-30}"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
PORT_NAME="$(basename "$PORT")"
OUTPUT="${3:-/tmp/serial_${PORT_NAME}_${TIMESTAMP}.txt}"
WAIT_FOR="${4:-}"

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== Upload and Capture ==="
echo "Port:     $PORT"
echo "Duration: ${DURATION}s"
echo "Output:   $OUTPUT"
[ -n "$WAIT_FOR" ] && echo "Wait for: \"$WAIT_FOR\""
echo ""

# Step 1: Compile
echo "--- Compiling ---"
cd "$PROJECT_DIR"
if ! pio run 2>&1 | tail -5; then
    echo "COMPILE FAILED" | tee "$OUTPUT"
    exit 1
fi
echo ""

# Step 2: Upload
echo "--- Uploading to $PORT ---"
if ! pio run -t upload --upload-port "$PORT" 2>&1 | tail -5; then
    echo "UPLOAD FAILED" | tee "$OUTPUT"
    exit 1
fi
echo ""

# Step 3: Capture serial output
echo "--- Capturing serial output (${DURATION}s max) ---"

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
    time.sleep(2)  # Wait for Arduino bootloader

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
                    print(line)  # Also print to stdout
                    no_data_count = 0

                    # Early exit if we see the wait_for string
                    if wait_for and wait_for in line:
                        f.write(f'\n=== Found \"{wait_for}\" - stopping ===\n')
                        print(f'[Found \"{wait_for}\" - stopping]')
                        break
                else:
                    no_data_count += 1
                    # If we got some data then 10s of silence = done
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
