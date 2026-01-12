#!/bin/bash

###############################################################################
# Test Script for Dual-Role Firmware
#
# Project: SwarmLoc - GPS-Denied Positioning System
# Test: test_07_dual_role
# Date: 2026-01-11
#
# Purpose: Automated testing of dual-role firmware functionality
#
# Usage:
#   ./test_dual_role.sh [test_number]
#
# Tests:
#   1 - Single node boot and status
#   2 - Role switching (A→T→A)
#   3 - EEPROM persistence
#   4 - Anchor-Tag pair ranging
#   5 - TDMA multi-tag operation
#   all - Run all tests
#
###############################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERIAL_PORT_1="/dev/ttyACM0"
SERIAL_PORT_2="/dev/ttyACM1"
BAUD_RATE=115200
TEST_DURATION=30  # seconds

###############################################################################
# Helper Functions
###############################################################################

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

check_serial_port() {
    local port=$1
    if [ ! -e "$port" ]; then
        print_error "Serial port $port not found"
        echo "Available ports:"
        ls -l /dev/ttyACM* 2>/dev/null || echo "  No /dev/ttyACM* ports found"
        return 1
    fi
    print_success "Serial port $port found"
    return 0
}

monitor_serial() {
    local port=$1
    local duration=$2
    local output_file=$3

    print_info "Monitoring $port for ${duration}s..."

    # Use screen to capture output
    timeout ${duration}s screen -L -Logfile "$output_file" "$port" $BAUD_RATE || true

    if [ -f "$output_file" ]; then
        print_success "Captured output to $output_file"
        return 0
    else
        print_error "Failed to capture output"
        return 1
    fi
}

send_serial_command() {
    local port=$1
    local command=$2

    print_info "Sending command '$command' to $port"

    # Send command using echo and stty
    stty -F "$port" $BAUD_RATE raw -echo
    echo -n "$command" > "$port"

    print_success "Command sent"
}

###############################################################################
# Test Functions
###############################################################################

test_01_boot_status() {
    print_header "Test 1: Single Node Boot and Status"

    check_serial_port "$SERIAL_PORT_1" || return 1

    print_info "Step 1: Monitor boot sequence"
    monitor_serial "$SERIAL_PORT_1" 10 "test_01_boot.log"

    print_info "Step 2: Check for expected boot messages"
    if grep -q "SwarmLoc Dual-Role Firmware" test_01_boot.log && \
       grep -q "Current role:" test_01_boot.log; then
        print_success "Boot sequence detected"
    else
        print_error "Boot messages missing"
        return 1
    fi

    print_info "Step 3: Send status command 'S'"
    sleep 2
    send_serial_command "$SERIAL_PORT_1" "S"

    print_info "Step 4: Capture status output"
    monitor_serial "$SERIAL_PORT_1" 5 "test_01_status.log"

    print_info "Step 5: Verify status output"
    if grep -q "SYSTEM STATUS" test_01_status.log && \
       grep -q "Current Role:" test_01_status.log && \
       grep -q "Free RAM:" test_01_status.log; then
        print_success "Status command works"
    else
        print_error "Status output incomplete"
        return 1
    fi

    print_success "Test 1 PASSED"
    return 0
}

