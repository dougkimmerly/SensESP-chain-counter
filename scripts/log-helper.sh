#!/bin/bash

# SensESP Chain Counter - Log Helper Menu
# Quick access to common log operations

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
SCRIPTS_DIR="${PROJECT_ROOT}/scripts"
LOGS_DIR="${PROJECT_ROOT}/logs"

show_menu() {
    clear
    echo -e "${BLUE}=========================================================="
    echo -e "SensESP Chain Counter - Log Management"
    echo -e "==========================================================${NC}"
    echo ""
    echo "1. Start new log capture"
    echo "2. Stop current log capture"
    echo "3. List recent logs"
    echo "4. Analyze a log file"
    echo "5. Tail latest log (real-time)"
    echo "6. Search logs for pattern"
    echo "7. Clean old logs (interactive)"
    echo "8. Show disk usage"
    echo "9. Help"
    echo "0. Exit"
    echo ""
    echo -n "Select option: "
}

start_capture() {
    echo ""
    echo -e "${CYAN}Session types: test, debug, deploy, retrieval, general${NC}"
    echo -n "Enter session name: "
    read -r session_name

    if [ -z "$session_name" ]; then
        echo -e "${RED}Session name required${NC}"
        return
    fi

    echo -n "Baud rate [115200]: "
    read -r baud_rate
    baud_rate=${baud_rate:-115200}

    echo ""
    "${SCRIPTS_DIR}/start-log-capture.sh" "$session_name" "$baud_rate"
}

stop_capture() {
    echo ""
    "${SCRIPTS_DIR}/stop-log-capture.sh"
    echo ""
    read -p "Press Enter to continue..."
}

list_logs() {
    echo ""
    echo -n "Number of logs to show [10]: "
    read -r num_logs
    num_logs=${num_logs:-10}

    echo ""
    "${SCRIPTS_DIR}/list-logs.sh" "$num_logs"
    echo ""
    read -p "Press Enter to continue..."
}

analyze_log() {
    echo ""
    "${SCRIPTS_DIR}/list-logs.sh" 5
    echo ""
    echo -n "Enter log filename (or full path): "
    read -r log_file

    if [ -z "$log_file" ]; then
        echo -e "${RED}Log file required${NC}"
        return
    fi

    # If just filename provided, prepend logs directory
    if [[ ! "$log_file" =~ ^/ ]]; then
        log_file="${LOGS_DIR}/${log_file}"
    fi

    echo ""
    "${SCRIPTS_DIR}/analyze-log.sh" "$log_file"
    echo ""
    read -p "Press Enter to continue..."
}

tail_latest() {
    echo ""
    LATEST_LOG=$(find "$LOGS_DIR" -name "*.log" -type f | sort -r | head -1)

    if [ -z "$LATEST_LOG" ]; then
        echo -e "${YELLOW}No log files found${NC}"
        read -p "Press Enter to continue..."
        return
    fi

    echo -e "Tailing: ${GREEN}$(basename "$LATEST_LOG")${NC}"
    echo -e "${YELLOW}Press Ctrl+C to stop${NC}"
    echo ""

    tail -f "$LATEST_LOG"
}

