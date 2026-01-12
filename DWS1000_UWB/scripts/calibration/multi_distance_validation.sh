#!/bin/bash
#
# DW1000 Multi-Distance Validation Script
#
# Purpose: Validate calibration across multiple distances
# Usage: ./multi_distance_validation.sh [PORT]
#
# Tests at standard distances: 0.5m, 1.0m, 2.0m, 3.0m, 5.0m
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
PORT="${1:-/dev/ttyUSB0}"
BAUD_RATE=115200
MEASUREMENT_TIME=30  # seconds per distance
SAMPLES_MIN=30  # minimum samples required

# Test distances (meters)
declare -a TEST_DISTANCES=(0.5 1.0 2.0 3.0 5.0)

# Derived paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/validation_data"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
VALIDATION_DIR="${OUTPUT_DIR}/validation_${TIMESTAMP}"
LOG_FILE="${VALIDATION_DIR}/validation.log"

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}   DW1000 Multi-Distance Validation${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""
echo -e "Port:           ${GREEN}${PORT}${NC}"
echo -e "Test Distances: ${GREEN}${TEST_DISTANCES[@]} m${NC}"
echo -e "Time/Distance:  ${GREEN}${MEASUREMENT_TIME} seconds${NC}"
echo ""

# Create output directory
mkdir -p "${VALIDATION_DIR}"

# Function to log messages
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "${LOG_FILE}"
}

# Function to check device
check_device() {
    if [ ! -e "${PORT}" ]; then
        echo -e "${RED}ERROR: Device not found at ${PORT}${NC}"
        exit 1
    fi
    log "Device found at ${PORT}"
}

# Function to collect measurements at one distance
collect_at_distance() {
    local distance=$1
    local output_file="${VALIDATION_DIR}/distance_${distance}m.csv"

    echo ""
    echo -e "${BLUE}================================================${NC}"
    echo -e "${BLUE}   Testing at ${distance} meters${NC}"
    echo -e "${BLUE}================================================${NC}"
    echo ""
    echo -e "${YELLOW}ACTION REQUIRED:${NC}"
    echo -e "1. Position devices at exactly ${GREEN}${distance} meters${NC} apart"
    echo -e "2. Ensure clear line-of-sight"
    echo -e "3. Verify both devices are powered and running"
    echo -e "4. Press Enter when ready..."
    echo ""
    read -r

    log "Starting measurement at ${distance}m"

    # Collect data
    echo -e "${YELLOW}Collecting data for ${MEASUREMENT_TIME} seconds...${NC}"
    timeout ${MEASUREMENT_TIME}s cat "${PORT}" > "${output_file}" 2>&1 || true

    # Check sample count
    local count=$(grep -c "^[0-9]" "${output_file}" 2>/dev/null || echo "0")
    log "Collected ${count} samples at ${distance}m"

    if [ "${count}" -lt "${SAMPLES_MIN}" ]; then
        echo -e "${RED}WARNING: Only ${count} samples collected (minimum: ${SAMPLES_MIN})${NC}"
        echo -e "${YELLOW}Do you want to retry this distance? (y/n)${NC}"
        read -r response
        if [[ "$response" =~ ^[Yy]$ ]]; then
            collect_at_distance "${distance}"
            return
        fi
    fi

    # Analyze measurements
    python3 "${SCRIPT_DIR}/analyze_measurements.py" \
        --input "${output_file}" \
        --actual-distance "${distance}" \
        --output-json "${output_file%.csv}.json" \
        --quiet

    # Display results
    local mean=$(python3 -c "import json; d=json.load(open('${output_file%.csv}.json')); print(f\"{d['mean_distance_m']:.3f}\")")
    local error=$(python3 -c "import json; d=json.load(open('${output_file%.csv}.json')); print(f\"{d['error_cm']:.1f}\")")
    local stddev=$(python3 -c "import json; d=json.load(open('${output_file%.csv}.json')); print(f\"{d['stddev_cm']:.1f}\")")

    echo ""
    echo -e "${GREEN}Results at ${distance}m:${NC}"
    echo -e "  Measured:   ${mean} m"
    echo -e "  Error:      ${error} cm"
    echo -e "  Std Dev:    ${stddev} cm"
    echo ""
}

