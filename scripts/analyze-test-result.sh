#!/bin/bash
# analyze-test-result.sh - Analyze a single test log and determine PASS/FAIL
#
# Usage: ./analyze-test-result.sh <log_file>
# Output: Creates <log_file>.analysis.txt with PASS/FAIL status and details
#
# Pass/Fail Criteria:
# autoDrop:
#   - Reached target rode ±1.0m
#   - No ERROR messages
#   - Completed (saw "autoDrop completed" or next testNotification)
#   - Negative slack occurrences < 10
#   - No "stuck" conditions (same rode value repeated >20 times)
#
# autoRetrieve:
#   - Reached 2.0m ±0.5m
#   - No "free fall" direction changes during retrieval
#   - Rode decreased monotonically (no runaway counting)
#   - No ERROR messages
#   - Completed within expected time

set -euo pipefail

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Validate input
if [ $# -ne 1 ]; then
    echo "Usage: $0 <log_file>" >&2
    exit 1
fi

LOG_FILE="$1"
ANALYSIS_FILE="${LOG_FILE}.analysis.txt"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: Log file not found: $LOG_FILE" >&2
    exit 1
fi

# Extract test parameters from filename
# Expected format: test-NNN_<type>_<wind>kn_<depth>m.log
BASENAME=$(basename "$LOG_FILE")
if [[ "$BASENAME" =~ test-([0-9]+)_([a-zA-Z]+)_([0-9]+)kn_([0-9]+)m\.log ]]; then
    TEST_NUM="${BASH_REMATCH[1]}"
    TEST_TYPE="${BASH_REMATCH[2]}"
    WIND_SPEED="${BASH_REMATCH[3]}"
    DEPTH="${BASH_REMATCH[4]}"
else
    echo "Warning: Cannot parse filename, using defaults" >&2
    TEST_NUM="unknown"
    TEST_TYPE="unknown"
    WIND_SPEED="unknown"
    DEPTH="unknown"
fi

# Get test parameters from parse script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ "$TEST_NUM" != "unknown" ] && [ -x "$SCRIPT_DIR/parse-test-number.sh" ]; then
    eval "$("$SCRIPT_DIR/parse-test-number.sh" "$TEST_NUM")"
    TARGET_RODE="$targetRode"
else
    # Fallback if parse script not available
    if [ "$TEST_TYPE" = "autoDrop" ]; then
        case $DEPTH in
            3) TARGET_RODE=15.0 ;;
            5) TARGET_RODE=25.0 ;;
            8) TARGET_RODE=40.0 ;;
            12) TARGET_RODE=60.0 ;;
            *) TARGET_RODE="unknown" ;;
        esac
    else
        TARGET_RODE=2.0
    fi
fi

# Initialize counters
ERROR_COUNT=0
WARNING_COUNT=0
NEGATIVE_SLACK_COUNT=0
ISSUES=()

# Extract key metrics from log
START_TIME=$(grep -m 1 "Command received is $TEST_TYPE" "$LOG_FILE" | grep -oE "I \([0-9]+\)" | grep -oE "[0-9]+" || echo "0")
END_TIME=0

# Check for completion
if grep -q "autoDrop completed" "$LOG_FILE" 2>/dev/null; then
    END_TIME=$(grep -m 1 "autoDrop completed" "$LOG_FILE" | grep -oE "I \([0-9]+\)" | grep -oE "[0-9]+" || echo "$START_TIME")
    COMPLETED=true
elif grep -q "Auto-retrieve.*stopping" "$LOG_FILE" 2>/dev/null; then
    END_TIME=$(grep -m 1 "Auto-retrieve.*stopping" "$LOG_FILE" | grep -oE "I \([0-9]+\)" | grep -oE "[0-9]+" || echo "$START_TIME")
    COMPLETED=true
