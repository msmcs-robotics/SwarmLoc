#!/bin/bash
# Capture serial output from an ESP32 (no upload).
# Resets the chip via RTS so you see the boot banner.
#
# Usage:
#   scripts/monitor.sh [PORT] [DURATION_SEC] [--no-reset]
#
# Defaults: PORT=/dev/ttyUSB0, DURATION_SEC=30

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
exec "$PROJECT_DIR/scripts/capture_serial.py" "${@:-/dev/ttyUSB0 30}"
