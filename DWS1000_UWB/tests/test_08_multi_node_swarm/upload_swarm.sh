#!/bin/bash

################################################################################
# upload_swarm.sh - Multi-Node Swarm Firmware Upload Tool
#
# Project: SwarmLoc Multi-Node Swarm Test
# Purpose: Upload node_firmware.ino to multiple Arduino boards with different
#          NODE_ID configurations
#
# Usage:
#   ./upload_swarm.sh
#   ./upload_swarm.sh --auto    # Auto-detect and upload to all devices
#   ./upload_swarm.sh --help    # Show help
#
# Features:
# - Detects all connected Arduino devices
# - Prompts for node ID assignment (1-5)
# - Updates config.h with correct NODE_ID
# - Compiles and uploads firmware
# - Verifies upload success
# - Logs all operations
#
# Requirements:
# - PlatformIO CLI (pio)
# - Arduino Uno boards connected via USB
#
################################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOG_FILE="$SCRIPT_DIR/upload_log.txt"

# Configuration
MAX_NODES=5
MIN_NODES=3

################################################################################
# Functions
################################################################################

print_banner() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}SwarmLoc Multi-Node Upload Tool${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_FILE"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1" | tee -a "$LOG_FILE"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_FILE"
}

check_dependencies() {
    print_info "Checking dependencies..."

    # Check for PlatformIO
    if ! command -v pio &> /dev/null; then
        print_error "PlatformIO CLI not found. Please install it first:"
        echo "  curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/get-platformio.py | python3"
        exit 1
    fi

    # Check for sed
    if ! command -v sed &> /dev/null; then
        print_error "sed not found. Please install it first."
        exit 1
    fi

    print_success "All dependencies found"
    echo ""
}

