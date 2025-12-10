#!/bin/bash

# SensESP Chain Counter - Analyze Log File
# Usage: ./scripts/analyze-log.sh <log-file>

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# Check if log file provided
if [ -z "$1" ]; then
    echo -e "${RED}Error: Log file required${NC}"
    echo "Usage: $0 <log-file>"
    echo ""
    echo "Examples:"
    echo "  $0 logs/2024-12-06_15-30-45_retrieval-test.log"
    echo "  $0 logs/latest.log"
    exit 1
fi

LOG_FILE="$1"

# Check if file exists
if [ ! -f "$LOG_FILE" ]; then
    echo -e "${RED}Error: Log file not found: $LOG_FILE${NC}"
    exit 1
fi

# Get basic file info
FILENAME=$(basename "$LOG_FILE")
FILESIZE=$(du -h "$LOG_FILE" | cut -f1)
LINE_COUNT=$(wc -l < "$LOG_FILE")
MOD_TIME=$(stat -f "%Sm" -t "%Y-%m-%d %H:%M:%S" "$LOG_FILE" 2>/dev/null || stat -c "%y" "$LOG_FILE" 2>/dev/null | cut -d'.' -f1)

# Display header
echo -e "${BLUE}=========================================================="
echo -e "SensESP Chain Counter - Log Analysis"
echo -e "==========================================================${NC}"
echo -e "File:     ${GREEN}${FILENAME}${NC}"
echo -e "Size:     ${GREEN}${FILESIZE}${NC}"
echo -e "Lines:    ${GREEN}${LINE_COUNT}${NC}"
echo -e "Modified: ${GREEN}${MOD_TIME}${NC}"
echo ""

# Extract session info from header if available
SESSION_INFO=$(grep "Session:" "$LOG_FILE" 2>/dev/null | head -1 || echo "")
if [ -n "$SESSION_INFO" ]; then
    echo -e "${CYAN}${SESSION_INFO}${NC}"
fi

START_INFO=$(grep "Started:" "$LOG_FILE" 2>/dev/null | head -1 || echo "")
if [ -n "$START_INFO" ]; then
    echo -e "${CYAN}${START_INFO}${NC}"
fi

echo ""
echo -e "${BLUE}=== Event Counts ===${NC}"

# Count different event types
ERROR_COUNT=$(grep -i -E "(error|exception|fatal|critical)" "$LOG_FILE" 2>/dev/null | wc -l | tr -d ' ')
WARNING_COUNT=$(grep -i -E "warn" "$LOG_FILE" 2>/dev/null | wc -l | tr -d ' ')
INFO_COUNT=$(grep -i -E "info" "$LOG_FILE" 2>/dev/null | wc -l | tr -d ' ')
DEBUG_COUNT=$(grep -i -E "debug" "$LOG_FILE" 2>/dev/null | wc -l | tr -d ' ')

# Color code based on count
if [ "$ERROR_COUNT" -gt 0 ]; then
    echo -e "Errors:   ${RED}${ERROR_COUNT}${NC}"
else
    echo -e "Errors:   ${GREEN}${ERROR_COUNT}${NC}"
fi

if [ "$WARNING_COUNT" -gt 0 ]; then
    echo -e "Warnings: ${YELLOW}${WARNING_COUNT}${NC}"
else
    echo -e "Warnings: ${GREEN}${WARNING_COUNT}${NC}"
fi

echo -e "Info:     ${CYAN}${INFO_COUNT}${NC}"
echo -e "Debug:    ${CYAN}${DEBUG_COUNT}${NC}"

echo ""
echo -e "${BLUE}=== State Transitions ===${NC}"

# Look for deployment state changes
DEPLOY_STATES=$(grep -i "deployment.*state" "$LOG_FILE" 2>/dev/null | wc -l)
echo -e "Deployment state changes: ${GREEN}${DEPLOY_STATES}${NC}"

# Look for retrieval state changes
RETRIEVAL_STATES=$(grep -i "retrieval.*state" "$LOG_FILE" 2>/dev/null | wc -l)
echo -e "Retrieval state changes:  ${GREEN}${RETRIEVAL_STATES}${NC}"

