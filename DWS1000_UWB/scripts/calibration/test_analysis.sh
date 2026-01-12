#!/bin/bash
#
# Test the calibration analysis tools
#
# This script tests the Python analysis tool with sample data
# to verify everything is working correctly.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

echo "Testing DW1000 Calibration Analysis Tools"
echo "=========================================="
echo ""

# Check Python
echo "1. Checking Python 3..."
if ! command -v python3 &> /dev/null; then
    echo "ERROR: Python 3 not found"
    exit 1
fi
echo "   ✓ Python 3 found: $(python3 --version)"
echo ""

# Check if sample data exists
echo "2. Checking sample data..."
if [ ! -f "sample_data.csv" ]; then
    echo "ERROR: sample_data.csv not found"
    exit 1
fi
echo "   ✓ Sample data found"
echo ""

# Test basic analysis
echo "3. Testing basic analysis..."
python3 analyze_measurements.py \
    --input sample_data.csv \
    --actual-distance 1.0 \
    --output-json test_results.json \
    --quiet

if [ ! -f "test_results.json" ]; then
    echo "ERROR: Failed to generate JSON output"
    exit 1
fi
echo "   ✓ Analysis successful"
echo ""

# Display results
echo "4. Results:"
echo "   Sample count: $(python3 -c "import json; print(json.load(open('test_results.json'))['num_samples'])")"
echo "   Mean distance: $(python3 -c "import json; print(f\"{json.load(open('test_results.json'))['mean_distance_m']:.3f} m\")")"
echo "   Error: $(python3 -c "import json; print(f\"{json.load(open('test_results.json'))['error_cm']:.1f} cm\")")"
echo "   Adjustment: $(python3 -c "import json; d=json.load(open('test_results.json')); adj=int((d['error_m']/2.0)*213.14); print(f\"{adj:+d} time units\")")"
echo ""

# Check matplotlib
echo "5. Checking matplotlib (optional for plots)..."
if python3 -c "import matplotlib" 2>/dev/null; then
    echo "   ✓ Matplotlib available"
    echo "   Testing plot generation..."

    python3 analyze_measurements.py \
        --input sample_data.csv \
        --actual-distance 1.0 \
        --output-plot test_plot.png \
        --quiet

    if [ -f "test_plot.png" ]; then
        echo "   ✓ Plot generated: test_plot.png"
    else
        echo "   ⚠ Plot generation failed"
    fi
else
    echo "   ⚠ Matplotlib not available (plots disabled)"
    echo "   Install with: pip3 install matplotlib numpy"
fi
echo ""

# Test complete
echo "=========================================="
echo "All tests passed!"
echo ""
echo "The calibration tools are ready to use."
echo "Run: ./calibrate_antenna_delay.sh [PORT] [DISTANCE]"
echo ""

# Cleanup test files
read -p "Clean up test files? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -f test_results.json test_plot.png
    echo "Test files removed."
fi