# Function to generate validation report
generate_report() {
    local report_file="${VALIDATION_DIR}/validation_report.txt"
    local csv_file="${VALIDATION_DIR}/validation_summary.csv"

    log "Generating validation report..."

    # Create CSV header
    echo "Actual_Distance_m,Measured_Distance_m,Error_cm,StdDev_cm,Min_m,Max_m,Samples" > "${csv_file}"

    # Collect all results
    echo -e "${BLUE}================================================${NC}"
    echo -e "${BLUE}   Validation Summary${NC}"
    echo -e "${BLUE}================================================${NC}"
    echo ""
    printf "%-12s %-12s %-10s %-10s\n" "Distance" "Measured" "Error" "Std Dev"
    printf "%-12s %-12s %-10s %-10s\n" "--------" "--------" "-----" "-------"

    local total_error=0
    local max_error=0
    local num_distances=0

    for distance in "${TEST_DISTANCES[@]}"; do
        local json_file="${VALIDATION_DIR}/distance_${distance}m.json"

        if [ -f "${json_file}" ]; then
            local mean=$(python3 -c "import json; d=json.load(open('${json_file}')); print(f\"{d['mean_distance_m']:.3f}\")")
            local error=$(python3 -c "import json; d=json.load(open('${json_file}')); print(f\"{d['error_cm']:.1f}\")")
            local stddev=$(python3 -c "import json; d=json.load(open('${json_file}')); print(f\"{d['stddev_cm']:.1f}\")")
            local min_dist=$(python3 -c "import json; d=json.load(open('${json_file}')); print(f\"{d['min_distance_m']:.3f}\")")
            local max_dist=$(python3 -c "import json; d=json.load(open('${json_file}')); print(f\"{d['max_distance_m']:.3f}\")")
            local samples=$(python3 -c "import json; d=json.load(open('${json_file}')); print(d['num_samples'])")

            printf "%-12s %-12s %-10s %-10s\n" "${distance}m" "${mean}m" "${error}cm" "${stddev}cm"

            # Add to CSV
            echo "${distance},${mean},${error},${stddev},${min_dist},${max_dist},${samples}" >> "${csv_file}"

            # Track statistics
            local abs_error=$(python3 -c "print(abs(${error}))")
            total_error=$(python3 -c "print(${total_error} + ${abs_error})")
            if (( $(echo "${abs_error} > ${max_error}" | bc -l) )); then
                max_error=${abs_error}
            fi
            num_distances=$((num_distances + 1))
        fi
    done

    echo ""

    # Calculate average error
    local avg_error=$(python3 -c "print(${total_error} / ${num_distances} if ${num_distances} > 0 else 0)")

    # Determine pass/fail
    local pass_threshold=10.0
    local pass=true

    if (( $(echo "${max_error} > ${pass_threshold}" | bc -l) )); then
        pass=false
    fi

    # Display summary statistics
    echo -e "${BLUE}Statistics:${NC}"
    echo -e "  Average Error:  ${avg_error} cm"
    echo -e "  Maximum Error:  ${max_error} cm"
    echo ""

    if [ "${pass}" = true ]; then
        echo -e "${GREEN}================================================${NC}"
        echo -e "${GREEN}   VALIDATION PASSED${NC}"
        echo -e "${GREEN}================================================${NC}"
        echo -e "${GREEN}All measurements within ±${pass_threshold} cm${NC}"
        log "Validation PASSED"
    else
        echo -e "${YELLOW}================================================${NC}"
        echo -e "${YELLOW}   VALIDATION NEEDS IMPROVEMENT${NC}"
        echo -e "${YELLOW}================================================${NC}"
        echo -e "${YELLOW}Maximum error (${max_error} cm) exceeds threshold (${pass_threshold} cm)${NC}"
        echo -e "${YELLOW}Consider recalibration or checking for:${NC}"
        echo -e "  - Measurement accuracy"
        echo -e "  - RF interference"
        echo -e "  - Multipath effects"
        log "Validation NEEDS IMPROVEMENT (max error: ${max_error} cm)"
    fi

    echo ""
    echo -e "${GREEN}CSV data saved to: ${csv_file}${NC}"
    echo -e "${GREEN}Validation directory: ${VALIDATION_DIR}${NC}"
    echo ""

    # Create detailed report
    cat > "${report_file}" << EOF
DW1000 Multi-Distance Validation Report
========================================

Date: $(date)
Test Distances: ${TEST_DISTANCES[@]} m
Measurement Time: ${MEASUREMENT_TIME} seconds per distance

Results Summary:
----------------
$(cat "${csv_file}")

Statistics:
-----------
Average Error: ${avg_error} cm
Maximum Error: ${max_error} cm
Pass Threshold: ${pass_threshold} cm

Result: $([ "${pass}" = true ] && echo "PASSED" || echo "NEEDS IMPROVEMENT")

Recommendations:
----------------
EOF

    if [ "${pass}" = true ]; then
        cat >> "${report_file}" << EOF
The calibration is performing well across all test distances.
The antenna delay value is correctly configured.

Next steps:
- Document the calibrated antenna delay in your configuration
- Test in your target environment
- Consider periodic recalibration if temperature varies significantly
EOF
    else
        cat >> "${report_file}" << EOF
The calibration shows errors exceeding the acceptable threshold.

Recommended actions:
1. Check if error is constant across all distances
   - If constant: Run antenna delay calibration again
   - If increasing with distance: Check for clock drift or hardware issues

2. Verify measurement setup:
   - Ensure distances measured accurately (center-to-center)
   - Check for clear line-of-sight
   - Verify no RF interference

3. Environmental factors:
   - Check for multipath reflections
   - Ensure stable temperature
   - Test in outdoor environment if possible

4. Re-run calibration using: ./calibrate_antenna_delay.sh
EOF
    fi

    echo -e "${GREEN}Report saved to: ${report_file}${NC}"
}

