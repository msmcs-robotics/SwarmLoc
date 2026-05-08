#!/bin/bash
# Build, upload, then capture serial output for an ESP32 on PORT for SECS.
# Mirrors the helper pattern used in DWS1000_UWB/scripts/.
#
# Usage:
#   scripts/upload_and_capture.sh [PORT] [DURATION_SEC]
#
# Defaults: PORT=/dev/ttyUSB0, DURATION_SEC=15

set -euo pipefail

PORT="${1:-/dev/ttyUSB0}"
DURATION="${2:-15}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "[upload] building..."
pio run -d "$PROJECT_DIR"

echo "[upload] flashing $PORT..."
pio run -t upload --upload-port "$PORT" -d "$PROJECT_DIR"

echo "--- BEGIN SERIAL CAPTURE (${DURATION}s) ---"
exec "$PROJECT_DIR/scripts/capture_serial.py" "$PORT" "$DURATION"