search_logs() {
    echo ""
    echo -n "Enter search pattern (regex): "
    read -r pattern

    if [ -z "$pattern" ]; then
        echo -e "${RED}Search pattern required${NC}"
        return
    fi

    echo ""
    echo -e "${CYAN}Searching all logs for: ${pattern}${NC}"
    echo ""

    grep -i -n -H --color=always "$pattern" "$LOGS_DIR"/*.log 2>/dev/null || echo "No matches found"

    echo ""
    read -p "Press Enter to continue..."
}

clean_logs() {
    echo ""
    echo -e "${YELLOW}Clean old log files${NC}"
    echo ""
    echo "1. Delete logs older than 7 days"
    echo "2. Delete logs older than 30 days"
    echo "3. Delete all logs (keep structure)"
    echo "4. Cancel"
    echo ""
    echo -n "Select option: "
    read -r clean_option

    case $clean_option in
        1)
            echo ""
            find "$LOGS_DIR" -name "*.log" -type f -mtime +7 -ls
            echo ""
            echo -n "Delete these files? (yes/no): "
            read -r confirm
            if [ "$confirm" = "yes" ]; then
                find "$LOGS_DIR" -name "*.log" -type f -mtime +7 -delete
                echo -e "${GREEN}Old logs deleted${NC}"
            fi
            ;;
        2)
            echo ""
            find "$LOGS_DIR" -name "*.log" -type f -mtime +30 -ls
            echo ""
            echo -n "Delete these files? (yes/no): "
            read -r confirm
            if [ "$confirm" = "yes" ]; then
                find "$LOGS_DIR" -name "*.log" -type f -mtime +30 -delete
                echo -e "${GREEN}Old logs deleted${NC}"
            fi
            ;;
        3)
            echo ""
            echo -e "${RED}WARNING: This will delete ALL log files!${NC}"
            echo -n "Type 'DELETE ALL' to confirm: "
            read -r confirm
            if [ "$confirm" = "DELETE ALL" ]; then
                rm -f "$LOGS_DIR"/*.log
                echo -e "${GREEN}All logs deleted${NC}"
            else
                echo -e "${YELLOW}Cancelled${NC}"
            fi
            ;;
        *)
            echo "Cancelled"
            ;;
    esac

    echo ""
    read -p "Press Enter to continue..."
}

show_disk_usage() {
    echo ""
    echo -e "${CYAN}Disk Usage Analysis${NC}"
    echo ""

    TOTAL_SIZE=$(du -sh "$LOGS_DIR" | cut -f1)
    LOG_COUNT=$(find "$LOGS_DIR" -name "*.log" -type f | wc -l)

    echo -e "Total size:    ${GREEN}${TOTAL_SIZE}${NC}"
    echo -e "Log count:     ${GREEN}${LOG_COUNT}${NC}"
    echo ""

    echo -e "${CYAN}Largest logs:${NC}"
    find "$LOGS_DIR" -name "*.log" -type f -exec du -h {} \; | sort -rh | head -10

    echo ""
    read -p "Press Enter to continue..."
}

show_help() {
    echo ""
    echo -e "${BLUE}=========================================================="
    echo -e "Help - Log Management System"
    echo -e "==========================================================${NC}"
    echo ""
    echo "This system captures serial output from the ESP32 device"
    echo "and saves it to timestamped log files for later analysis."
    echo ""
    echo -e "${CYAN}Quick Start:${NC}"
    echo "1. Connect your ESP32 device via USB"
    echo "2. Use option 1 to start capturing logs"
    echo "3. Give your session a descriptive name"
    echo "4. The log will be saved automatically"
    echo "5. Use option 2 to stop when done"
    echo ""
    echo -e "${CYAN}Analyzing Logs:${NC}"
    echo "- Option 3: List all available logs"
    echo "- Option 4: Detailed analysis of a specific log"
    echo "- Option 5: Watch logs in real-time"
    echo "- Option 6: Search for specific patterns"
    echo ""
    echo -e "${CYAN}Command Line Usage:${NC}"
    echo "  Start:   ${SCRIPTS_DIR}/start-log-capture.sh <session-name>"
    echo "  Stop:    ${SCRIPTS_DIR}/stop-log-capture.sh"
    echo "  List:    ${SCRIPTS_DIR}/list-logs.sh"
    echo "  Analyze: ${SCRIPTS_DIR}/analyze-log.sh <log-file>"
    echo ""
    read -p "Press Enter to continue..."
}

# Main loop
while true; do
    show_menu
    read -r choice

    case $choice in
        1) start_capture ;;
        2) stop_capture ;;
        3) list_logs ;;
        4) analyze_log ;;
        5) tail_latest ;;
        6) search_logs ;;
        7) clean_logs ;;
        8) show_disk_usage ;;
        9) show_help ;;
        0)
            echo ""
            echo -e "${GREEN}Goodbye!${NC}"
            exit 0
            ;;
        *)
            echo ""
            echo -e "${RED}Invalid option${NC}"
            sleep 1
            ;;
    esac
done