detect_arduino_devices() {
    print_info "Detecting Arduino devices..."

    # Find all ttyACM and ttyUSB devices
    DEVICES=($(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true))

    if [ ${#DEVICES[@]} -eq 0 ]; then
        print_error "No Arduino devices found!"
        print_info "Please connect Arduino boards and try again."
        exit 1
    fi

    print_success "Found ${#DEVICES[@]} device(s):"
    for i in "${!DEVICES[@]}"; do
        echo "  [$i] ${DEVICES[$i]}"
    done
    echo ""
}

update_config_h() {
    local node_id=$1
    local config_file="$SCRIPT_DIR/config.h"

    print_info "Updating config.h with NODE_ID=$node_id..."

    # Backup original config
    cp "$config_file" "$config_file.backup"

    # Update NODE_ID in config.h
    sed -i "s/^#define NODE_ID .*/#define NODE_ID $node_id/" "$config_file"

    # Verify the change
    if grep -q "^#define NODE_ID $node_id" "$config_file"; then
        print_success "config.h updated successfully"
        return 0
    else
        print_error "Failed to update config.h"
        mv "$config_file.backup" "$config_file"
        return 1
    fi
}

compile_firmware() {
    print_info "Compiling firmware..."

    cd "$PROJECT_ROOT"

    # Compile using PlatformIO
    if pio run --environment uno --project-dir "$PROJECT_ROOT" > /dev/null 2>&1; then
        print_success "Firmware compiled successfully"
        return 0
    else
        print_error "Firmware compilation failed"
        pio run --environment uno --project-dir "$PROJECT_ROOT"
        return 1
    fi
}

upload_to_device() {
    local device=$1
    local node_id=$2

    print_info "Uploading to $device (Node $node_id)..."

    cd "$PROJECT_ROOT"

    # Update platformio.ini with correct upload port
    local platformio_ini="$PROJECT_ROOT/platformio.ini"
    cp "$platformio_ini" "$platformio_ini.backup"

    sed -i "s|^upload_port = .*|upload_port = $device|" "$platformio_ini"
    sed -i "s|^monitor_port = .*|monitor_port = $device|" "$platformio_ini"

    # Upload using PlatformIO
    if pio run --target upload --environment uno --project-dir "$PROJECT_ROOT"; then
        print_success "Upload to $device completed"
        mv "$platformio_ini.backup" "$platformio_ini"
        return 0
    else
        print_error "Upload to $device failed"
        mv "$platformio_ini.backup" "$platformio_ini"
        return 1
    fi
}

verify_upload() {
    local device=$1
    local node_id=$2

    print_info "Verifying upload on $device..."

    # Open serial port and read first few lines
    timeout 5 cat "$device" 2>/dev/null | head -n 20 > /tmp/verify_output.txt || true

    # Check if output contains node ID
    if grep -q "Node ID: $node_id" /tmp/verify_output.txt; then
        print_success "Node $node_id verified on $device"
        return 0
    else
        print_warning "Could not verify node ID on $device"
        print_info "Check serial monitor manually"
        return 1
    fi
}

interactive_upload() {
    print_banner
    check_dependencies
    detect_arduino_devices

    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Interactive Upload Mode${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo "This script will upload firmware to each device."
    echo "You will be prompted to assign a Node ID (1-$MAX_NODES) to each device."
    echo ""
    echo "Node 1 = Coordinator (anchor)"
    echo "Nodes 2-5 = Mobile tags"
    echo ""

    # Track uploaded nodes
    declare -a uploaded_nodes
    uploaded_count=0

    for device in "${DEVICES[@]}"; do
        echo ""
        echo -e "${YELLOW}========================================${NC}"
        echo -e "${YELLOW}Device: $device${NC}"
        echo -e "${YELLOW}========================================${NC}"

        # Prompt for node ID
        while true; do
            read -p "Assign Node ID (1-$MAX_NODES) or 's' to skip: " node_id

            if [ "$node_id" = "s" ] || [ "$node_id" = "S" ]; then
                print_info "Skipping $device"
                break
            fi

            # Validate node ID
            if ! [[ "$node_id" =~ ^[1-5]$ ]]; then
                print_error "Invalid node ID. Must be 1-$MAX_NODES."
                continue
            fi

            # Check if node ID already used
            if [[ " ${uploaded_nodes[@]} " =~ " ${node_id} " ]]; then
                print_error "Node ID $node_id already assigned!"
                continue
            fi

            # Update config.h
            if ! update_config_h "$node_id"; then
                print_error "Failed to update configuration"
                continue
            fi

            # Compile firmware
            if ! compile_firmware; then
                print_error "Compilation failed"
                continue
            fi

            # Upload to device
            if upload_to_device "$device" "$node_id"; then
                uploaded_nodes+=("$node_id")
                uploaded_count=$((uploaded_count + 1))
                print_success "Node $node_id configured on $device"

                # Optional verification
                read -p "Verify upload? (y/n): " verify_choice
                if [ "$verify_choice" = "y" ] || [ "$verify_choice" = "Y" ]; then
                    verify_upload "$device" "$node_id"
                fi
            else
                print_error "Upload failed for $device"
            fi

            break
        done
    done

    # Summary
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Upload Summary${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo "Uploaded nodes: ${uploaded_nodes[@]}"
    echo "Total: $uploaded_count devices"
    echo ""

    if [ $uploaded_count -ge $MIN_NODES ]; then
        print_success "Minimum node count ($MIN_NODES) met!"
        print_info "Your swarm is ready to test."
    else
        print_warning "Only $uploaded_count nodes uploaded. Minimum is $MIN_NODES."
    fi

    echo ""
    print_info "Log saved to: $LOG_FILE"
}

show_help() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
  (none)       Interactive mode - prompts for node ID assignment
  --help       Show this help message
  --auto       Auto-assign node IDs (1, 2, 3, ...) to detected devices

Description:
  This script uploads the multi-node swarm firmware to Arduino boards.
  Each board is configured with a unique NODE_ID (1-5).

  Node 1: Coordinator (acts as anchor)
  Nodes 2-5: Mobile tags (with TDMA slot allocation)

Requirements:
  - PlatformIO CLI installed
  - Arduino Uno boards connected via USB
  - Firmware in: $SCRIPT_DIR

Examples:
  # Interactive mode (recommended)
  ./upload_swarm.sh

  # Auto-assign node IDs
  ./upload_swarm.sh --auto

See Also:
  - README.md for detailed setup instructions
  - monitor_swarm.py to monitor all nodes simultaneously

EOF
}

auto_upload() {
    print_banner
    check_dependencies
    detect_arduino_devices

    print_info "Auto-upload mode: Assigning Node IDs sequentially..."
    echo ""

    node_id=1
    for device in "${DEVICES[@]}"; do
        if [ $node_id -gt $MAX_NODES ]; then
            print_warning "Maximum nodes ($MAX_NODES) reached. Skipping $device"
            continue
        fi

        echo ""
        print_info "Uploading Node $node_id to $device..."

        # Update config.h
        if ! update_config_h "$node_id"; then
            print_error "Failed to update configuration for Node $node_id"
            continue
        fi

        # Compile firmware
        if ! compile_firmware; then
            print_error "Compilation failed for Node $node_id"
            continue
        fi

        # Upload to device
        if upload_to_device "$device" "$node_id"; then
            print_success "Node $node_id configured on $device"
            node_id=$((node_id + 1))
        else
            print_error "Upload failed for $device"
        fi
    done

    echo ""
    print_info "Auto-upload complete. Configured $((node_id - 1)) nodes."
}

################################################################################
# Main
################################################################################

# Initialize log
echo "=== SwarmLoc Upload Log ===" > "$LOG_FILE"
echo "Date: $(date)" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

# Parse arguments
case "${1:-}" in
    --help|-h)
        show_help
        exit 0
        ;;
    --auto)
        auto_upload
        ;;
    "")
        interactive_upload
        ;;
    *)
        print_error "Unknown option: $1"
        show_help
        exit 1
        ;;
esac

exit 0
