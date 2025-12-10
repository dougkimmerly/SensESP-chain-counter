#!/bin/bash
# test-serial-capture.sh - Simple serial capture test
#
# Just captures serial output to verify we can read from the ESP32
# No test detection, no analysis - just raw capture

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Configuration
SERIAL_PORT=""
BAUD_RATE=115200

# Auto-detect serial port
for port in /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/tty.usbserial-*; do
    if [ -e "$port" ]; then
        SERIAL_PORT="$port"
        break
    fi
done

if [ -z "$SERIAL_PORT" ]; then
    echo -e "${RED}Error: No serial port found${NC}" >&2
    exit 1
fi

# Check if port is locked
if lsof "$SERIAL_PORT" 2>/dev/null | grep -q .; then
    echo -e "${RED}Error: Serial port is in use:${NC}" >&2
    lsof "$SERIAL_PORT" 2>/dev/null || true
    echo ""
    echo -e "${YELLOW}To fix: pkill -f 'platformio device monitor'${NC}"
    exit 1
fi

# Create test capture directory
TIMESTAMP=$(date '+%Y-%m-%d_%H-%M-%S')
CAPTURE_DIR="$PROJECT_DIR/logs/test-captures"
mkdir -p "$CAPTURE_DIR"

LOG_FILE="$CAPTURE_DIR/${TIMESTAMP}_raw-capture.log"

# Cleanup function
cleanup() {
    echo ""
    echo -e "${YELLOW}Stopping capture...${NC}"

    # Kill any cat processes
    pkill -f "cat $SERIAL_PORT" 2>/dev/null || true

    echo -e "${GREEN}Capture saved to: $LOG_FILE${NC}"

    # Show file size
    if [ -f "$LOG_FILE" ]; then
        SIZE=$(wc -c < "$LOG_FILE" | tr -d ' ')
        LINES=$(wc -l < "$LOG_FILE" | tr -d ' ')
        echo -e "${GREEN}Captured: $LINES lines, $SIZE bytes${NC}"
    fi

    exit 0
}

# Set up signal handler
trap cleanup SIGINT SIGTERM

# Print startup banner
echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}Serial Capture Test${NC}"
echo -e "${BLUE}============================================================${NC}"
echo -e "Serial Port: ${CYAN}$SERIAL_PORT${NC}"
echo -e "Baud Rate: ${CYAN}$BAUD_RATE${NC}"
echo -e "Log File: ${CYAN}$LOG_FILE${NC}"
echo -e "${BLUE}============================================================${NC}"
echo ""
echo -e "${YELLOW}Capturing serial output... Press Ctrl+C to stop${NC}"
echo -e "${YELLOW}Lines will be displayed below:${NC}"
echo ""

# Configure serial port
stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo 2>/dev/null

# Read from serial port - save to file AND display
# Use tee to both save and display
cat "$SERIAL_PORT" 2>&1 | while IFS= read -r line; do
    # Write to log file
    echo "$line" >> "$LOG_FILE"

    # Display with timestamp
    echo -e "${CYAN}[$(date '+%H:%M:%S')]${NC} $line"
done

# If we get here, serial ended
echo -e "${YELLOW}Serial monitoring ended${NC}"
cleanup
