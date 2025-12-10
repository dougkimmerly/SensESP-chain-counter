#!/bin/bash
# generate-test-summary.sh - Create overall summary from all test analyses
#
# Usage: ./generate-test-summary.sh <test_run_directory>
# Output: Creates summary.txt in the test run directory
#
# Summary includes:
# - Total/Pass/Fail counts
# - Success rate by test type
# - Success rate by depth
# - Success rate by wind speed
# - List of failed tests with reasons
# - Timing statistics

set -euo pipefail

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Validate input
if [ $# -ne 1 ]; then
    echo "Usage: $0 <test_run_directory>" >&2
    exit 1
fi

TEST_DIR="$1"
SUMMARY_FILE="$TEST_DIR/summary.txt"

if [ ! -d "$TEST_DIR" ]; then
    echo "Error: Directory not found: $TEST_DIR" >&2
    exit 1
fi

# Initialize counters
TOTAL=0
PASSED=0
FAILED=0
INCOMPLETE=0

# Temporary files for categorization (compatible with Bash 3.x)
TMP_DIR=$(mktemp -d)
trap "rm -rf $TMP_DIR" EXIT

mkdir -p "$TMP_DIR/types" "$TMP_DIR/depths" "$TMP_DIR/winds"

# Failed test details file
FAILED_TESTS_FILE="$TMP_DIR/failed_tests.txt"
touch "$FAILED_TESTS_FILE"

# Timing statistics
TOTAL_DURATION_SEC=0
MIN_DURATION_SEC=999999
MAX_DURATION_SEC=0

# Process all analysis files
for ANALYSIS in "$TEST_DIR"/*.analysis.txt; do
    if [ ! -f "$ANALYSIS" ]; then
        continue
    fi

    TOTAL=$((TOTAL + 1))

    # Extract key information
    STATUS=$(grep "^STATUS:" "$ANALYSIS" | awk '{print $2}' | head -1)
    TEST_TYPE=$(grep "^Test Type:" "$ANALYSIS" | awk '{print $3}' | head -1)
    DEPTH=$(grep "^Depth:" "$ANALYSIS" | awk '{print $2}' | sed 's/m//' | head -1)
    WIND=$(grep "^Wind Speed:" "$ANALYSIS" | awk '{print $3}' | sed 's/kn//' | head -1)
    DURATION=$(grep "^Duration:" "$ANALYSIS" | awk '{print $2, $3}' | head -1)
    TEST_NUM=$(grep "^Test Number:" "$ANALYSIS" | awk '{print $3}' | head -1)

    # Convert duration to seconds for statistics
    if [[ "$DURATION" =~ ([0-9]+)m\ ([0-9]+)s ]]; then
        MINS="${BASH_REMATCH[1]}"
        SECS="${BASH_REMATCH[2]}"
        DURATION_SEC=$((MINS * 60 + SECS))
        TOTAL_DURATION_SEC=$((TOTAL_DURATION_SEC + DURATION_SEC))

        if [ "$DURATION_SEC" -lt "$MIN_DURATION_SEC" ]; then
            MIN_DURATION_SEC=$DURATION_SEC
        fi
        if [ "$DURATION_SEC" -gt "$MAX_DURATION_SEC" ]; then
            MAX_DURATION_SEC=$DURATION_SEC
        fi
    fi

    # Update category totals using files
    echo "1" >> "$TMP_DIR/types/${TEST_TYPE}.total"
    echo "1" >> "$TMP_DIR/depths/${DEPTH}.total"
    echo "1" >> "$TMP_DIR/winds/${WIND}.total"

    # Update counters
    if [ "$STATUS" = "PASS" ]; then
        PASSED=$((PASSED + 1))
        echo "1" >> "$TMP_DIR/types/${TEST_TYPE}.pass"
        echo "1" >> "$TMP_DIR/depths/${DEPTH}.pass"
        echo "1" >> "$TMP_DIR/winds/${WIND}.pass"
    elif [ "$STATUS" = "FAIL" ]; then
        FAILED=$((FAILED + 1))

        # Extract failure reason
        ISSUES=$(grep -A 10 "Issues:" "$ANALYSIS" | grep "^  -" | head -1 | sed 's/^  - //')
        echo "Test $TEST_NUM ($TEST_TYPE @ ${WIND}kn, ${DEPTH}m): $ISSUES" >> "$FAILED_TESTS_FILE"
    else
        INCOMPLETE=$((INCOMPLETE + 1))
    fi
done

# Calculate success rate
if [ "$TOTAL" -gt 0 ]; then
    SUCCESS_RATE=$(echo "scale=1; $PASSED * 100 / $TOTAL" | bc)
else
    SUCCESS_RATE=0
fi

# Calculate average duration
if [ "$TOTAL" -gt 0 ] && [ "$TOTAL_DURATION_SEC" -gt 0 ]; then
    AVG_DURATION_SEC=$((TOTAL_DURATION_SEC / TOTAL))
    AVG_MINS=$((AVG_DURATION_SEC / 60))
    AVG_SECS=$((AVG_DURATION_SEC % 60))
    AVG_DURATION="${AVG_MINS}m ${AVG_SECS}s"

    MIN_MINS=$((MIN_DURATION_SEC / 60))
    MIN_SECS=$((MIN_DURATION_SEC % 60))
    MIN_DURATION="${MIN_MINS}m ${MIN_SECS}s"

    MAX_MINS=$((MAX_DURATION_SEC / 60))
    MAX_SECS=$((MAX_DURATION_SEC % 60))
    MAX_DURATION="${MAX_MINS}m ${MAX_SECS}s"
else
    AVG_DURATION="N/A"
    MIN_DURATION="N/A"
    MAX_DURATION="N/A"
fi

# Helper function to count lines in file
count_lines() {
    if [ -f "$1" ]; then
        wc -l < "$1" | tr -d ' '
    else
        echo "0"
    fi
}

# Generate summary report
{
    echo "============================================================"
    echo "SensESP Chain Counter - Test Run Summary"
    echo "============================================================"
    echo "Generated: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "Test Directory: $TEST_DIR"
    echo "============================================================"
    echo ""
    echo "OVERALL RESULTS"
    echo "------------------------------------------------------------"
    echo "Total Tests: $TOTAL / 56"
    echo "Passed: $PASSED"
    echo "Failed: $FAILED"
    echo "Incomplete: $INCOMPLETE"
    echo "Success Rate: ${SUCCESS_RATE}%"
    echo ""
    echo "============================================================"
    echo "SUCCESS RATE BY TEST TYPE"
    echo "------------------------------------------------------------"

    for TYPE_FILE in "$TMP_DIR/types"/*.total; do
        if [ -f "$TYPE_FILE" ]; then
            TYPE=$(basename "$TYPE_FILE" .total)
            TYPE_TOTAL=$(count_lines "$TYPE_FILE")
            TYPE_PASS=$(count_lines "$TMP_DIR/types/${TYPE}.pass")
            TYPE_RATE=$(echo "scale=1; $TYPE_PASS * 100 / $TYPE_TOTAL" | bc)
            printf "%-15s: %2d/%2d (%.1f%%)\n" "$TYPE" "$TYPE_PASS" "$TYPE_TOTAL" "$TYPE_RATE"
        fi
    done

    echo ""
    echo "============================================================"
    echo "SUCCESS RATE BY DEPTH"
    echo "------------------------------------------------------------"

    for DEPTH_FILE in "$TMP_DIR/depths"/*.total; do
        if [ -f "$DEPTH_FILE" ]; then
            DEPTH=$(basename "$DEPTH_FILE" .total)
            DEPTH_TOTAL=$(count_lines "$DEPTH_FILE")
            DEPTH_PASS=$(count_lines "$TMP_DIR/depths/${DEPTH}.pass")
            DEPTH_RATE=$(echo "scale=1; $DEPTH_PASS * 100 / $DEPTH_TOTAL" | bc)
            printf "%-15s: %2d/%2d (%.1f%%)\n" "${DEPTH}m" "$DEPTH_PASS" "$DEPTH_TOTAL" "$DEPTH_RATE"
        fi
    done | sort -n

    echo ""
    echo "============================================================"
    echo "SUCCESS RATE BY WIND SPEED"
    echo "------------------------------------------------------------"

    for WIND_FILE in "$TMP_DIR/winds"/*.total; do
        if [ -f "$WIND_FILE" ]; then
            WIND=$(basename "$WIND_FILE" .total)
            WIND_TOTAL=$(count_lines "$WIND_FILE")
            WIND_PASS=$(count_lines "$TMP_DIR/winds/${WIND}.pass")
            WIND_RATE=$(echo "scale=1; $WIND_PASS * 100 / $WIND_TOTAL" | bc)
            printf "%-15s: %2d/%2d (%.1f%%)\n" "${WIND}kn" "$WIND_PASS" "$WIND_TOTAL" "$WIND_RATE"
        fi
    done | sort -n

    echo ""
    echo "============================================================"
    echo "TIMING STATISTICS"
    echo "------------------------------------------------------------"
    echo "Average Duration: $AVG_DURATION"
    echo "Minimum Duration: $MIN_DURATION"
    echo "Maximum Duration: $MAX_DURATION"

    if [ "$TOTAL" -gt 0 ] && [ "$TOTAL_DURATION_SEC" -gt 0 ]; then
        TOTAL_HOURS=$((TOTAL_DURATION_SEC / 3600))
        TOTAL_MINS=$(( (TOTAL_DURATION_SEC % 3600) / 60 ))
        echo "Total Test Time: ${TOTAL_HOURS}h ${TOTAL_MINS}m"

        # Estimate remaining time if not all tests complete
        if [ "$TOTAL" -lt 56 ]; then
            REMAINING_TESTS=$((56 - TOTAL))
            EST_REMAINING_SEC=$((REMAINING_TESTS * AVG_DURATION_SEC))
            EST_HOURS=$((EST_REMAINING_SEC / 3600))
            EST_MINS=$(( (EST_REMAINING_SEC % 3600) / 60 ))
            echo "Estimated Remaining: ${EST_HOURS}h ${EST_MINS}m ($REMAINING_TESTS tests)"
        fi
    fi

    echo ""
    echo "============================================================"
    echo "FAILED TESTS"
    echo "------------------------------------------------------------"

    if [ -s "$FAILED_TESTS_FILE" ]; then
        while IFS= read -r line; do
            echo "- $line"
        done < "$FAILED_TESTS_FILE"
    else
        echo "None"
    fi

    echo ""
    echo "============================================================"
    echo "END OF SUMMARY"
    echo "============================================================"
} > "$SUMMARY_FILE"

# Print summary to terminal with colors
echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}Test Run Summary${NC}"
echo -e "${BLUE}============================================================${NC}"
echo -e "Total Tests: ${YELLOW}$TOTAL / 56${NC}"
if [ "$PASSED" -gt 0 ]; then
    echo -e "Passed: ${GREEN}$PASSED${NC}"
fi
if [ "$FAILED" -gt 0 ]; then
    echo -e "Failed: ${RED}$FAILED${NC}"
fi
if [ "$INCOMPLETE" -gt 0 ]; then
    echo -e "Incomplete: ${YELLOW}$INCOMPLETE${NC}"
fi
echo -e "Success Rate: ${YELLOW}${SUCCESS_RATE}%${NC}"
echo ""
echo -e "${BLUE}Summary saved to:${NC} $SUMMARY_FILE"
echo -e "${BLUE}============================================================${NC}"
