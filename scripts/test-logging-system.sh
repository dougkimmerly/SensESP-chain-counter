#!/bin/bash

# SensESP Chain Counter - Test Logging System
# Verifies that the logging infrastructure is properly set up

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOGS_DIR="${PROJECT_ROOT}/logs"
SCRIPTS_DIR="${PROJECT_ROOT}/scripts"

echo -e "${BLUE}=========================================================="
echo -e "Testing SensESP Chain Counter Logging System"
echo -e "==========================================================${NC}"
echo ""

TESTS_PASSED=0
TESTS_FAILED=0

# Test function
test_item() {
    local description="$1"
    local test_command="$2"

    echo -n "Testing: $description... "

    if eval "$test_command" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        ((TESTS_FAILED++))
        return 1
    fi
}

# Test directory structure
echo -e "${CYAN}Directory Structure${NC}"
test_item "logs directory exists" "[ -d '$LOGS_DIR' ]"
test_item "scripts directory exists" "[ -d '$SCRIPTS_DIR' ]"
test_item "logs/.gitignore exists" "[ -f '$LOGS_DIR/.gitignore' ]"
test_item "logs/README.md exists" "[ -f '$LOGS_DIR/README.md' ]"
echo ""

# Test script files
echo -e "${CYAN}Script Files${NC}"
test_item "start-log-capture.sh exists" "[ -f '$SCRIPTS_DIR/start-log-capture.sh' ]"
test_item "stop-log-capture.sh exists" "[ -f '$SCRIPTS_DIR/stop-log-capture.sh' ]"
test_item "list-logs.sh exists" "[ -f '$SCRIPTS_DIR/list-logs.sh' ]"
test_item "analyze-log.sh exists" "[ -f '$SCRIPTS_DIR/analyze-log.sh' ]"
test_item "log-helper.sh exists" "[ -f '$SCRIPTS_DIR/log-helper.sh' ]"
test_item "scripts/README.md exists" "[ -f '$SCRIPTS_DIR/README.md' ]"
echo ""

# Test executable permissions
echo -e "${CYAN}Executable Permissions${NC}"
test_item "start-log-capture.sh is executable" "[ -x '$SCRIPTS_DIR/start-log-capture.sh' ]"
test_item "stop-log-capture.sh is executable" "[ -x '$SCRIPTS_DIR/stop-log-capture.sh' ]"
test_item "list-logs.sh is executable" "[ -x '$SCRIPTS_DIR/list-logs.sh' ]"
test_item "analyze-log.sh is executable" "[ -x '$SCRIPTS_DIR/analyze-log.sh' ]"
test_item "log-helper.sh is executable" "[ -x '$SCRIPTS_DIR/log-helper.sh' ]"
echo ""

# Test .gitignore configuration
echo -e "${CYAN}Git Configuration${NC}"
test_item "main .gitignore has logs entries" "grep -q 'logs/\*\.log' '$PROJECT_ROOT/.gitignore'"
test_item "logs/.gitignore ignores *.log" "grep -q '\*\.log' '$LOGS_DIR/.gitignore'"
echo ""

# Test script syntax (basic check)
echo -e "${CYAN}Script Syntax${NC}"
test_item "start-log-capture.sh syntax" "bash -n '$SCRIPTS_DIR/start-log-capture.sh'"
test_item "stop-log-capture.sh syntax" "bash -n '$SCRIPTS_DIR/stop-log-capture.sh'"
test_item "list-logs.sh syntax" "bash -n '$SCRIPTS_DIR/list-logs.sh'"
test_item "analyze-log.sh syntax" "bash -n '$SCRIPTS_DIR/analyze-log.sh'"
test_item "log-helper.sh syntax" "bash -n '$SCRIPTS_DIR/log-helper.sh'"
echo ""

# Test help/usage messages
echo -e "${CYAN}Script Help Messages${NC}"
test_item "start-log-capture.sh shows usage" "'$SCRIPTS_DIR/start-log-capture.sh' 2>&1 | grep -q 'Usage'"
test_item "analyze-log.sh shows usage" "'$SCRIPTS_DIR/analyze-log.sh' 2>&1 | grep -q 'Usage'"
echo ""

# Summary
echo -e "${BLUE}=========================================================="
echo -e "Test Summary"
echo -e "==========================================================${NC}"
echo -e "Passed: ${GREEN}${TESTS_PASSED}${NC}"
echo -e "Failed: ${RED}${TESTS_FAILED}${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed! Logging system is ready to use.${NC}"
    echo ""
    echo "Quick start:"
    echo "  1. Interactive menu: ./scripts/log-helper.sh"
    echo "  2. Start capture:    ./scripts/start-log-capture.sh <session-name>"
    echo "  3. Stop capture:     ./scripts/stop-log-capture.sh"
    echo "  4. List logs:        ./scripts/list-logs.sh"
    echo ""
    exit 0
else
    echo -e "${RED}Some tests failed. Please review the errors above.${NC}"
    exit 1
fi
