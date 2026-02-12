#!/bin/bash
# upload_dual_and_capture.sh - Upload different firmware to two devices, then capture both
# Useful for TX/RX testing where each device needs different code.
#
# Usage:
#   ./scripts/upload_dual_and_capture.sh SRC1 PORT1 SRC2 PORT2 [DURATION]
#
# Examples:
#   ./scripts/upload_dual_and_capture.sh tests/test_tx.cpp /dev/ttyACM0 tests/test_rx.cpp /dev/ttyACM1 30
#
# Arguments:
#   SRC1     - Source file for device 1 (copied to src/main.cpp before upload)
#   PORT1    - Serial port for device 1
#   SRC2     - Source file for device 2
#   PORT2    - Serial port for device 2
#   DURATION - Capture duration in seconds (default: 30)

set -euo pipefail

SRC1="${1:?Usage: $0 SRC1 PORT1 SRC2 PORT2 [DURATION]}"
PORT1="${2:?Usage: $0 SRC1 PORT1 SRC2 PORT2 [DURATION]}"
SRC2="${3:?Usage: $0 SRC1 PORT1 SRC2 PORT2 [DURATION]}"
PORT2="${4:?Usage: $0 SRC1 PORT1 SRC2 PORT2 [DURATION]}"
DURATION="${5:-30}"

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
OUT1="/tmp/dev1_$(basename $PORT1)_${TIMESTAMP}.txt"
OUT2="/tmp/dev2_$(basename $PORT2)_${TIMESTAMP}.txt"
MAIN_CPP="$PROJECT_DIR/src/main.cpp"
MAIN_BACKUP="$PROJECT_DIR/src/main.cpp.dual_backup"

echo "=== Dual Upload and Capture ==="
echo "Device 1: $SRC1 -> $PORT1"
echo "Device 2: $SRC2 -> $PORT2"
echo "Duration: ${DURATION}s"
echo ""

# Backup main.cpp
cp "$MAIN_CPP" "$MAIN_BACKUP"

# Step 1: Upload to device 1
echo "--- Step 1: Upload to Device 1 ($PORT1) ---"
cp "$PROJECT_DIR/$SRC1" "$MAIN_CPP"
cd "$PROJECT_DIR"
if ! pio run 2>&1 | tail -3; then
    cp "$MAIN_BACKUP" "$MAIN_CPP"
    echo "COMPILE FAILED for $SRC1"
    exit 1
fi
if ! pio run -t upload --upload-port "$PORT1" 2>&1 | tail -3; then
    cp "$MAIN_BACKUP" "$MAIN_CPP"
    echo "UPLOAD FAILED to $PORT1"
    exit 1
fi
echo "Device 1 uploaded."
echo ""

# Step 2: Upload to device 2
echo "--- Step 2: Upload to Device 2 ($PORT2) ---"
cp "$PROJECT_DIR/$SRC2" "$MAIN_CPP"
if ! pio run 2>&1 | tail -3; then
    cp "$MAIN_BACKUP" "$MAIN_CPP"
    echo "COMPILE FAILED for $SRC2"
    exit 1
fi
if ! pio run -t upload --upload-port "$PORT2" 2>&1 | tail -3; then
    cp "$MAIN_BACKUP" "$MAIN_CPP"
    echo "UPLOAD FAILED to $PORT2"
    exit 1
fi
echo "Device 2 uploaded."
echo ""

# Restore main.cpp
cp "$MAIN_BACKUP" "$MAIN_CPP"

# Step 3: Capture both simultaneously
echo "--- Step 3: Capturing both (${DURATION}s) ---"
bash "$PROJECT_DIR/scripts/capture_dual.sh" "$PORT1" "$PORT2" "$DURATION" "dual"

echo ""
echo "=== Complete ==="
