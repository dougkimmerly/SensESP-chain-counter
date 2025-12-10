#!/bin/bash
# extract-tests-from-log.sh - Extract individual test logs from continuous capture
#
# Usage: ./extract-tests-from-log.sh <continuous_log_file> [output_dir]
#
# Scans a continuous log file for test boundaries (testNotification markers),
# extracts each test segment, and creates individual test log files compatible
# with analyze-test-result.sh
#
# Output files: test-NNN_<type>_<wind>kn_<depth>m.log

set -euo pipefail

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Validate input
if [ $# -lt 1 ]; then
    echo "Usage: $0 <continuous_log_file> [output_dir]" >&2
    echo "" >&2
    echo "Examples:" >&2
    echo "  $0 logs/device-monitor-251206-164000.log" >&2
    echo "  $0 logs/device-monitor-251206-164000.log logs/test-runs/extracted-$(date +%Y%m%d)" >&2
    exit 1
fi

LOG_FILE="$1"

# Default output directory
if [ $# -ge 2 ]; then
    OUTPUT_DIR="$2"
else
    TIMESTAMP=$(date '+%Y-%m-%d_%H-%M-%S')
    OUTPUT_DIR="$PROJECT_DIR/logs/test-runs/extracted-$TIMESTAMP"
fi

# Validate log file exists
if [ ! -f "$LOG_FILE" ]; then
    echo -e "${RED}Error: Log file not found: $LOG_FILE${NC}" >&2
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Print startup banner
echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}SensESP Chain Counter - Test Log Extraction${NC}"
echo -e "${BLUE}============================================================${NC}"
echo -e "Source Log:   ${CYAN}$LOG_FILE${NC}"
echo -e "Output Dir:   ${CYAN}$OUTPUT_DIR${NC}"
echo -e "${BLUE}============================================================${NC}"
echo ""

# Temporary working directory
WORK_DIR=$(mktemp -d)
trap "rm -rf $WORK_DIR" EXIT

# Step 1: Find all testNotification markers with line numbers
echo -e "${YELLOW}Step 1: Scanning for test boundaries...${NC}"
grep -n "Command received is testNotification" "$LOG_FILE" > "$WORK_DIR/test_markers.txt" || {
    echo -e "${RED}No test notifications found in log${NC}"
    exit 1
}

TEST_COUNT=$(wc -l < "$WORK_DIR/test_markers.txt" | tr -d ' ')
echo -e "${GREEN}Found $TEST_COUNT test notifications${NC}"
echo ""

# Step 2: Extract each test segment
echo -e "${YELLOW}Step 2: Extracting test segments...${NC}"

CURRENT_TEST_NUM=0
EXTRACTED_COUNT=0

while IFS=: read -r start_line rest; do
    CURRENT_TEST_NUM=$((CURRENT_TEST_NUM + 1))

    # Calculate end line (next testNotification or end of file)
    NEXT_LINE=$(awk -F: -v start="$start_line" 'NR>1 && $1>start {print $1; exit}' "$WORK_DIR/test_markers.txt")

    if [ -z "$NEXT_LINE" ]; then
        # This is the last test, extract to end of file
        END_LINE=$(wc -l < "$LOG_FILE" | tr -d ' ')
    else
        # Extract up to (but not including) next testNotification
        END_LINE=$((NEXT_LINE - 1))
    fi

    # Extract test segment to temp file
    TEST_SEGMENT="$WORK_DIR/test_${CURRENT_TEST_NUM}.log"
    sed -n "${start_line},${END_LINE}p" "$LOG_FILE" > "$TEST_SEGMENT"

    # Identify test type from the segment
    # Look for "Command received is autoDrop" or "Command received is autoRetrieve"
    TEST_TYPE=$(grep -m 1 "Command received is auto" "$TEST_SEGMENT" | grep -oE "auto[A-Za-z]+" || echo "unknown")

    if [ "$TEST_TYPE" = "unknown" ]; then
        echo -e "${YELLOW}  Test $CURRENT_TEST_NUM: Type unknown, skipping${NC}"
        continue
    fi

    # Get test parameters using parse-test-number.sh
    if [ -x "$SCRIPT_DIR/parse-test-number.sh" ]; then
        eval "$("$SCRIPT_DIR/parse-test-number.sh" "$CURRENT_TEST_NUM" 2>/dev/null)" || {
            echo -e "${YELLOW}  Test $CURRENT_TEST_NUM: Cannot parse parameters, using defaults${NC}"
            testType="$TEST_TYPE"
            windSpeed="0"
            depth="0"
            targetRode="0.0"
        }
    else
        # Fallback if parse script not available
        testType="$TEST_TYPE"
        windSpeed="0"
        depth="0"
        targetRode="0.0"
    fi

    # Create output filename
    OUTPUT_FILE="$OUTPUT_DIR/test-$(printf '%03d' "$CURRENT_TEST_NUM")_${testType}_${windSpeed}kn_${depth}m.log"

    # Copy segment to output
    cp "$TEST_SEGMENT" "$OUTPUT_FILE"

    # Get line count and final rode value for summary
    LINE_COUNT=$(wc -l < "$OUTPUT_FILE" | tr -d ' ')
    FINAL_RODE=$(grep "Rode:" "$OUTPUT_FILE" 2>/dev/null | tail -1 | grep -oE "[0-9]+\.[0-9]+" | head -1 || echo "?")

    echo -e "${GREEN}  ✓ Test $CURRENT_TEST_NUM${NC}: $testType @ ${windSpeed}kn, ${depth}m → ${targetRode}m (final: ${FINAL_RODE}m, $LINE_COUNT lines)"

    EXTRACTED_COUNT=$((EXTRACTED_COUNT + 1))

done < "$WORK_DIR/test_markers.txt"

echo ""
echo -e "${BLUE}============================================================${NC}"
echo -e "${GREEN}Extraction Complete${NC}"
echo -e "${BLUE}============================================================${NC}"
echo -e "Tests Extracted: ${GREEN}$EXTRACTED_COUNT${NC} / $TEST_COUNT"
echo -e "Output Directory: ${CYAN}$OUTPUT_DIR${NC}"
echo ""
echo -e "${YELLOW}Next Steps:${NC}"
echo "1. Analyze individual tests:"
echo "   cd $OUTPUT_DIR && for log in test-*.log; do $SCRIPT_DIR/analyze-test-result.sh \$log; done"
echo ""
echo "2. Generate summary:"
echo "   $SCRIPT_DIR/generate-test-summary.sh $OUTPUT_DIR"
echo ""
echo "3. View specific test:"
echo "   cat $OUTPUT_DIR/test-001_*.log"
echo ""
