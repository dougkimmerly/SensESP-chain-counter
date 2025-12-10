#!/bin/bash

# SensESP Chain Counter - List Log Files
# Usage: ./scripts/list-logs.sh [number-of-logs]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOGS_DIR="${PROJECT_ROOT}/logs"

# Number of logs to show (default: 10)
NUM_LOGS="${1:-10}"

# Check if logs directory exists
if [ ! -d "$LOGS_DIR" ]; then
    echo -e "${RED}Error: Logs directory not found: $LOGS_DIR${NC}"
    exit 1
fi

# Find all log files
LOG_FILES=($(find "$LOGS_DIR" -name "*.log" -type f | sort -r))

# Check if any logs exist
if [ ${#LOG_FILES[@]} -eq 0 ]; then
    echo -e "${YELLOW}No log files found in $LOGS_DIR${NC}"
    exit 0
fi

# Count total error lines across all logs
count_errors() {
    local file="$1"
    grep -i -c -E "(error|exception|fatal|critical|fail)" "$file" 2>/dev/null || echo "0"
}

# Get duration from log file
get_duration() {
    local file="$1"
    local start_line=$(grep -m 1 "Started:" "$file" 2>/dev/null || echo "")
    local last_line=$(tail -n 1 "$file" 2>/dev/null || echo "")

    if [ -z "$start_line" ] || [ -z "$last_line" ]; then
        echo "N/A"
        return
    fi

    # Try to extract timestamps and calculate duration
    # This is a simplified version - might need enhancement
    echo "N/A"
}

# Display header
echo -e "${BLUE}SensESP Chain Counter - Log Files${NC}"
echo -e "${BLUE}=================================${NC}"
echo ""
echo -e "Total logs found: ${GREEN}${#LOG_FILES[@]}${NC}"
echo -e "Showing: ${GREEN}${NUM_LOGS}${NC} most recent"
echo ""

# Display log files
printf "%-30s %-10s %-10s %-15s\n" "FILENAME" "SIZE" "ERRORS" "MODIFIED"
printf "%-30s %-10s %-10s %-15s\n" "--------" "----" "------" "--------"

count=0
for log_file in "${LOG_FILES[@]}"; do
    if [ $count -ge $NUM_LOGS ]; then
        break
    fi

    filename=$(basename "$log_file")
    filesize=$(du -h "$log_file" | cut -f1)
    error_count=$(count_errors "$log_file")
    mod_time=$(stat -f "%Sm" -t "%Y-%m-%d %H:%M" "$log_file" 2>/dev/null || stat -c "%y" "$log_file" 2>/dev/null | cut -d'.' -f1)

    # Color code by error count
    if [ "$error_count" -gt 10 ]; then
        error_color=$RED
    elif [ "$error_count" -gt 0 ]; then
        error_color=$YELLOW
    else
        error_color=$GREEN
    fi

    printf "%-30s %-10s ${error_color}%-10s${NC} %-15s\n" \
        "$filename" "$filesize" "$error_count" "$mod_time"

    ((count++))
done

echo ""

# Show total disk usage
TOTAL_SIZE=$(du -sh "$LOGS_DIR" | cut -f1)
echo -e "Total logs directory size: ${GREEN}${TOTAL_SIZE}${NC}"

# Check for active capture
PID_FILE="${LOGS_DIR}/.capture.pid"
if [ -f "$PID_FILE" ]; then
    CAPTURE_PID=$(cat "$PID_FILE")
    if ps -p "$CAPTURE_PID" > /dev/null 2>&1; then
        echo -e "\n${YELLOW}Active capture in progress (PID: $CAPTURE_PID)${NC}"
    fi
fi

echo ""
echo "Usage:"
echo "  View a log:    cat logs/<filename>"
echo "  Analyze a log: ./scripts/analyze-log.sh logs/<filename>"
echo "  Show more:     $0 <number>"
