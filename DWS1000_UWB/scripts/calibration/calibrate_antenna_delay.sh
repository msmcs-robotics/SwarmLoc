#!/bin/bash
#
# DW1000 Antenna Delay Calibration - Master Script
#
# Purpose: Automated iterative calibration of antenna delay
# Usage: ./calibrate_antenna_delay.sh [PORT] [ACTUAL_DISTANCE]
#
# Requirements:
# - Arduino with calibration firmware uploaded
# - Clear line-of-sight setup at known distance
# - Python 3 (for analysis)
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PORT="${1:-/dev/ttyUSB0}"
ACTUAL_DISTANCE="${2:-1.000}"
BAUD_RATE=115200
SAMPLES_PER_ITERATION=50
MAX_ITERATIONS=10
TARGET_ERROR_CM=5.0
MEASUREMENT_TIME=30  # seconds

# Derived paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/calibration_data"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_FILE="${OUTPUT_DIR}/calibration_${TIMESTAMP}.log"

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}   DW1000 Antenna Delay Calibration${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""
echo -e "Port:               ${GREEN}${PORT}${NC}"
echo -e "Actual Distance:    ${GREEN}${ACTUAL_DISTANCE} m${NC}"
echo -e "Target Error:       ${GREEN}<${TARGET_ERROR_CM} cm${NC}"
echo -e "Samples/Iteration:  ${GREEN}${SAMPLES_PER_ITERATION}${NC}"
echo -e "Max Iterations:     ${GREEN}${MAX_ITERATIONS}${NC}"
echo ""

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Function to log messages
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "${LOG_FILE}"
}

# Function to check if device is connected
check_device() {
    if [ ! -e "${PORT}" ]; then
        echo -e "${RED}ERROR: Device not found at ${PORT}${NC}"
        echo -e "${YELLOW}Available ports:${NC}"
        ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "No devices found"
        exit 1
    fi
    log "Device found at ${PORT}"
}

# Function to collect measurements
collect_measurements() {
    local output_file=$1
    local duration=$2

    log "Collecting measurements for ${duration} seconds..."
    echo -e "${YELLOW}Please ensure TAG and ANCHOR are at ${ACTUAL_DISTANCE}m distance${NC}"
    echo -e "${YELLOW}Data collection in progress...${NC}"

    # Capture serial data
    timeout ${duration}s cat "${PORT}" > "${output_file}" 2>&1 || true

    # Count valid measurements
    local count=$(grep -c "^[0-9]" "${output_file}" 2>/dev/null || echo "0")
    log "Collected ${count} measurements"

    if [ "${count}" -lt 10 ]; then
        echo -e "${RED}WARNING: Only ${count} measurements collected${NC}"
        echo -e "${YELLOW}Make sure the firmware is running and outputting CSV data${NC}"
        return 1
    fi

    return 0
}

# Function to parse measurements and calculate statistics
analyze_measurements() {
    local data_file=$1

    log "Analyzing measurements..."

    # Use Python analysis script
    python3 "${SCRIPT_DIR}/analyze_measurements.py" \
        --input "${data_file}" \
        --actual-distance "${ACTUAL_DISTANCE}" \
        --output-json "${data_file%.csv}.json" \
        --quiet
}

# Function to calculate antenna delay adjustment
calculate_adjustment() {
    local json_file=$1

    # Extract values from JSON
    local mean_distance=$(python3 -c "import json; print(json.load(open('${json_file}'))['mean_distance_m'])")
    local error=$(python3 -c "import json; print(json.load(open('${json_file}'))['error_m'])")
    local error_cm=$(python3 -c "import json; print(json.load(open('${json_file}'))['error_cm'])")

    # Calculate adjustment
    # error_per_device = error_m / 2.0
    # adjustment = error_per_device * 213.14
    local adjustment=$(python3 -c "error=${error}; adj=int((error/2.0)*213.14); print(adj)")

    echo "${adjustment}"
}

# Function to get current antenna delay from firmware
get_current_delay() {
    # This would need to be implemented based on how the firmware reports it
    # For now, we'll track it in a file
    local delay_file="${OUTPUT_DIR}/current_delay.txt"

    if [ -f "${delay_file}" ]; then
        cat "${delay_file}"
    else
        echo "16450"  # Default starting value
    fi
}

# Function to set antenna delay in firmware
set_antenna_delay() {
    local new_delay=$1
    local delay_file="${OUTPUT_DIR}/current_delay.txt"

    echo "${new_delay}" > "${delay_file}"

    echo -e "${YELLOW}================================================${NC}"
    echo -e "${YELLOW}ACTION REQUIRED: Update Antenna Delay${NC}"
    echo -e "${YELLOW}================================================${NC}"
    echo ""
    echo -e "1. Update ANTENNA_DELAY in firmware to: ${GREEN}${new_delay}${NC}"
    echo -e "2. Re-upload firmware to both TAG and ANCHOR"
    echo -e "3. Press Enter when ready to continue..."
    echo ""
    read -r
}