# Look for auto stage mentions
AUTO_STAGE=$(grep -i "auto.*stage" "$LOG_FILE" 2>/dev/null | wc -l)
echo -e "Auto stage updates:       ${GREEN}${AUTO_STAGE}${NC}"

echo ""
echo -e "${BLUE}=== Chain Events ===${NC}"

# Motor activations
MOTOR_UP=$(grep -i "motor.*up\|raising\|retrieve" "$LOG_FILE" 2>/dev/null | wc -l)
MOTOR_DOWN=$(grep -i "motor.*down\|deploy\|lower" "$LOG_FILE" 2>/dev/null | wc -l)
MOTOR_STOP=$(grep -i "motor.*stop\|halt" "$LOG_FILE" 2>/dev/null | wc -l)

echo -e "Motor up events:   ${GREEN}${MOTOR_UP}${NC}"
echo -e "Motor down events: ${GREEN}${MOTOR_DOWN}${NC}"
echo -e "Motor stop events: ${GREEN}${MOTOR_STOP}${NC}"

# Slack calculations
SLACK_CALC=$(grep -i "slack" "$LOG_FILE" 2>/dev/null | wc -l)
echo -e "Slack calculations: ${GREEN}${SLACK_CALC}${NC}"

echo ""
echo -e "${BLUE}=== Signal K Events ===${NC}"

# Signal K publishes/subscribes
SK_PUBLISH=$(grep -i "publishing\|published" "$LOG_FILE" 2>/dev/null | wc -l)
SK_SUBSCRIBE=$(grep -i "subscrib" "$LOG_FILE" 2>/dev/null | wc -l)

echo -e "Signal K publishes:   ${GREEN}${SK_PUBLISH}${NC}"
echo -e "Signal K subscribes:  ${GREEN}${SK_SUBSCRIBE}${NC}"

# Depth updates
DEPTH_UPDATE=$(grep -i "depth" "$LOG_FILE" 2>/dev/null | wc -l)
echo -e "Depth updates:        ${GREEN}${DEPTH_UPDATE}${NC}"

# Distance updates
DISTANCE_UPDATE=$(grep -i "distance" "$LOG_FILE" 2>/dev/null | wc -l)
echo -e "Distance updates:     ${GREEN}${DISTANCE_UPDATE}${NC}"

echo ""
echo -e "${BLUE}=== System Events ===${NC}"

# Resets/reboots
RESET_COUNT=$(grep -i -E "reset|reboot|restart" "$LOG_FILE" 2>/dev/null | wc -l | tr -d ' ')
echo -e "Resets/Reboots:    ${YELLOW}${RESET_COUNT}${NC}"

# WiFi events
WIFI_CONNECT=$(grep -i "wifi.*connect\|connected to" "$LOG_FILE" 2>/dev/null | wc -l)
WIFI_DISCONNECT=$(grep -i "wifi.*disconnect\|lost connection" "$LOG_FILE" 2>/dev/null | wc -l)

echo -e "WiFi connects:     ${GREEN}${WIFI_CONNECT}${NC}"
echo -e "WiFi disconnects:  ${YELLOW}${WIFI_DISCONNECT}${NC}"

echo ""

# Show recent errors if any
if [ "$ERROR_COUNT" -gt 0 ]; then
    echo -e "${RED}=== Recent Errors (last 5) ===${NC}"
    grep -i -E "(error|exception|fatal|critical)" "$LOG_FILE" 2>/dev/null | tail -5
    echo ""
fi

# Show recent warnings if any
if [ "$WARNING_COUNT" -gt 0 ] && [ "$WARNING_COUNT" -lt 20 ]; then
    echo -e "${YELLOW}=== Recent Warnings ===${NC}"
    grep -i "warn" "$LOG_FILE" 2>/dev/null | tail -5
    echo ""
fi

echo -e "${BLUE}=========================================================="
echo -e "Analysis Complete"
echo -e "==========================================================${NC}"
echo ""
echo "View full log: cat $LOG_FILE"
echo "Search log:    grep -i '<pattern>' $LOG_FILE"
echo "Tail log:      tail -f $LOG_FILE"
