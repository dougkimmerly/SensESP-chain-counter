#!/bin/bash
# orchestrate-tests.sh - Main test orchestration script
#
# Monitors serial output, detects test boundaries, creates per-test log files,
# runs analysis after each test, and generates running summary.
#
# Usage: ./orchestrate-tests.sh [options]
#
# Options:
#   --port <device>    Serial port (default: auto-detect from platformio.ini)
#   --baud <rate>      Baud rate (default: 115200)
#   --no-analysis      Skip automatic analysis (capture logs only)
#   --help             Show this help message

set -euo pipefail

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Default configuration
SERIAL_PORT=""
BAUD_RATE=115200
RUN_ANALYSIS=true
TOTAL_TESTS=56

# Parse command line arguments
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
        --no-analysis)
            RUN_ANALYSIS=false
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --port <device>    Serial port (default: auto-detect)"
            echo "  --baud <rate>      Baud rate (default: 115200)"
            echo "  --no-analysis      Skip automatic analysis"
            echo "  --help             Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Use --help for usage information" >&2
            exit 1
            ;;
    esac
done

# Auto-detect serial port if not specified
if [ -z "$SERIAL_PORT" ]; then
    # Try to find active serial port
    for port in /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/tty.usbserial-*; do
        if [ -e "$port" ]; then
            SERIAL_PORT="$port"
            break
        fi
    done

    if [ -z "$SERIAL_PORT" ]; then
        echo -e "${RED}Error: No serial port found${NC}" >&2
        echo "Please specify port with --port option" >&2
        exit 1
    fi
fi

# Validate serial port exists
if [ ! -e "$SERIAL_PORT" ]; then
    echo -e "${RED}Error: Serial port not found: $SERIAL_PORT${NC}" >&2
    exit 1
fi

# Check if serial port is locked by another process
if lsof "$SERIAL_PORT" 2>/dev/null | grep -q .; then
    echo -e "${RED}Error: Serial port is in use by another process${NC}" >&2
    echo ""
    echo -e "${YELLOW}Processes using $SERIAL_PORT:${NC}" >&2
    lsof "$SERIAL_PORT" 2>/dev/null || true
    echo ""
    echo -e "${YELLOW}Common causes:${NC}"
    echo "  - VS Code PlatformIO serial monitor is open"
    echo "  - Another monitoring script is running"
    echo "  - Previous monitor didn't close cleanly"
    echo ""
    echo -e "${YELLOW}Solutions:${NC}"
    echo "  1. Close VS Code's serial monitor (PlatformIO toolbar)"
    echo "  2. Kill PlatformIO monitor: pkill -f 'platformio device monitor'"
    echo "  3. Kill specific process: kill <PID>"
    exit 1
fi

# Create test run directory with timestamp
TIMESTAMP=$(date '+%Y-%m-%d_%H-%M-%S')
RUN_DIR="$PROJECT_DIR/logs/test-runs/$TIMESTAMP"
mkdir -p "$RUN_DIR"

# Create symlink to latest run
LATEST_LINK="$PROJECT_DIR/logs/test-runs/latest"
rm -f "$LATEST_LINK"
ln -s "$RUN_DIR" "$LATEST_LINK"

# Log file tracking
CURRENT_LOG=""
CURRENT_TEST_NUM=0
TESTS_COMPLETED=0
TESTS_PASSED=0
TESTS_FAILED=0

# Test state tracking
TEST_IN_PROGRESS=false
TEST_START_TIME=0

# Analysis process tracking
declare -a ANALYSIS_PIDS=()

