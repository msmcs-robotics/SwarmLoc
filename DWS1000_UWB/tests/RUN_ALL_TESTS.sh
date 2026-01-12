#!/bin/bash
# Master Test Runner - Post Bug Fix Verification
# Runs all tests in sequence and generates comprehensive report

set -e

PROJECT_ROOT="/home/devel/Desktop/SwarmLoc/DWS1000_UWB"
TEST_DIR="$PROJECT_ROOT/tests"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_FILE="$PROJECT_ROOT/tests/test_outputs/MASTER_REPORT_$TIMESTAMP.md"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

mkdir -p "$PROJECT_ROOT/tests/test_outputs"

echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}DW1000 COMPREHENSIVE TEST SUITE - POST BUG FIX${NC}"
echo -e "${BLUE}============================================================${NC}"
echo ""
echo "Bug Fixed: interruptOnReceiveFailed() in DW1000.cpp"
echo "  Lines 992-996: LEN_SYS_STATUS → LEN_SYS_MASK"
echo ""
echo "Timestamp: $TIMESTAMP"
echo "Report: $REPORT_FILE"
echo ""
echo -e "${YELLOW}This will take approximately 5-6 minutes${NC}"
echo ""

# Initialize report
cat > "$REPORT_FILE" << 'HEADER'
# DW1000 Comprehensive Test Report
# Post Bug Fix Verification

## Executive Summary

**Date**: TIMESTAMP_PLACEHOLDER
**Bug Fix**: DW1000.cpp interruptOnReceiveFailed() - LEN_SYS_STATUS → LEN_SYS_MASK (lines 992-996)
**Impact**: Fixed buffer overrun in interrupt mask register that prevented all hardware interrupts from firing

## Test Results

HEADER

sed -i "s/TIMESTAMP_PLACEHOLDER/$(date)/" "$REPORT_FILE"

# Function to run test and capture result
run_test() {
    local test_name=$1
    local test_script=$2
    shift 2
    local test_args="$@"

    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}Running: $test_name${NC}"
    echo -e "${BLUE}========================================${NC}\n"

    if bash "$test_script" $test_args; then
        echo -e "\n${GREEN}✓ $test_name completed${NC}\n"
        return 0
    else
        echo -e "\n${RED}✗ $test_name failed${NC}\n"
        return 1
    fi
}

# Test 3: BasicSender
echo -e "${YELLOW}>>> Test 3: BasicSender${NC}"
if run_test "Test 3: BasicSender" "$TEST_DIR/run_test_03_sender_only.sh" /dev/ttyACM0 60; then
    TEST3_STATUS="✅ PASSED"
else
    TEST3_STATUS="❌ FAILED"
fi

echo ""
echo "Press Enter to continue to Test 4..."
read

# Test 4: Sender + Receiver
echo -e "${YELLOW}>>> Test 4: BasicSender + BasicReceiver${NC}"
if run_test "Test 4: TX/RX" "$TEST_DIR/run_test_04_tx_rx.sh" /dev/ttyACM0 /dev/ttyACM1 60; then
    TEST4_STATUS="✅ PASSED"
else
    TEST4_STATUS="❌ FAILED"
fi

echo ""
echo "Press Enter to continue to Test 6 (CRITICAL)..."
read

# Test 6: Ranging (THE CRITICAL TEST)
echo -e "${YELLOW}>>> Test 6: DW1000Ranging (CRITICAL)${NC}"
echo -e "${YELLOW}This is the most important test - proves bug fix works!${NC}"
if run_test "Test 6: Ranging" "$TEST_DIR/run_test_06_ranging.sh" /dev/ttyACM0 /dev/ttyACM1 120; then
    TEST6_STATUS="✅ PASSED"
else
    TEST6_STATUS="❌ FAILED"
fi

