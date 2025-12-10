#!/bin/bash

# SensESP Chain Counter - Stop Log Capture
# Usage: ./scripts/stop-log-capture.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOGS_DIR="${PROJECT_ROOT}/logs"
PID_FILE="${LOGS_DIR}/.capture.pid"
METADATA_FILE="${LOGS_DIR}/.capture.metadata"

# Check if capture is running
if [ ! -f "$PID_FILE" ]; then
    echo -e "${YELLOW}No active log capture found.${NC}"
    exit 0
fi

# Get the PID
CAPTURE_PID=$(cat "$PID_FILE")

# Check if process is actually running
if ! ps -p "$CAPTURE_PID" > /dev/null 2>&1; then
    echo -e "${YELLOW}Log capture process not found (stale PID file).${NC}"
    rm -f "$PID_FILE"
    rm -f "$METADATA_FILE"
    exit 0
fi

# Get log file path if available
LOG_FILE=""
if [ -f "$METADATA_FILE" ]; then
    LOG_FILE=$(cat "$METADATA_FILE")
fi

echo -e "${YELLOW}Stopping log capture (PID: $CAPTURE_PID)...${NC}"

# Kill the process tree (includes pio monitor and tee)
pkill -P "$CAPTURE_PID" 2>/dev/null || true
kill "$CAPTURE_PID" 2>/dev/null || true

# Wait a moment for cleanup
sleep 1

# Force kill if still running
if ps -p "$CAPTURE_PID" > /dev/null 2>&1; then
    kill -9 "$CAPTURE_PID" 2>/dev/null || true
fi

# Cleanup PID files
rm -f "$PID_FILE"
rm -f "$METADATA_FILE"

echo -e "${GREEN}Log capture stopped.${NC}"

if [ -n "$LOG_FILE" ] && [ -f "$LOG_FILE" ]; then
    FILE_SIZE=$(du -h "$LOG_FILE" | cut -f1)
    echo -e "Log file: ${GREEN}${LOG_FILE}${NC} (${FILE_SIZE})"
fi