# Main calibration loop
calibrate() {
    log "Starting calibration process..."

    local iteration=1
    local current_delay=$(get_current_delay)
    local converged=false

    echo -e "${BLUE}Initial antenna delay: ${current_delay}${NC}"
    echo ""

    while [ ${iteration} -le ${MAX_ITERATIONS} ]; do
        echo -e "${BLUE}================================================${NC}"
        echo -e "${BLUE}   Iteration ${iteration}/${MAX_ITERATIONS}${NC}"
        echo -e "${BLUE}   Current Antenna Delay: ${current_delay}${NC}"
        echo -e "${BLUE}================================================${NC}"
        echo ""

        # Data file for this iteration
        local data_file="${OUTPUT_DIR}/iter_${iteration}_delay_${current_delay}.csv"
        local json_file="${OUTPUT_DIR}/iter_${iteration}_delay_${current_delay}.json"

        # Collect measurements
        if ! collect_measurements "${data_file}" ${MEASUREMENT_TIME}; then
            echo -e "${RED}Failed to collect measurements. Retrying...${NC}"
            sleep 2
            continue
        fi

        # Analyze measurements
        analyze_measurements "${data_file}"

        # Read results
        local mean_distance=$(python3 -c "import json; d=json.load(open('${json_file}')); print(f\"{d['mean_distance_m']:.3f}\")")
        local error_cm=$(python3 -c "import json; d=json.load(open('${json_file}')); print(f\"{d['error_cm']:.1f}\")")
        local stddev_cm=$(python3 -c "import json; d=json.load(open('${json_file}')); print(f\"{d['stddev_cm']:.1f}\")")
        local samples=$(python3 -c "import json; d=json.load(open('${json_file}')); print(d['num_samples'])")

        echo ""
        echo -e "${BLUE}Results:${NC}"
        echo -e "  Samples:          ${samples}"
        echo -e "  Measured:         ${mean_distance} m"
        echo -e "  Actual:           ${ACTUAL_DISTANCE} m"
        echo -e "  Error:            ${error_cm} cm"
        echo -e "  Std Dev:          ${stddev_cm} cm"
        echo ""

        # Check convergence
        local abs_error=$(python3 -c "print(abs(${error_cm}))")
        if (( $(echo "${abs_error} < ${TARGET_ERROR_CM}" | bc -l) )); then
            echo -e "${GREEN}================================================${NC}"
            echo -e "${GREEN}   CALIBRATION COMPLETE!${NC}"
            echo -e "${GREEN}================================================${NC}"
            echo ""
            echo -e "${GREEN}Final antenna delay: ${current_delay}${NC}"
            echo -e "${GREEN}Final error: ${error_cm} cm${NC}"
            echo -e "${GREEN}Standard deviation: ${stddev_cm} cm${NC}"
            echo ""
            log "Calibration completed successfully in ${iteration} iterations"
            converged=true
            break
        fi

        # Calculate adjustment
        local adjustment=$(calculate_adjustment "${json_file}")
        local new_delay=$((current_delay + adjustment))

        echo -e "${YELLOW}Adjustment needed:${NC}"
        echo -e "  Current delay:    ${current_delay}"
        echo -e "  Adjustment:       ${adjustment}"
        echo -e "  New delay:        ${new_delay}"
        echo ""

        # Sanity check
        if [ ${new_delay} -lt 16300 ] || [ ${new_delay} -gt 16600 ]; then
            echo -e "${RED}WARNING: New delay ${new_delay} is outside normal range (16300-16600)${NC}"
            echo -e "${YELLOW}This may indicate a measurement or hardware problem.${NC}"
            echo -e "${YELLOW}Do you want to continue? (y/n)${NC}"
            read -r response
            if [[ ! "$response" =~ ^[Yy]$ ]]; then
                log "Calibration aborted by user"
                exit 1
            fi
        fi

        # Apply new delay
        current_delay=${new_delay}
        set_antenna_delay ${current_delay}

        iteration=$((iteration + 1))
    done

    if [ "${converged}" = false ]; then
        echo -e "${RED}================================================${NC}"
        echo -e "${RED}   Calibration did not converge${NC}"
        echo -e "${RED}================================================${NC}"
        echo ""
        echo -e "${RED}Reached maximum iterations (${MAX_ITERATIONS})${NC}"
        echo -e "${YELLOW}Current error: ${error_cm} cm${NC}"
        echo -e "${YELLOW}Please check:${NC}"
        echo -e "  - Measurement distance is accurate"
        echo -e "  - Devices are aligned properly"
        echo -e "  - Clear line-of-sight exists"
        echo -e "  - No RF interference present"
        log "Calibration did not converge after ${MAX_ITERATIONS} iterations"
        exit 1
    fi

    # Generate final report
    generate_report "${current_delay}"
}

# Function to generate calibration report
generate_report() {
    local final_delay=$1
    local report_file="${OUTPUT_DIR}/calibration_report_${TIMESTAMP}.txt"

    cat > "${report_file}" << EOF
DW1000 Antenna Delay Calibration Report
========================================

Date: $(date)
Actual Distance: ${ACTUAL_DISTANCE} m
Target Error: <${TARGET_ERROR_CM} cm

Final Results:
--------------
Final Antenna Delay: ${final_delay}

Calibration completed successfully.

Next Steps:
-----------
1. The antenna delay has been set to ${final_delay}
2. Run multi-distance validation using: ./multi_distance_validation.sh
3. Document this value in your system configuration

All measurement data saved to: ${OUTPUT_DIR}

EOF

    echo ""
    echo -e "${GREEN}Report saved to: ${report_file}${NC}"
    cat "${report_file}"
}

# Main execution
main() {
    log "Calibration script started"

    # Check prerequisites
    check_device

    # Check if Python is available
    if ! command -v python3 &> /dev/null; then
        echo -e "${RED}ERROR: Python 3 is required but not found${NC}"
        exit 1
    fi

    # Check if analysis script exists
    if [ ! -f "${SCRIPT_DIR}/analyze_measurements.py" ]; then
        echo -e "${RED}ERROR: analyze_measurements.py not found${NC}"
        echo -e "${YELLOW}Please ensure all scripts are in ${SCRIPT_DIR}${NC}"
        exit 1
    fi

    # Run calibration
    calibrate

    log "Calibration script completed"
}

# Run main function
main
