#!/bin/bash
# DWS1000_UWB Test Utilities
# Comprehensive testing utilities for UWB development

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Function: Detect Arduino ports
detect_ports() {
    echo -e "${CYAN}Detecting Arduino ports...${NC}"
    PORTS=($(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo ""))

    if [ ${#PORTS[@]} -eq 0 ]; then
        echo -e "${RED}No Arduino ports detected!${NC}"
        return 1
    fi

    echo -e "${GREEN}Found ${#PORTS[@]} port(s):${NC}"
    for i in "${!PORTS[@]}"; do
        echo "  [$i] ${PORTS[$i]}"
    done

    return 0
}

# Function: Monitor serial port
monitor_serial() {
    local PORT=$1
    local BAUD=${2:-115200}
    local DURATION=${3:-60}
    local OUTPUT_FILE=${4:-/tmp/serial_output.txt}

    echo -e "${CYAN}Monitoring $PORT at $BAUD baud for ${DURATION}s...${NC}"

    python3 -c "
import serial
import time
import sys

try:
    ser = serial.Serial('$PORT', $BAUD, timeout=1)
    print(f'=== Monitoring $PORT @ $BAUD baud for ${DURATION}s ===', file=sys.stderr)

    start = time.time()
    while (time.time() - start) < $DURATION:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)

    ser.close()
    print(f'=== Monitoring complete ===', file=sys.stderr)
except Exception as e:
    print(f'Error: {e}', file=sys.stderr)
    sys.exit(1)
" > "$OUTPUT_FILE" 2>&1

    echo -e "${GREEN}Output saved to: $OUTPUT_FILE${NC}"
}

# Function: Monitor two ports simultaneously
monitor_dual() {
    local PORT1=$1
    local PORT2=$2
    local BAUD=${3:-115200}
    local DURATION=${4:-60}

    echo -e "${CYAN}Monitoring both ports for ${DURATION}s...${NC}"

    local OUT1="/tmp/anchor_$(date +%Y%m%d_%H%M%S).txt"
    local OUT2="/tmp/tag_$(date +%Y%m%d_%H%M%S).txt"

    monitor_serial "$PORT1" "$BAUD" "$DURATION" "$OUT1" &
    PID1=$!

    monitor_serial "$PORT2" "$BAUD" "$DURATION" "$OUT2" &
    PID2=$!

    wait $PID1
    wait $PID2

    echo
    echo -e "${YELLOW}=== PORT 1: $PORT1 ===${NC}"
    cat "$OUT1"
    echo
    echo -e "${YELLOW}=== PORT 2: $PORT2 ===${NC}"
    cat "$OUT2"
}

# Function: Compile PlatformIO project
compile_pio() {
    local PROJECT_DIR=$1

    echo -e "${CYAN}Compiling PlatformIO project: $PROJECT_DIR${NC}"
    cd "$PROJECT_DIR"

    if pio run -s; then
        echo -e "${GREEN}✓ Compilation successful${NC}"
        return 0
    else
        echo -e "${RED}✗ Compilation failed${NC}"
        return 1
    fi
}

# Function: Upload to Arduino
upload_pio() {
    local PROJECT_DIR=$1
    local PORT=$2

    echo -e "${CYAN}Uploading to $PORT...${NC}"
    cd "$PROJECT_DIR"

    if pio run -t upload --upload-port "$PORT" -s; then
        echo -e "${GREEN}✓ Upload successful${NC}"
        return 0
    else
        echo -e "${RED}✗ Upload failed${NC}"
        return 1
    fi
}

# Function: Run test suite
run_test() {
    local TEST_NAME=$1
    local TEST_DIR="$PROJECT_ROOT/tests/$TEST_NAME"

    if [ ! -d "$TEST_DIR" ]; then
        echo -e "${RED}Test directory not found: $TEST_DIR${NC}"
        return 1
    fi

    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  Running Test: $TEST_NAME${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo

    # Check if test has a run script
    if [ -f "$TEST_DIR/run_test.sh" ]; then
        bash "$TEST_DIR/run_test.sh"
    else
        echo -e "${YELLOW}No run_test.sh found, using default procedure${NC}"
        compile_pio "$TEST_DIR"
    fi
}

# Function: List all tests
list_tests() {
    echo -e "${CYAN}Available tests:${NC}"
    for test_dir in "$PROJECT_ROOT/tests"/test_*; do
        if [ -d "$test_dir" ]; then
            local test_name=$(basename "$test_dir")
            echo "  - $test_name"
        fi
    done
}

# Function: Clean build artifacts
clean_builds() {
    echo -e "${CYAN}Cleaning build artifacts...${NC}"
    find "$PROJECT_ROOT" -type d -name ".pio" -exec rm -rf {} + 2>/dev/null || true
    find "$PROJECT_ROOT" -type f -name "*.hex" -delete 2>/dev/null || true
    find "$PROJECT_ROOT" -type f -name "*.elf" -delete 2>/dev/null || true
    echo -e "${GREEN}✓ Cleanup complete${NC}"
}

# Function: Create test report
create_report() {
    local TEST_NAME=$1
    local OUTPUT_FILE="$PROJECT_ROOT/docs/findings/TEST_RESULTS.md"

    echo -e "${CYAN}Creating test report for: $TEST_NAME${NC}"
    echo -e "${GREEN}Report will be appended to: $OUTPUT_FILE${NC}"
}

# Function: Show help
show_help() {
    cat << EOF
${GREEN}DWS1000_UWB Test Utilities${NC}

Usage: $0 [command] [options]

Commands:
  detect                    Detect connected Arduino ports
  monitor PORT [BAUD]       Monitor single serial port (default 115200 baud)
  dual PORT1 PORT2 [BAUD]   Monitor two ports simultaneously
  compile TEST_NAME         Compile a test
  upload TEST_NAME PORT     Compile and upload a test
  run TEST_NAME             Run complete test suite
  list                      List all available tests
  clean                     Clean build artifacts
  report TEST_NAME          Create test report
  help                      Show this help message

Examples:
  $0 detect
  $0 monitor /dev/ttyACM0 115200
  $0 dual /dev/ttyACM0 /dev/ttyACM1
  $0 run test_06_ranging
  $0 list
  $0 clean

EOF
}

# Main command handler
main() {
    case "${1:-help}" in
        detect)
            detect_ports
            ;;
        monitor)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: PORT required${NC}"
                echo "Usage: $0 monitor PORT [BAUD] [DURATION]"
                exit 1
            fi
            monitor_serial "$2" "${3:-115200}" "${4:-60}"
            ;;
        dual)
            if [ -z "$2" ] || [ -z "$3" ]; then
                echo -e "${RED}Error: Two PORTs required${NC}"
                echo "Usage: $0 dual PORT1 PORT2 [BAUD] [DURATION]"
                exit 1
            fi
            monitor_dual "$2" "$3" "${4:-115200}" "${5:-60}"
            ;;
        compile)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: TEST_NAME required${NC}"
                exit 1
            fi
            compile_pio "$PROJECT_ROOT/tests/$2"
            ;;
        upload)
            if [ -z "$2" ] || [ -z "$3" ]; then
                echo -e "${RED}Error: TEST_NAME and PORT required${NC}"
                exit 1
            fi
            compile_pio "$PROJECT_ROOT/tests/$2"
            upload_pio "$PROJECT_ROOT/tests/$2" "$3"
            ;;
        run)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: TEST_NAME required${NC}"
                exit 1
            fi
            run_test "$2"
            ;;
        list)
            list_tests
            ;;
        clean)
            clean_builds
            ;;
        report)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: TEST_NAME required${NC}"
                exit 1
            fi
            create_report "$2"
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            echo -e "${RED}Unknown command: $1${NC}"
            echo
            show_help
            exit 1
            ;;
    esac
}

# Run main
main "$@"
