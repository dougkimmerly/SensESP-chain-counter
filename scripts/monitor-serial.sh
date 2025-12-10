#!/bin/bash
# monitor-serial.sh - Interactive serial monitor that handles Ctrl+C properly
#
# This script wraps the serial read to ensure Ctrl+C actually stops it!
#
# Usage:
#   ./monitor-serial.sh                    # Auto-detect port, monitor interactively
#   ./monitor-serial.sh --port /dev/...    # Specify port
#   ./monitor-serial.sh --timestamps       # Show timestamps

set -euo pipefail

# Default configuration
SERIAL_PORT=""
BAUD_RATE=115200
SHOW_TIMESTAMPS=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --port)
            SERIAL_PORT="$2"
            shift 2
            ;;
        --baud)
            BAUD_RATE="$2"
            shift 2
            ;;
        --timestamps)
            SHOW_TIMESTAMPS=true
            shift
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Usage: $0 [--port <device>] [--baud <rate>] [--timestamps]" >&2
            exit 1
            ;;
    esac
done

# Auto-detect serial port if not specified
if [ -z "$SERIAL_PORT" ]; then
    for port in /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/tty.usbserial-*; do
        if [ -e "$port" ]; then
            SERIAL_PORT="$port"
            break
        fi
    done

    if [ -z "$SERIAL_PORT" ]; then
        echo "Error: No serial port found" >&2
        exit 1
    fi
fi

# Validate serial port exists
if [ ! -e "$SERIAL_PORT" ]; then
    echo "Error: Serial port not found: $SERIAL_PORT" >&2
    exit 1
fi

# Check if port is locked
if lsof "$SERIAL_PORT" 2>/dev/null | grep -q .; then
    echo "Error: Serial port is in use:" >&2
    lsof "$SERIAL_PORT" 2>/dev/null || true
    echo ""
    echo "To fix, run: pkill -f 'platformio device monitor'"
    exit 1
fi

# PID tracking for cleanup
CAT_PID=""

# Cleanup function
cleanup() {
    echo ""
    echo "Stopping serial monitor..."

    # Kill the cat process if it's running
    if [ -n "$CAT_PID" ] && ps -p "$CAT_PID" > /dev/null 2>&1; then
        kill "$CAT_PID" 2>/dev/null || true
        wait "$CAT_PID" 2>/dev/null || true
    fi

    # Kill any lingering cat processes on this port
    pkill -f "cat $SERIAL_PORT" 2>/dev/null || true

    echo "Serial monitor stopped."
    exit 0
}

# Set up signal handlers
trap cleanup SIGINT SIGTERM EXIT

# Configure serial port
echo "Configuring serial port: $SERIAL_PORT @ $BAUD_RATE baud"
stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo 2>/dev/null

echo "Serial monitor running. Press Ctrl+C to stop."
echo "----------------------------------------"

# Start reading serial port
if [ "$SHOW_TIMESTAMPS" = true ]; then
    # With timestamps
    cat "$SERIAL_PORT" 2>&1 | while IFS= read -r line; do
        echo "[$(date '+%H:%M:%S')] $line"
    done &
else
    # Without timestamps
    cat "$SERIAL_PORT" 2>&1 &
fi

# Save the cat process PID
CAT_PID=$!

# Wait for the cat process
wait "$CAT_PID" 2>/dev/null || true