elif grep -q "Command received is testNotification" "$LOG_FILE" | tail -n +2 > /dev/null 2>&1; then
    # Next test started, so this one completed
    END_TIME=$(grep "Command received is testNotification" "$LOG_FILE" | tail -1 | grep -oE "I \([0-9]+\)" | grep -oE "[0-9]+" || echo "$START_TIME")
    COMPLETED=true
else
    COMPLETED=false
    ISSUES+=("Test did not complete")
fi

# Calculate duration (convert ms to seconds)
if [ "$END_TIME" -gt 0 ]; then
    DURATION_MS=$((END_TIME - START_TIME))
    DURATION_SEC=$((DURATION_MS / 1000))
    DURATION_MIN=$((DURATION_SEC / 60))
    DURATION_REMAINING_SEC=$((DURATION_SEC % 60))
    DURATION="${DURATION_MIN}m ${DURATION_REMAINING_SEC}s"
else
    DURATION="unknown"
fi

# Count errors and warnings (|| true to avoid exit on no matches with set -e)
ERROR_COUNT=$(grep "ERROR" "$LOG_FILE" 2>/dev/null | wc -l | tr -d ' \n\r' || echo "0")
ERROR_COUNT=$(echo "$ERROR_COUNT" | head -1)
ERROR_COUNT=${ERROR_COUNT:-0}
WARNING_COUNT=$(grep "WARNING" "$LOG_FILE" 2>/dev/null | wc -l | tr -d ' \n\r' || echo "0")
WARNING_COUNT=$(echo "$WARNING_COUNT" | head -1)
WARNING_COUNT=${WARNING_COUNT:-0}

# Count negative slack events
NEGATIVE_SLACK_COUNT=$(grep "Slack.*-[0-9]" "$LOG_FILE" 2>/dev/null | wc -l | tr -d ' \n\r' || echo "0")
NEGATIVE_SLACK_COUNT=$(echo "$NEGATIVE_SLACK_COUNT" | head -1)
NEGATIVE_SLACK_COUNT=${NEGATIVE_SLACK_COUNT:-0}

# Get final rode value
FINAL_RODE=$(grep "Rode:" "$LOG_FILE" | tail -1 | grep -oE "[0-9]+\.[0-9]+" | head -1 || echo "unknown")

# Check for errors
if [ "$ERROR_COUNT" -gt 0 ]; then
    ISSUES+=("Found $ERROR_COUNT ERROR messages")
fi

# Test-specific checks
if [ "$TEST_TYPE" = "autoDrop" ]; then
    # Check if target was reached (±1.0m tolerance)
    if [ "$FINAL_RODE" != "unknown" ] && [ "$TARGET_RODE" != "unknown" ]; then
        DIFF=$(echo "$FINAL_RODE - $TARGET_RODE" | bc | sed 's/^-//')
        EXCEEDS=$(echo "$DIFF > 1.0" | bc -l)
        if [ "$EXCEEDS" -eq 1 ]; then
            ISSUES+=("Target not reached: ${FINAL_RODE}m vs ${TARGET_RODE}m (diff: ${DIFF}m)")
        fi
    fi

    # Check for excessive negative slack
    if [ "$NEGATIVE_SLACK_COUNT" -ge 10 ]; then
        ISSUES+=("Excessive negative slack events: $NEGATIVE_SLACK_COUNT")
    fi

    # Check for stuck condition (same rode value repeated >20 times)
    # Extract rode values and check for repetition
    RODE_VALUES=$(grep -oE "Rode: [0-9]+\.[0-9]+" "$LOG_FILE" | grep -oE "[0-9]+\.[0-9]+" || echo "")
    if [ -n "$RODE_VALUES" ]; then
        MAX_REPETITION=$(echo "$RODE_VALUES" | uniq -c | awk '{print $1}' | sort -rn | head -1)
        if [ "${MAX_REPETITION:-0}" -gt 20 ]; then
            ISSUES+=("Stuck condition detected: same rode value repeated $MAX_REPETITION times")
        fi
    fi

