#!/bin/bash

# SensESP Chain Counter - Start Log Capture
# Usage: ./scripts/start-log-capture.sh <session-name>

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOGS_DIR="${PROJECT_ROOT}/logs"
PID_FILE="${LOGS_DIR}/.capture.pid"
METADATA_FILE="${LOGS_DIR}/.capture.metadata"

# Check if session name provided
if [ -z "$1" ]; then
    echo -e "${RED}Error: Session name required${NC}"
    echo "Usage: $0 <session-name>"
    echo ""
    echo "Examples:"
    echo "  $0 retrieval-test"
    echo "  $0 deployment-debug"
    echo "  $0 general"
    exit 1
fi

SESSION_NAME="$1"
BAUD_RATE="${2:-115200}"  # Default to 115200 if not specified

# Check if already capturing
if [ -f "$PID_FILE" ]; then
    OLD_PID=$(cat "$PID_FILE")
    if ps -p "$OLD_PID" > /dev/null 2>&1; then
        echo -e "${YELLOW}Warning: Log capture already running (PID: $OLD_PID)${NC}"
        echo "Stop the existing capture first: ./scripts/stop-log-capture.sh"
        exit 1
    else
        # Stale PID file, remove it
        rm -f "$PID_FILE"
        rm -f "$METADATA_FILE"
    fi
fi

# Create logs directory if it doesn't exist
mkdir -p "$LOGS_DIR"

# Generate timestamp and log filename
TIMESTAMP=$(date +%Y-%m-%d_%H-%M-%S)
LOG_FILE="${LOGS_DIR}/${TIMESTAMP}_${SESSION_NAME}.log"

# Create metadata header
HEADER="=========================================================="
HEADER="${HEADER}\nSensESP Chain Counter - Log Capture"
HEADER="${HEADER}\nStarted: $(date '+%Y-%m-%d %H:%M:%S')"
HEADER="${HEADER}\nSession: ${SESSION_NAME}"
HEADER="${HEADER}\nBaud: ${BAUD_RATE}"
HEADER="${HEADER}\n=========================================================="

# Write header to log file
echo -e "$HEADER" > "$LOG_FILE"

echo -e "${BLUE}SensESP Chain Counter - Log Capture${NC}"
echo -e "${BLUE}====================================${NC}"
echo ""
echo -e "Session:  ${GREEN}${SESSION_NAME}${NC}"
echo -e "Log file: ${GREEN}${LOG_FILE}${NC}"
echo -e "Baud:     ${GREEN}${BAUD_RATE}${NC}"
echo ""
echo -e "${YELLOW}Starting capture... (Press Ctrl+C to stop or use ./scripts/stop-log-capture.sh)${NC}"
echo ""

# Save metadata for stop script
echo "$LOG_FILE" > "$METADATA_FILE"

# Start PlatformIO monitor and capture output
# Using script to capture in background and tee for simultaneous display/logging
{
    cd "$PROJECT_ROOT"
    pio device monitor --baud "$BAUD_RATE" --raw 2>&1 | tee -a "$LOG_FILE"
} &

# Save the PID
MONITOR_PID=$!
echo "$MONITOR_PID" > "$PID_FILE"

# Wait for the process
wait "$MONITOR_PID" 2>/dev/null || true

# Cleanup on exit
rm -f "$PID_FILE"
rm -f "$METADATA_FILE"

echo ""
echo -e "${GREEN}Log capture stopped.${NC}"
echo -e "Log saved to: ${GREEN}${LOG_FILE}${NC}"
