#!/bin/bash
# DWS1000_UWB development workflow tool
#
# Usage:
#   ./dev.sh build [ENV]          Build firmware (default: uno_live)
#   ./dev.sh flash PORT [ENV]     Build + flash to device
#   ./dev.sh monitor PORT         Serial monitor
#   ./dev.sh go PORT [ENV]        Build + flash + monitor
#   ./dev.sh flash-both [ENV]     Flash both devices (ACM0=anchor, ACM1=tag)
#   ./dev.sh calibrate            Flash calibration firmware to both devices
#   ./dev.sh envs                 List available PIO environments
#   ./dev.sh ports                List connected serial ports

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Default values
DEFAULT_ENV="uno_tag"
DEFAULT_BAUD=115200
PORT_ANCHOR="/dev/ttyACM0"
PORT_TAG="/dev/ttyACM1"

usage() {
    echo "DWS1000_UWB Development Tool"
    echo ""
    echo "Usage: $(basename "$0") <command> [options]"
    echo ""
    echo "Commands:"
    echo "  build [ENV]          Build firmware (default: $DEFAULT_ENV)"
    echo "  flash PORT [ENV]     Build + flash to PORT"
    echo "  monitor PORT         Open serial monitor"
    echo "  go PORT [ENV]        Build + flash + monitor"
    echo "  flash-both [ENV]     Flash anchor to ACM0 + tag to ACM1"
    echo "  calibrate            Flash calibration firmware to both"
    echo "  envs                 List PIO environments"
    echo "  ports                List connected serial ports"
    echo ""
    echo "Environments:"
    echo "  uno_anchor           Anchor/responder (flash to ACM0)"
    echo "  uno_tag              Tag/initiator (flash to ACM1, default)"
    echo "  uno_calibration      Antenna delay calibration + OLED"
    echo "  uno_ng               DW1000-ng base (manual test files)"
    echo "  uno                  Legacy thotro library (deprecated)"
}

cmd_build() {
    local env="${1:-$DEFAULT_ENV}"
    echo "Building environment: $env"
    cd "$PROJECT_DIR"
    pio run -e "$env"
}

cmd_flash() {
    local port="$1"
    local env="${2:-$DEFAULT_ENV}"
    if [ -z "$port" ]; then
        echo "Error: port required. Usage: dev.sh flash /dev/ttyACM0 [ENV]"
        exit 1
    fi
    echo "Flashing $env to $port"
    cd "$PROJECT_DIR"
    pio run -e "$env" -t upload --upload-port "$port"
}

cmd_monitor() {
    local port="${1:-$PORT_ANCHOR}"
    echo "Monitoring $port at $DEFAULT_BAUD baud (Ctrl+C to exit)"
    python3 "$SCRIPT_DIR/serial_monitor.py" "$port" --baud "$DEFAULT_BAUD"
}

cmd_go() {
    local port="$1"
    local env="${2:-$DEFAULT_ENV}"
    if [ -z "$port" ]; then
        echo "Error: port required. Usage: dev.sh go /dev/ttyACM0 [ENV]"
        exit 1
    fi
    cmd_flash "$port" "$env"
    sleep 1
    cmd_monitor "$port"
}

cmd_flash_both() {
    echo "Flashing anchor + tag to both devices..."
    echo "  Anchor: $PORT_ANCHOR (uno_anchor)"
    echo "  Tag:    $PORT_TAG (uno_tag)"
    cd "$PROJECT_DIR"
    pio run -e uno_anchor -t upload --upload-port "$PORT_ANCHOR"
    pio run -e uno_tag -t upload --upload-port "$PORT_TAG"
    echo "Both devices flashed."
}

cmd_calibrate() {
    echo "Flashing calibration firmware to both devices..."
    echo "  Anchor: $PORT_ANCHOR (test_twr_anchor.cpp with CALIBRATION_MODE)"
    echo "  Tag:    $PORT_TAG (test_calibration_tag.cpp)"
    cd "$PROJECT_DIR"

    # Flash anchor first, then calibration tag
    echo ""
    echo "--- Flashing anchor ---"
    pio run -e uno_calibration -t upload --upload-port "$PORT_ANCHOR"

    echo ""
    echo "--- Flashing calibration tag ---"
    pio run -e uno_calibration -t upload --upload-port "$PORT_TAG"

    echo ""
    echo "Both devices flashed with calibration firmware."
    echo "Monitor the tag to see calibration progress:"
    echo "  python3 tools/serial_monitor.py $PORT_TAG"
}

cmd_envs() {
    cd "$PROJECT_DIR"
    echo "Available PIO environments:"
    grep '^\[env:' platformio.ini | sed 's/\[env:\(.*\)\]/  \1/'
}

cmd_ports() {
    echo "Connected serial ports:"
    ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "  (none found)"
}

# Parse command
case "${1:-}" in
    build)      cmd_build "$2" ;;
    flash)      cmd_flash "$2" "$3" ;;
    monitor)    cmd_monitor "$2" ;;
    go)         cmd_go "$2" "$3" ;;
    flash-both) cmd_flash_both "$2" ;;
    calibrate)  cmd_calibrate ;;
    envs)       cmd_envs ;;
    ports)      cmd_ports ;;
    -h|--help|help|"")  usage ;;
    *)
        echo "Unknown command: $1"
        usage
        exit 1
        ;;
esac