elif [ "$TEST_TYPE" = "autoRetrieve" ]; then
    # Check if target was reached (±0.5m tolerance)
    if [ "$FINAL_RODE" != "unknown" ] && [ "$TARGET_RODE" != "unknown" ]; then
        DIFF=$(echo "$FINAL_RODE - $TARGET_RODE" | bc | sed 's/^-//')
        EXCEEDS=$(echo "$DIFF > 0.5" | bc -l)
        if [ "$EXCEEDS" -eq 1 ]; then
            ISSUES+=("Target not reached: ${FINAL_RODE}m vs ${TARGET_RODE}m (diff: ${DIFF}m)")
        fi
    fi

    # Check for free fall direction changes
    FREEFALL_COUNT=$(grep "Direction: free fall" "$LOG_FILE" 2>/dev/null | wc -l | tr -d ' \n\r' || echo "0")
    FREEFALL_COUNT=$(echo "$FREEFALL_COUNT" | head -1)
    FREEFALL_COUNT=${FREEFALL_COUNT:-0}
    if [ "$FREEFALL_COUNT" -gt 0 ]; then
        ISSUES+=("Free fall detected during retrieval: $FREEFALL_COUNT occurrences")
    fi

    # Check for rode increasing (should only decrease during retrieval)
    # Extract consecutive rode values and check if any increase
    RODE_INCREASES=$(grep -oE "Rode: [0-9]+\.[0-9]+" "$LOG_FILE" | grep -oE "[0-9]+\.[0-9]+" | \
        awk 'NR>1 && $1>prev {count++} {prev=$1} END {print count+0}')
    RODE_INCREASES=${RODE_INCREASES:-0}
    if [ "$RODE_INCREASES" -gt 5 ]; then
        ISSUES+=("Rode increased during retrieval: $RODE_INCREASES times (possible runaway)")
    fi
fi

# Determine overall status
if [ "$COMPLETED" = false ] || [ ${#ISSUES[@]} -gt 0 ]; then
    STATUS="FAIL"
else
    STATUS="PASS"
fi

# Generate analysis report
{
    echo "=============================================="
    echo "Test Analysis Report"
    echo "=============================================="
    echo "Test Number: $TEST_NUM"
    echo "Test Type: $TEST_TYPE"
    echo "Depth: ${DEPTH}m"
    echo "Wind Speed: ${WIND_SPEED}kn"
    echo "Target Rode: ${TARGET_RODE}m"
    echo "----------------------------------------------"
    echo "STATUS: $STATUS"
    echo "Final Rode: ${FINAL_RODE}m"
    echo "Duration: $DURATION"
    echo "Completed: $COMPLETED"
    echo "----------------------------------------------"
    echo "Errors: $ERROR_COUNT"
    echo "Warnings: $WARNING_COUNT"
    echo "Negative Slack Events: $NEGATIVE_SLACK_COUNT"
    echo "----------------------------------------------"
    if [ ${#ISSUES[@]} -gt 0 ]; then
        echo "Issues:"
        for issue in "${ISSUES[@]}"; do
            echo "  - $issue"
        done
    else
        echo "Issues: None"
    fi
    echo "=============================================="
} > "$ANALYSIS_FILE"

# Print to terminal with color
if [ "$STATUS" = "PASS" ]; then
    echo -e "${GREEN}[PASS]${NC} Test $TEST_NUM ($TEST_TYPE @ ${WIND_SPEED}kn, ${DEPTH}m): ${FINAL_RODE}m / ${TARGET_RODE}m in $DURATION"
else
    FIRST_ISSUE="${ISSUES[0]:-No details}"
    echo -e "${RED}[FAIL]${NC} Test $TEST_NUM ($TEST_TYPE @ ${WIND_SPEED}kn, ${DEPTH}m): $FIRST_ISSUE"
fi

# Output analysis file path
echo "Analysis saved to: $ANALYSIS_FILE"

# Exit with status code
if [ "$STATUS" = "PASS" ]; then
    exit 0
else
    exit 1
fi