test_02_role_switching() {
    print_header "Test 2: Role Switching (A→T→A)"

    check_serial_port "$SERIAL_PORT_1" || return 1

    print_info "Step 1: Check initial role"
    send_serial_command "$SERIAL_PORT_1" "S"
    monitor_serial "$SERIAL_PORT_1" 5 "test_02_initial.log"

    if grep -q "Current Role: TAG" test_02_initial.log; then
        print_info "Initial role: TAG"
        INITIAL_ROLE="TAG"
    elif grep -q "Current Role: ANCHOR" test_02_initial.log; then
        print_info "Initial role: ANCHOR"
        INITIAL_ROLE="ANCHOR"
    else
        print_error "Could not determine initial role"
        return 1
    fi

    print_info "Step 2: Switch to opposite role"
    if [ "$INITIAL_ROLE" = "TAG" ]; then
        print_info "Switching TAG → ANCHOR"
        send_serial_command "$SERIAL_PORT_1" "A"
        sleep 8  # Wait for reset
    else
        print_info "Switching ANCHOR → TAG"
        send_serial_command "$SERIAL_PORT_1" "T"
        sleep 8  # Wait for reset
    fi

    print_info "Step 3: Verify new role"
    monitor_serial "$SERIAL_PORT_1" 5 "test_02_switched.log"

    if [ "$INITIAL_ROLE" = "TAG" ]; then
        if grep -q "Current role: ANCHOR" test_02_switched.log; then
            print_success "Switched to ANCHOR"
        else
            print_error "Failed to switch to ANCHOR"
            return 1
        fi
    else
        if grep -q "Current role: TAG" test_02_switched.log; then
            print_success "Switched to TAG"
        else
            print_error "Failed to switch to TAG"
            return 1
        fi
    fi

    print_info "Step 4: Switch back to original role"
    if [ "$INITIAL_ROLE" = "TAG" ]; then
        send_serial_command "$SERIAL_PORT_1" "T"
    else
        send_serial_command "$SERIAL_PORT_1" "A"
    fi
    sleep 8

    print_info "Step 5: Verify back to original"
    monitor_serial "$SERIAL_PORT_1" 5 "test_02_restored.log"

    if grep -q "Current role: $INITIAL_ROLE" test_02_restored.log; then
        print_success "Restored to original role: $INITIAL_ROLE"
    else
        print_error "Failed to restore original role"
        return 1
    fi

    print_success "Test 2 PASSED"
    return 0
}

test_03_eeprom_persistence() {
    print_header "Test 3: EEPROM Persistence"

    check_serial_port "$SERIAL_PORT_1" || return 1

    print_info "Step 1: Set role to ANCHOR"
    send_serial_command "$SERIAL_PORT_1" "A"
    sleep 8

    print_info "Step 2: Capture current role"
    monitor_serial "$SERIAL_PORT_1" 5 "test_03_before_power.log"

    if ! grep -q "Current role: ANCHOR" test_03_before_power.log; then
        print_error "Failed to set ANCHOR role"
        return 1
    fi

    print_warning "Step 3: MANUAL ACTION REQUIRED"
    print_warning "Please unplug and replug the USB cable for $SERIAL_PORT_1"
    print_warning "Press ENTER when done..."
    read -r

    print_info "Step 4: Wait for boot"
    sleep 3

    print_info "Step 5: Check role after power cycle"
    monitor_serial "$SERIAL_PORT_1" 5 "test_03_after_power.log"

    if grep -q "Current role: ANCHOR" test_03_after_power.log; then
        print_success "Role persisted across power cycle"
    else
        print_error "Role did NOT persist"
        return 1
    fi

    print_success "Test 3 PASSED"
    return 0
}

test_04_anchor_tag_ranging() {
    print_header "Test 4: Anchor-Tag Pair Ranging"

    check_serial_port "$SERIAL_PORT_1" || return 1
    check_serial_port "$SERIAL_PORT_2" || return 1

    print_info "Step 1: Configure Node 1 as ANCHOR"
    send_serial_command "$SERIAL_PORT_1" "A"
    sleep 8

    print_info "Step 2: Configure Node 2 as TAG"
    send_serial_command "$SERIAL_PORT_2" "T"
    sleep 8

    print_info "Step 3: Monitor both nodes for ${TEST_DURATION}s"
    print_warning "MANUAL ACTION: Ensure nodes are 1-2 meters apart"

    # Monitor both in parallel
    timeout ${TEST_DURATION}s screen -L -Logfile "test_04_anchor.log" "$SERIAL_PORT_1" $BAUD_RATE &
    PID1=$!
    timeout ${TEST_DURATION}s screen -L -Logfile "test_04_tag.log" "$SERIAL_PORT_2" $BAUD_RATE &
    PID2=$!

    wait $PID1 $PID2 || true

    print_info "Step 4: Analyze ranging output"

    # Check anchor output
    if grep -q "RANGE" test_04_anchor.log && grep -q "Distance:" test_04_anchor.log; then
        print_success "Anchor received ranging messages"
        ANCHOR_RANGES=$(grep -c "RANGE" test_04_anchor.log)
        print_info "Anchor: $ANCHOR_RANGES range measurements"
    else
        print_error "Anchor: No ranging messages"
        return 1
    fi

    # Check tag output
    if grep -q "RANGE" test_04_tag.log && grep -q "Distance:" test_04_tag.log; then
        print_success "Tag received ranging messages"
        TAG_RANGES=$(grep -c "RANGE" test_04_tag.log)
        print_info "Tag: $TAG_RANGES range measurements"
    else
        print_error "Tag: No ranging messages"
        return 1
    fi

    # Calculate update rate
    UPDATE_RATE=$(echo "scale=2; $TAG_RANGES / $TEST_DURATION" | bc)
    print_info "Update rate: ${UPDATE_RATE} Hz"

    if (( $(echo "$UPDATE_RATE >= 1.0" | bc -l) )); then
        print_success "Update rate acceptable (≥1 Hz)"
    else
        print_warning "Update rate low (<1 Hz)"
    fi

    print_success "Test 4 PASSED"
    return 0
}

