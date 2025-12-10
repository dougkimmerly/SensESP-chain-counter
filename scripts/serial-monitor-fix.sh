#!/bin/bash
# serial-monitor-fix.sh - Robust serial monitoring for macOS ESP32
#
# This script provides reliable serial port monitoring that works in background,
# can be piped, and handles port locking issues on macOS.
#
# Usage: ./serial-monitor-fix.sh [port] [baud]
#
# Default: /dev/cu.usbserial-0001 @ 115200 baud

set -euo pipefail

# Configuration
SERIAL_PORT="${1:-/dev/cu.usbserial-0001}"
BAUD_RATE="${2:-115200}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Stopping serial monitor...${NC}" >&2
    exit 0
}

trap cleanup SIGINT SIGTERM

# Check if port exists
if [ ! -e "$SERIAL_PORT" ]; then
    echo -e "${RED}Error: Serial port not found: $SERIAL_PORT${NC}" >&2
    exit 1
fi

# Check if port is locked
if lsof "$SERIAL_PORT" 2>/dev/null | grep -q .; then
    echo -e "${RED}Error: Serial port is in use by another process${NC}" >&2
    echo -e "${YELLOW}Processes using $SERIAL_PORT:${NC}" >&2
    lsof "$SERIAL_PORT" 2>/dev/null >&2
    echo "" >&2
    echo -e "${YELLOW}Solutions:${NC}" >&2
    echo "  1. Close VS Code's serial monitor" >&2
    echo "  2. Kill the process: kill <PID>" >&2
    echo "  3. Use 'pkill -f \"platformio device monitor\"'" >&2
    exit 1
fi

# Configure serial port
# macOS uses -f instead of -F for stty
stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo 2>/dev/null

echo -e "${GREEN}Serial monitor started${NC}" >&2
echo -e "Port: ${SERIAL_PORT} @ ${BAUD_RATE} baud" >&2
echo -e "Press Ctrl+C to stop" >&2
echo -e "-----------------------------------" >&2

# Read from serial port
# Using 'cat' is simple and reliable on macOS
# It works in foreground, background, and when piped
exec cat "$SERIAL_PORT"