# Cleanup function
cleanup() {
    echo ""
    echo -e "${YELLOW}Shutting down...${NC}"

    # Wait for any running analyses to complete
    if [ ${#ANALYSIS_PIDS[@]} -gt 0 ]; then
        echo "Waiting for analyses to complete..."
        for pid in "${ANALYSIS_PIDS[@]}"; do
            wait "$pid" 2>/dev/null || true
        done
    fi

    # Generate final summary
    echo "Generating final summary..."
    "$SCRIPT_DIR/generate-test-summary.sh" "$RUN_DIR" || true

    echo -e "${GREEN}Test run saved to: $RUN_DIR${NC}"
    echo -e "${GREEN}Summary: $RUN_DIR/summary.txt${NC}"

    exit 0
}

# Set up signal handlers
trap cleanup SIGINT SIGTERM

# Function to start a new test log
start_test_log() {
    local test_num=$1

    # Get test parameters
    eval "$("$SCRIPT_DIR/parse-test-number.sh" "$test_num")"

    # Create log filename
    local log_file="$RUN_DIR/test-$(printf '%03d' "$test_num")_${testType}_${windSpeed}kn_${depth}m.log"

    CURRENT_LOG="$log_file"
    CURRENT_TEST_NUM="$test_num"
    TEST_IN_PROGRESS=true
    TEST_START_TIME=$(date +%s)

    # Print test start banner
    echo -e "${CYAN}[Test $test_num/$TOTAL_TESTS]${NC} $testType @ ${windSpeed}kn, ${depth}m → ${targetRode}m ... ${YELLOW}RUNNING${NC}"
}

# Function to finish current test log
finish_test_log() {
    if [ -z "$CURRENT_LOG" ]; then
        return
    fi

    TEST_IN_PROGRESS=false
    TESTS_COMPLETED=$((TESTS_COMPLETED + 1))

    # Run analysis in background if enabled
    if [ "$RUN_ANALYSIS" = true ] && [ -f "$CURRENT_LOG" ]; then
        (
            "$SCRIPT_DIR/analyze-test-result.sh" "$CURRENT_LOG" > /dev/null 2>&1
            ANALYSIS_STATUS=$?

            # Update counters in a file (since we're in a subshell)
            if [ $ANALYSIS_STATUS -eq 0 ]; then
                echo "PASS" >> "$RUN_DIR/.pass_count"
            else
                echo "FAIL" >> "$RUN_DIR/.fail_count"
            fi

            # Generate running summary
            "$SCRIPT_DIR/generate-test-summary.sh" "$RUN_DIR" > /dev/null 2>&1 || true
        ) &
        ANALYSIS_PIDS+=($!)
    fi

    CURRENT_LOG=""
}

# Function to update progress display
update_progress() {
    # Count passes and fails from files
    local passes=0
    local fails=0

    if [ -f "$RUN_DIR/.pass_count" ]; then
        passes=$(wc -l < "$RUN_DIR/.pass_count" | tr -d ' ')
    fi
    if [ -f "$RUN_DIR/.fail_count" ]; then
        fails=$(wc -l < "$RUN_DIR/.fail_count" | tr -d ' ')
    fi

    local completed=$((passes + fails))
    if [ $completed -gt 0 ]; then
        local percent=$(echo "scale=1; $completed * 100 / $TOTAL_TESTS" | bc)
        local success_rate=$(echo "scale=1; $passes * 100 / $completed" | bc)

        echo -e "${BLUE}Progress:${NC} $completed/$TOTAL_TESTS (${percent}%) | ${GREEN}Passed: $passes${NC} | ${RED}Failed: $fails${NC} | Success: ${success_rate}%"
    fi
}

# Function to process a line from serial output
process_line() {
    local line="$1"

    # Write to current log if active
    if [ -n "$CURRENT_LOG" ]; then
        echo "$line" >> "$CURRENT_LOG"
    fi

    # Check for test notification
    if echo "$line" | grep -q "Command received is testNotification"; then
        # If a test was in progress, finish it
        if [ "$TEST_IN_PROGRESS" = true ]; then
            finish_test_log
            update_progress
        fi

        # Increment test number (tests are sequential)
        CURRENT_TEST_NUM=$((CURRENT_TEST_NUM + 1))

        # Start new test log (will happen after 1-second delay in actual test)
        # We start the log immediately to capture all output
        start_test_log "$CURRENT_TEST_NUM"
    fi

    # Check for explicit completion messages
    if echo "$line" | grep -qE "autoDrop completed|Auto-retrieve.*stopping"; then
        if [ "$TEST_IN_PROGRESS" = true ]; then
            # Don't finish yet, wait for next testNotification or timeout
            # This allows us to capture any post-completion logs
            true
        fi
    fi

    # Check for timeout (test running > 10 minutes)
    if [ "$TEST_IN_PROGRESS" = true ] && [ "$TEST_START_TIME" -gt 0 ]; then
        local current_time=$(date +%s)
        local elapsed=$((current_time - TEST_START_TIME))
        if [ $elapsed -gt 600 ]; then
            echo -e "${RED}Warning: Test $CURRENT_TEST_NUM timeout (>10 minutes)${NC}"
            finish_test_log
            update_progress
        fi
    fi
}

# Print startup banner
echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}SensESP Chain Counter - Test Orchestration${NC}"
echo -e "${BLUE}============================================================${NC}"
echo -e "Started: ${CYAN}$(date '+%Y-%m-%d %H:%M:%S')${NC}"
echo -e "Serial Port: ${CYAN}$SERIAL_PORT${NC}"
echo -e "Baud Rate: ${CYAN}$BAUD_RATE${NC}"
echo -e "Run Directory: ${CYAN}$RUN_DIR${NC}"
echo -e "${BLUE}============================================================${NC}"
echo ""
echo -e "${YELLOW}Waiting for test notifications...${NC}"
echo -e "${YELLOW}Ready to capture $TOTAL_TESTS tests. Press Ctrl+C to stop.${NC}"
echo ""

# Start monitoring serial port
# IMPORTANT: On macOS, we use stty + cat instead of 'pio device monitor' because:
# 1. PIO monitor doesn't work reliably when piped/backgrounded (TTY detection)
# 2. stty + cat works in all contexts: foreground, background, piped
# 3. It's simpler and more robust for automated monitoring

# Configure serial port with proper settings
# macOS uses -f instead of -F for stty
stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo 2>/dev/null

# Read from serial port using cat (most reliable on macOS)
# Use unbuffered output with stdbuf (or gstdbuf on macOS via homebrew)
if command -v stdbuf &> /dev/null; then
    stdbuf -oL cat "$SERIAL_PORT" 2>&1 | while IFS= read -r line; do
        process_line "$line"
    done
elif command -v gstdbuf &> /dev/null; then
    gstdbuf -oL cat "$SERIAL_PORT" 2>&1 | while IFS= read -r line; do
        process_line "$line"
    done
else
    # Fallback without buffering control - may have delays
    cat "$SERIAL_PORT" 2>&1 | while IFS= read -r line; do
        process_line "$line"
    done
fi

# If we get here, serial monitoring ended (device disconnected?)
echo -e "${YELLOW}Serial monitoring ended${NC}"
cleanup
