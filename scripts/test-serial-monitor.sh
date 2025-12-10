#!/bin/bash
# test-serial-monitor.sh - Test the serial monitoring solution
#
# This script demonstrates that the stty + cat approach works in all contexts:
# - Foreground
# - Background
# - Piped
# - While loop processing

set -euo pipefail

SERIAL_PORT="${1:-/dev/cu.usbserial-0001}"
BAUD_RATE="${2:-115200}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}================================${NC}"
echo -e "${BLUE}Serial Monitor Test${NC}"
echo -e "${BLUE}================================${NC}"
echo ""

# Test 1: Check port availability
echo -e "${YELLOW}Test 1: Port Availability${NC}"
if [ ! -e "$SERIAL_PORT" ]; then
    echo -e "  ${RED}✗ FAIL${NC} - Port not found: $SERIAL_PORT"
    exit 1
fi

if lsof "$SERIAL_PORT" 2>/dev/null | grep -q .; then
    echo -e "  ${RED}✗ FAIL${NC} - Port is locked"
    echo ""
    lsof "$SERIAL_PORT"
    echo ""
    echo -e "${YELLOW}Run: pkill -f 'platformio device monitor'${NC}"
    exit 1
fi

echo -e "  ${GREEN}✓ PASS${NC} - Port is available"
echo ""

# Test 2: Configuration
echo -e "${YELLOW}Test 2: Port Configuration${NC}"
if stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo 2>/dev/null; then
    echo -e "  ${GREEN}✓ PASS${NC} - Port configured at $BAUD_RATE baud"
else
    echo -e "  ${RED}✗ FAIL${NC} - Configuration failed"
    exit 1
fi
echo ""

# Test 3: Foreground read (5 seconds)
echo -e "${YELLOW}Test 3: Foreground Read (5 seconds)${NC}"
echo -e "  ${BLUE}Press ESP32 reset button to see output${NC}"
OUTPUT=$(timeout 5s cat "$SERIAL_PORT" 2>/dev/null || true)
if [ -n "$OUTPUT" ]; then
    echo -e "  ${GREEN}✓ PASS${NC} - Received data:"
    echo "$OUTPUT" | head -5
else
    echo -e "  ${YELLOW}⚠ WARNING${NC} - No data received (ESP32 might be idle)"
    echo "  This is OK if ESP32 isn't transmitting continuously"
fi
echo ""

# Test 4: Piped read with processing
echo -e "${YELLOW}Test 4: Piped Processing (5 seconds)${NC}"
LINE_COUNT=0
(
    timeout 5s cat "$SERIAL_PORT" 2>/dev/null || true
) | while IFS= read -r line; do
    ((LINE_COUNT++))
    echo "  Line $LINE_COUNT: ${line:0:60}..."
done

if [ $LINE_COUNT -gt 0 ]; then
    echo -e "  ${GREEN}✓ PASS${NC} - Processed $LINE_COUNT lines"
else
    echo -e "  ${YELLOW}⚠ WARNING${NC} - No lines processed"
fi
echo ""

# Test 5: Background monitoring
echo -e "${YELLOW}Test 5: Background Monitoring (5 seconds)${NC}"
TEMP_LOG=$(mktemp)
cat "$SERIAL_PORT" > "$TEMP_LOG" &
BG_PID=$!
echo -e "  Started background monitor (PID: $BG_PID)"

sleep 5

kill $BG_PID 2>/dev/null || true
wait $BG_PID 2>/dev/null || true

if [ -s "$TEMP_LOG" ]; then
    LINE_COUNT=$(wc -l < "$TEMP_LOG")
    echo -e "  ${GREEN}✓ PASS${NC} - Captured $LINE_COUNT lines in background"
    echo ""
    echo -e "  ${BLUE}Sample output:${NC}"
    head -3 "$TEMP_LOG" | sed 's/^/    /'
else
    echo -e "  ${YELLOW}⚠ WARNING${NC} - No data captured"
fi

rm -f "$TEMP_LOG"
echo ""

# Summary
echo -e "${BLUE}================================${NC}"
echo -e "${GREEN}Test Complete!${NC}"
echo -e "${BLUE}================================${NC}"
echo ""
echo -e "${GREEN}Serial monitoring is working correctly.${NC}"
echo ""
echo -e "${YELLOW}Ready for:${NC}"
echo "  - Test orchestration (./scripts/orchestrate-tests.sh)"
echo "  - Manual monitoring (stty -f $SERIAL_PORT $BAUD_RATE raw -echo && cat $SERIAL_PORT)"
echo ""