# Generate comprehensive report
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Generating Final Report${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Find most recent test outputs
LATEST_TEST03=$(ls -t "$PROJECT_ROOT/tests/test_outputs"/test03_sender_*.txt 2>/dev/null | head -1)
LATEST_TEST04_SENDER=$(ls -t "$PROJECT_ROOT/tests/test_outputs"/test04_sender_*.txt 2>/dev/null | head -1)
LATEST_TEST04_RECEIVER=$(ls -t "$PROJECT_ROOT/tests/test_outputs"/test04_receiver_*.txt 2>/dev/null | head -1)
LATEST_TEST06_TAG=$(ls -t "$PROJECT_ROOT/tests/test_outputs"/test06_tag_*.txt 2>/dev/null | head -1)
LATEST_TEST06_ANCHOR=$(ls -t "$PROJECT_ROOT/tests/test_outputs"/test06_anchor_*.txt 2>/dev/null | head -1)

# Analyze results
TEST03_TX_COUNT=$(grep -c "Transmit" "$LATEST_TEST03" 2>/dev/null || echo "0")
TEST04_TX_COUNT=$(grep -c "Transmitted" "$LATEST_TEST04_SENDER" 2>/dev/null || echo "0")
TEST04_RX_COUNT=$(grep -c "Received" "$LATEST_TEST04_RECEIVER" 2>/dev/null || echo "0")
TEST06_TAG_RANGES=$(grep -c "Range:" "$LATEST_TEST06_TAG" 2>/dev/null || echo "0")
TEST06_ANCHOR_RANGES=$(grep -c "Range:" "$LATEST_TEST06_ANCHOR" 2>/dev/null || echo "0")
TEST06_TOTAL_RANGES=$((TEST06_TAG_RANGES + TEST06_ANCHOR_RANGES))

# Append to report
cat >> "$REPORT_FILE" << RESULTS

### Test 3: BasicSender
**Status**: $TEST3_STATUS
**TX Events**: $TEST03_TX_COUNT
**Output**: $(basename "$LATEST_TEST03")

**Analysis**:
$(if [ "$TEST03_TX_COUNT" -gt 5 ]; then
    echo "✓ Sender transmitted multiple packets successfully"
    echo "✓ handleSent() callback is firing (proves interrupt fix)"
else
    echo "✗ Sender only transmitted initial packet or failed"
    echo "✗ Interrupts may not be working"
fi)

---

### Test 4: BasicSender + BasicReceiver
**Status**: $TEST4_STATUS
**TX Count**: $TEST04_TX_COUNT
**RX Count**: $TEST04_RX_COUNT
**Success Rate**: $(if [ "$TEST04_TX_COUNT" -gt 0 ]; then echo "scale=1; $TEST04_RX_COUNT * 100 / $TEST04_TX_COUNT" | bc; else echo "0"; fi)%

**Analysis**:
$(if [ "$TEST04_RX_COUNT" -gt 5 ]; then
    echo "✓ Receiver got packets successfully"
    echo "✓ handleReceived() callback is firing (proves interrupt fix)"
    echo "✓ TX and RX communication verified"
else
    echo "✗ No packets received or very limited reception"
fi)

**Outputs**:
- Sender: $(basename "$LATEST_TEST04_SENDER")
- Receiver: $(basename "$LATEST_TEST04_RECEIVER")

---

### Test 6: DW1000Ranging (CRITICAL TEST)
**Status**: $TEST6_STATUS
**TAG Ranges**: $TEST06_TAG_RANGES
**ANCHOR Ranges**: $TEST06_ANCHOR_RANGES
**Total Ranges**: $TEST06_TOTAL_RANGES

**Analysis**:
$(if [ "$TEST06_TOTAL_RANGES" -gt 20 ]; then
    echo "✓✓✓ EXCELLENT: Ranging protocol fully functional"
    echo "✓ Device discovery working"
    echo "✓ Two-way ranging measurements successful"
    echo "✓ Bug fix CONFIRMED SUCCESSFUL"
elif [ "$TEST06_TOTAL_RANGES" -gt 0 ]; then
    echo "⚠ PARTIAL: Some ranging but may be unstable"
else
    echo "✗✗✗ FAILURE: No ranging measurements"
    echo "✗ Bug fix may not have been applied correctly"
    echo "✗ Check library compilation"
fi)

**Outputs**:
- TAG: $(basename "$LATEST_TEST06_TAG")
- ANCHOR: $(basename "$LATEST_TEST06_ANCHOR")

---

## Before/After Comparison

### Before Bug Fix
- ❌ Interrupts not firing
- ❌ handleSent() never called
- ❌ handleReceived() never called
- ❌ Devices hung after initialization
- ❌ No ranging measurements
- ❌ Total test failures

### After Bug Fix
$(if [ "$TEST06_TOTAL_RANGES" -gt 20 ]; then
    echo "- ✅ Interrupts firing correctly"
    echo "- ✅ handleSent() callback working"
    echo "- ✅ handleReceived() callback working"
    echo "- ✅ Normal device operation"
    echo "- ✅ Ranging measurements successful"
    echo "- ✅ All communication features working"
else
    echo "- ❌ Results inconclusive or failed"
    echo "- ⚠ Further investigation needed"
fi)

---

## Overall Assessment

**Bug Fix Effectiveness**: $(if [ "$TEST06_TOTAL_RANGES" -gt 20 ]; then echo "FULLY SUCCESSFUL ✅"; elif [ "$TEST06_TOTAL_RANGES" -gt 0 ]; then echo "PARTIALLY SUCCESSFUL ⚠"; else echo "NOT SUCCESSFUL ❌"; fi)

**Key Evidence**:
- Test 3 TX count: $TEST03_TX_COUNT $(if [ "$TEST03_TX_COUNT" -gt 5 ]; then echo "✓"; else echo "✗"; fi)
- Test 4 RX count: $TEST04_RX_COUNT $(if [ "$TEST04_RX_COUNT" -gt 5 ]; then echo "✓"; else echo "✗"; fi)
- Test 6 Ranging: $TEST06_TOTAL_RANGES measurements $(if [ "$TEST06_TOTAL_RANGES" -gt 20 ]; then echo "✓"; else echo "✗"; fi)

**Conclusion**:
$(if [ "$TEST06_TOTAL_RANGES" -gt 20 ]; then
    echo "The bug fix successfully resolved the interrupt handling issue in DW1000.cpp."
    echo "All DW1000 communication features are now functional, including:"
    echo "- Basic TX/RX"
    echo "- Interrupt callbacks"
    echo "- Two-way ranging protocol"
    echo ""
    echo "The project can now proceed with full ranging and localization implementation."
else
    echo "The bug fix did not fully resolve the issue. Further debugging needed."
    echo "Recommended actions:"
    echo "1. Verify library recompilation"
    echo "2. Check hardware connections"
    echo "3. Review interrupt pin configuration"
fi)

---

## Recommendations

$(if [ "$TEST06_TOTAL_RANGES" -gt 20 ]; then
    echo "### Next Steps (Project Unblocked!)"
    echo "1. ✅ Implement multi-anchor ranging"
    echo "2. ✅ Add trilateration/multilateration"
    echo "3. ✅ Calibrate antenna delays for accuracy"
    echo "4. ✅ Test at various distances"
    echo "5. ✅ Integrate with swarm navigation"
else
    echo "### Debugging Required"
    echo "1. Verify DW1000.cpp was recompiled after fix"
    echo "2. Check IRQ pin connections (Pin 2)"
    echo "3. Test with oscilloscope/logic analyzer"
    echo "4. Consider alternative library versions"
fi)

---

## Test Output Files

All raw test outputs are saved in:
\`/home/devel/Desktop/SwarmLoc/DWS1000_UWB/tests/test_outputs/\`

Individual test files:
- Test 3: \`test03_sender_*.txt\`
- Test 4: \`test04_sender_*.txt\`, \`test04_receiver_*.txt\`
- Test 6: \`test06_tag_*.txt\`, \`test06_anchor_*.txt\`

---

**Report Generated**: $(date)
**By**: Automated Test Suite v1.0
RESULTS

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}ALL TESTS COMPLETE${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Final Report: $REPORT_FILE"
echo ""
echo "View report:"
echo "  cat $REPORT_FILE"
echo ""

# Display summary
echo -e "${BLUE}Quick Summary:${NC}"
echo "  Test 3 (Sender): $TEST3_STATUS ($TEST03_TX_COUNT TX events)"
echo "  Test 4 (TX/RX): $TEST4_STATUS ($TEST04_RX_COUNT packets received)"
echo "  Test 6 (Ranging): $TEST6_STATUS ($TEST06_TOTAL_RANGES range measurements)"
echo ""

if [ "$TEST06_TOTAL_RANGES" -gt 20 ]; then
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}🎉 SUCCESS! BUG FIX VERIFIED! 🎉${NC}"
    echo -e "${GREEN}========================================${NC}"
else
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}⚠ Results need review ⚠${NC}"
    echo -e "${RED}========================================${NC}"
fi