# Function to plot results (if matplotlib available)
plot_results() {
    log "Attempting to generate plots..."

    local csv_file="${VALIDATION_DIR}/validation_summary.csv"

    if ! python3 -c "import matplotlib" 2>/dev/null; then
        echo -e "${YELLOW}Matplotlib not available, skipping plots${NC}"
        return
    fi

    python3 "${SCRIPT_DIR}/analyze_measurements.py" \
        --plot-validation "${csv_file}" \
        --output-plot "${VALIDATION_DIR}/validation_plot.png" \
        2>/dev/null || echo -e "${YELLOW}Plot generation failed${NC}"
}

# Main execution
main() {
    log "Validation script started"

    # Check prerequisites
    check_device

    # Check Python
    if ! command -v python3 &> /dev/null; then
        echo -e "${RED}ERROR: Python 3 is required${NC}"
        exit 1
    fi

    # Check analysis script
    if [ ! -f "${SCRIPT_DIR}/analyze_measurements.py" ]; then
        echo -e "${RED}ERROR: analyze_measurements.py not found${NC}"
        exit 1
    fi

    echo -e "${YELLOW}This script will test calibration at ${#TEST_DISTANCES[@]} distances${NC}"
    echo -e "${YELLOW}Estimated time: $((MEASUREMENT_TIME * ${#TEST_DISTANCES[@]} / 60)) minutes${NC}"
    echo -e "${YELLOW}Press Enter to continue or Ctrl+C to abort...${NC}"
    read -r

    # Test each distance
    for distance in "${TEST_DISTANCES[@]}"; do
        collect_at_distance "${distance}"
    done

    # Generate report
    generate_report

    # Try to generate plots
    plot_results

    log "Validation script completed"
}

# Run main
main
