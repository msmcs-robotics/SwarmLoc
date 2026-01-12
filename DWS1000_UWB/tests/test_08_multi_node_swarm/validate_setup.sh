#!/bin/bash

################################################################################
# validate_setup.sh - Pre-Test Validation Script
#
# Purpose: Verify that all prerequisites are met before running swarm test
#
# Usage: ./validate_setup.sh
################################################################################

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}SwarmLoc Swarm Test Validation${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

print_pass() {
    echo -e "${GREEN}✓${NC} $1"
    PASS_COUNT=$((PASS_COUNT + 1))
}

print_fail() {
    echo -e "${RED}✗${NC} $1"
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

print_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
    WARN_COUNT=$((WARN_COUNT + 1))
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

check_command() {
    local cmd=$1
    local install_hint=$2

    if command -v "$cmd" &> /dev/null; then
        print_pass "$cmd found"
        return 0
    else
        print_fail "$cmd not found"
        if [ -n "$install_hint" ]; then
            echo "   Install: $install_hint"
        fi
        return 1
    fi
}

check_python_module() {
    local module=$1
    local install_hint=$2

    if python3 -c "import $module" &> /dev/null; then
        print_pass "Python module '$module' found"
        return 0
    else
        print_warn "Python module '$module' not found (optional)"
        if [ -n "$install_hint" ]; then
            echo "   Install: $install_hint"
        fi
        return 1
    fi
}

print_header

# Check 1: PlatformIO
echo "Checking build tools..."
check_command "pio" "curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/get-platformio.py | python3"

# Check 2: Python 3
check_command "python3" "sudo apt-get install python3"

# Check 3: Required Python modules
echo ""
echo "Checking Python dependencies..."
check_python_module "serial" "pip3 install pyserial"

# Check 4: Optional Python modules
check_python_module "colorama" "pip3 install colorama"
check_python_module "matplotlib" "pip3 install matplotlib"
check_python_module "numpy" "pip3 install numpy"

# Check 5: Connected devices
echo ""
echo "Checking for Arduino devices..."
DEVICES=($(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true))

if [ ${#DEVICES[@]} -eq 0 ]; then
    print_fail "No Arduino devices found"
    print_info "Connect Arduino boards via USB"
else
    print_pass "Found ${#DEVICES[@]} device(s):"
    for device in "${DEVICES[@]}"; do
        echo "   - $device"
    done

    # Recommend minimum 3 nodes
    if [ ${#DEVICES[@]} -lt 3 ]; then
        print_warn "Only ${#DEVICES[@]} device(s) connected. Minimum 3 recommended for swarm test."
    fi

    # Check permissions
    if [ -r "${DEVICES[0]}" ] && [ -w "${DEVICES[0]}" ]; then
        print_pass "Device permissions OK"
    else
        print_warn "May not have permission to access devices"
        echo "   Try: sudo usermod -a -G dialout \$USER"
        echo "   Then log out and back in"
    fi
fi

# Check 6: Project structure
echo ""
echo "Checking project files..."

FILES=(
    "node_firmware.ino"
    "config.h"
    "upload_swarm.sh"
    "monitor_swarm.py"
    "analyze_swarm_data.py"
    "README.md"
)

for file in "${FILES[@]}"; do
    if [ -f "$SCRIPT_DIR/$file" ]; then
        print_pass "$file exists"
    else
        print_fail "$file missing"
    fi
done

# Check if scripts are executable
if [ -x "$SCRIPT_DIR/upload_swarm.sh" ]; then
    print_pass "upload_swarm.sh is executable"
else
    print_warn "upload_swarm.sh not executable"
    echo "   Run: chmod +x upload_swarm.sh"
fi

if [ -x "$SCRIPT_DIR/monitor_swarm.py" ]; then
    print_pass "monitor_swarm.py is executable"
else
    print_warn "monitor_swarm.py not executable"
    echo "   Run: chmod +x monitor_swarm.py"
fi

# Check 7: DW1000 library
echo ""
echo "Checking DW1000 library..."
LIB_PATH="$SCRIPT_DIR/../../lib/DW1000"

if [ -d "$LIB_PATH" ]; then
    print_pass "DW1000 library found"

    # Check for key files
    if [ -f "$LIB_PATH/src/DW1000Ranging.h" ]; then
        print_pass "DW1000Ranging.h found"
    else
        print_fail "DW1000Ranging.h missing"
    fi
else
    print_fail "DW1000 library not found at $LIB_PATH"
fi

# Check 8: platformio.ini
echo ""
echo "Checking PlatformIO configuration..."
PLATFORMIO_INI="$SCRIPT_DIR/../../platformio.ini"

if [ -f "$PLATFORMIO_INI" ]; then
    print_pass "platformio.ini found"

    # Check for required settings
    if grep -q "board = uno" "$PLATFORMIO_INI"; then
        print_pass "Arduino Uno board configured"
    else
        print_warn "Arduino Uno not configured in platformio.ini"
    fi

    if grep -q "DW1000" "$PLATFORMIO_INI"; then
        print_pass "DW1000 library configured"
    else
        print_warn "DW1000 library not configured in platformio.ini"
    fi
else
    print_fail "platformio.ini not found"
fi

# Summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Validation Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Passed:  $PASS_COUNT${NC}"
echo -e "${YELLOW}Warnings: $WARN_COUNT${NC}"
echo -e "${RED}Failed:  $FAIL_COUNT${NC}"
echo ""

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "${GREEN}✓ System ready for swarm testing!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. ./upload_swarm.sh          # Upload firmware to all nodes"
    echo "  2. python3 monitor_swarm.py   # Start monitoring"
    echo "  3. python3 analyze_swarm_data.py logs/  # Analyze results"
    echo ""
    exit 0
else
    echo -e "${RED}✗ Please fix the above issues before testing${NC}"
    echo ""
    echo "Common fixes:"
    echo "  - Install PlatformIO: curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/get-platformio.py | python3"
    echo "  - Install Python deps: pip3 install pyserial colorama matplotlib numpy"
    echo "  - Connect Arduino boards via USB"
    echo "  - Fix permissions: sudo usermod -a -G dialout \$USER (then re-login)"
    echo ""
    exit 1
fi