test_05_tdma_multi_tag() {
    print_header "Test 5: TDMA Multi-Tag Operation"

    print_warning "This test requires 3+ nodes:"
    print_warning "  - 1 ANCHOR (port $SERIAL_PORT_1)"
    print_warning "  - 2+ TAGs (including port $SERIAL_PORT_2)"

    check_serial_port "$SERIAL_PORT_1" || return 1
    check_serial_port "$SERIAL_PORT_2" || return 1

    print_info "Step 1: Configure Node 1 as ANCHOR"
    send_serial_command "$SERIAL_PORT_1" "A"
    sleep 8

    print_info "Step 2: Configure Node 2 as TAG"
    send_serial_command "$SERIAL_PORT_2" "T"
    sleep 8

    print_warning "Step 3: MANUAL ACTION REQUIRED"
    print_warning "If you have a 3rd node, configure it as TAG now"
    print_warning "Press ENTER when ready to start test..."
    read -r

    print_info "Step 4: Monitor for ${TEST_DURATION}s"

    timeout ${TEST_DURATION}s screen -L -Logfile "test_05_anchor.log" "$SERIAL_PORT_1" $BAUD_RATE &
    PID1=$!
    timeout ${TEST_DURATION}s screen -L -Logfile "test_05_tag1.log" "$SERIAL_PORT_2" $BAUD_RATE &
    PID2=$!

    wait $PID1 $PID2 || true

    print_info "Step 5: Analyze TDMA operation"

    # Count unique tag addresses in anchor log
    if [ -f test_05_anchor.log ]; then
        TAG_COUNT=$(grep "RANGE" test_05_anchor.log | awk '{print $3}' | sort -u | wc -l)
        print_info "Anchor detected $TAG_COUNT unique tag(s)"

        if [ "$TAG_COUNT" -ge 2 ]; then
            print_success "Multiple tags detected"
        else
            print_warning "Only 1 tag detected (need 2+ for TDMA test)"
        fi
    fi

    # Check for TDMA slot messages
    if grep -q "TDMA enabled" test_05_tag1.log; then
        print_success "TDMA enabled on tag"
    else
        print_warning "TDMA not enabled (check NUM_NODES configuration)"
    fi

    print_success "Test 5 COMPLETED (manual verification required)"
    return 0
}

###############################################################################
# Main Script
###############################################################################

main() {
    print_header "SwarmLoc Dual-Role Firmware Test Suite"

    # Check if test number provided
    if [ $# -eq 0 ]; then
        echo "Usage: $0 [test_number]"
        echo ""
        echo "Available tests:"
        echo "  1   - Single node boot and status"
        echo "  2   - Role switching (A→T→A)"
        echo "  3   - EEPROM persistence"
        echo "  4   - Anchor-Tag pair ranging"
        echo "  5   - TDMA multi-tag operation"
        echo "  all - Run all tests"
        echo ""
        exit 1
    fi

    TEST_NUM=$1

    # Create log directory
    mkdir -p logs
    cd logs

    case $TEST_NUM in
        1)
            test_01_boot_status
            ;;
        2)
            test_02_role_switching
            ;;
        3)
            test_03_eeprom_persistence
            ;;
        4)
            test_04_anchor_tag_ranging
            ;;
        5)
            test_05_tdma_multi_tag
            ;;
        all)
            test_01_boot_status && \
            test_02_role_switching && \
            test_03_eeprom_persistence && \
            test_04_anchor_tag_ranging && \
            test_05_tdma_multi_tag
            ;;
        *)
            print_error "Unknown test number: $TEST_NUM"
            exit 1
            ;;
    esac

    if [ $? -eq 0 ]; then
        print_header "ALL TESTS PASSED"
        exit 0
    else
        print_header "TESTS FAILED"
        exit 1
    fi
}

# Run main
main "$@"
