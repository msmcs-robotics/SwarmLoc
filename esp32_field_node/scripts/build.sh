#!/bin/bash
# Build esp32_field_node firmware via PlatformIO.
# Wraps `pio run` so you don't need to remember the -d path.
#
# Usage:
#   scripts/build.sh                 # default: build all envs
#   scripts/build.sh -t clean        # clean build artifacts
#   scripts/build.sh --silent        # any extra args pass through

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
exec pio run -d "$PROJECT_DIR" "$@"
