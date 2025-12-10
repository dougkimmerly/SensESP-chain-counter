#!/bin/bash
# read-serial.sh - Simple, reliable serial port reader
#
# This is the CORRECT way to read serial on macOS - don't use pio monitor!
#
# WARNING FOR CLAUDE (AI):
#   Only use --check flag! Reading serial data will HANG because:
#   - Serial reads block until data arrives (unpredictable timing)
#   - timeout command doesn't work reliably with device files on macOS
#   - Claude's tool execution can't handle long-running interactive processes
#
# Usage:
#   ./read-serial.sh --check            # ✅ SAFE - Just check if port is available
#   ./read-serial.sh                    # ❌ UNSAFE - Will block indefinitely
#   ./read-serial.sh --timeout 5        # ❌ UNSAFE - timeout doesn't work reliably
#   ./read-serial.sh --lines 50         # ❌ UNSAFE - May never get 50 lines

set -euo pipefail

# Default configuration
SERIAL_PORT=""
BAUD_RATE=115200
MAX_LINES=0
TIMEOUT=0
CHECK_ONLY=false

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
        --lines)
            MAX_LINES="$2"
            shift 2
            ;;
        --timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        --check)
            CHECK_ONLY=true
            shift
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Usage: $0 [--port <device>] [--baud <rate>] [--lines <n>] [--timeout <seconds>] [--check]" >&2
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
    exit 1
fi

# If just checking, exit successfully
if [ "$CHECK_ONLY" = true ]; then
    echo "OK: $SERIAL_PORT is available"
    exit 0
fi

# Configure serial port
stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo 2>/dev/null

# Read from serial port with optional limits
if [ "$MAX_LINES" -gt 0 ] && [ "$TIMEOUT" -gt 0 ]; then
    # Read with both line limit and timeout - whichever comes first
    timeout "$TIMEOUT" cat "$SERIAL_PORT" 2>&1 | head -n "$MAX_LINES" || true
elif [ "$MAX_LINES" -gt 0 ]; then
    # Read limited number of lines (may hang if not enough data!)
    cat "$SERIAL_PORT" 2>&1 | head -n "$MAX_LINES"
elif [ "$TIMEOUT" -gt 0 ]; then
    # Read with timeout (safest option for testing)
    timeout "$TIMEOUT" cat "$SERIAL_PORT" 2>&1 || true
else
    # Read continuously until killed
    cat "$SERIAL_PORT" 2>&1
fi