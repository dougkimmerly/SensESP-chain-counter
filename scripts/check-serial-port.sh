#!/bin/bash
# check-serial-port.sh - Diagnostic tool for serial port issues
#
# Usage: ./check-serial-port.sh [port]
# Default: /dev/cu.usbserial-0001

set -euo pipefail

SERIAL_PORT="${1:-/dev/cu.usbserial-0001}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}====================================${NC}"
echo -e "${BLUE}Serial Port Diagnostic Tool${NC}"
echo -e "${BLUE}====================================${NC}"
echo ""

# Check 1: Port exists
echo -e "${BLUE}[1/5]${NC} Checking if port exists..."
if [ -e "$SERIAL_PORT" ]; then
    echo -e "  ${GREEN}✓${NC} Port exists: $SERIAL_PORT"
    ls -la "$SERIAL_PORT"
else
    echo -e "  ${RED}✗${NC} Port not found: $SERIAL_PORT"
    echo ""
    echo -e "${YELLOW}Available USB serial ports:${NC}"
    ls -la /dev/cu.* 2>/dev/null | grep -E "(usbserial|wchusbserial)" || echo "  None found"
    exit 1
fi

echo ""

# Check 2: Port permissions
echo -e "${BLUE}[2/5]${NC} Checking port permissions..."
if [ -r "$SERIAL_PORT" ] && [ -w "$SERIAL_PORT" ]; then
    echo -e "  ${GREEN}✓${NC} Port is readable and writable"
else
    echo -e "  ${RED}✗${NC} Port permissions issue"
    echo "  Try: sudo chmod 666 $SERIAL_PORT"
    exit 1
fi

echo ""

# Check 3: Port lock status
echo -e "${BLUE}[3/5]${NC} Checking if port is locked..."
if lsof "$SERIAL_PORT" 2>/dev/null | grep -q .; then
    echo -e "  ${RED}✗${NC} Port is LOCKED by another process"
    echo ""
    echo -e "${YELLOW}Processes using $SERIAL_PORT:${NC}"
    lsof "$SERIAL_PORT" 2>/dev/null
    echo ""
    echo -e "${YELLOW}To fix:${NC}"
    echo "  1. Close VS Code serial monitor"
    echo "  2. Run: pkill -f 'platformio device monitor'"

    # Offer to kill the process
    read -p "Kill the locking process now? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        pkill -f "platformio device monitor" 2>/dev/null || true
        sleep 1
        if lsof "$SERIAL_PORT" 2>/dev/null | grep -q .; then
            echo -e "  ${RED}✗${NC} Failed to kill process. Try manually."
            exit 1
        else
            echo -e "  ${GREEN}✓${NC} Process killed successfully"
        fi
    else
        exit 1
    fi
else
    echo -e "  ${GREEN}✓${NC} Port is available (not locked)"
fi

echo ""

# Check 4: Test serial port configuration
echo -e "${BLUE}[4/5]${NC} Testing port configuration..."
if stty -f "$SERIAL_PORT" 115200 cs8 -cstopb -parenb raw -echo 2>/dev/null; then
    echo -e "  ${GREEN}✓${NC} Port configured successfully (115200 8N1)"
    echo ""
    echo -e "${YELLOW}Current settings:${NC}"
    stty -f "$SERIAL_PORT" -a | head -5
else
    echo -e "  ${RED}✗${NC} Failed to configure port"
    exit 1
fi

echo ""

# Check 5: Test reading from port
echo -e "${BLUE}[5/5]${NC} Testing data reception (5 seconds)..."
echo -e "  ${YELLOW}Press ESP32 reset button now to see boot messages${NC}"
echo ""

# Read with timeout
(
    cat "$SERIAL_PORT" &
    CAT_PID=$!
    sleep 5
    kill $CAT_PID 2>/dev/null || true
) 2>&1 | head -20

echo ""
echo ""

# Summary
echo -e "${BLUE}====================================${NC}"
echo -e "${GREEN}Diagnostic Complete${NC}"
echo -e "${BLUE}====================================${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "  1. If you saw output above, port is working!"
echo "  2. If no output, check:"
echo "     - ESP32 is powered (LED on)"
echo "     - USB cable supports data (not just power)"
echo "     - Baud rate is 115200 in your code"
echo "     - Press reset button to trigger boot output"
echo ""
echo -e "${YELLOW}To start monitoring:${NC}"
echo "  stty -f $SERIAL_PORT 115200 raw -echo && cat $SERIAL_PORT"
echo ""
echo -e "${YELLOW}Or use the orchestrator:${NC}"
echo "  ./scripts/orchestrate-tests.sh --port $SERIAL_PORT"
echo ""
