#!/bin/bash
# test-orchestration-system.sh - Test the orchestration system with sample data
#
# This script verifies that all components of the test orchestration system work correctly

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=========================================="
echo "Testing Test Orchestration System"
echo "=========================================="
echo ""

# Test 1: Parse test numbers
echo "[Test 1] Testing parse-test-number.sh..."
for test_num in 1 14 15 28 29 42 43 56; do
    result=$("$SCRIPT_DIR/parse-test-number.sh" "$test_num")
    echo "  Test $test_num: $(echo "$result" | grep testType | cut -d= -f2) @ $(echo "$result" | grep depth | cut -d= -f2)m"
done
echo "  ✓ parse-test-number.sh works"
echo ""

# Test 2: Create sample test logs and analysis
echo "[Test 2] Testing analyze-test-result.sh..."
TEST_DIR=$(mktemp -d)
trap "rm -rf $TEST_DIR" EXIT

# Create sample autoDrop log (PASS)
cat > "$TEST_DIR/test-001_autoDrop_1kn_3m.log" <<'EOF'
I (12345) src/main.cpp: Command received is testNotification
I (12456) src/main.cpp: testNotification: Test 1/56
I (13500) src/main.cpp: Command received is autoDrop
I (13501) ChainController: Starting autoDrop to 15.0m
I (14000) ChainController: Rode: 0.0m, Direction: deploying, Slack: 0.0m
I (20000) ChainController: Rode: 15.2m, Direction: stationary, Slack: 12.6m
I (21000) ChainController: autoDrop completed successfully at 15.2m
EOF

# Create sample autoRetrieve log (PASS)
cat > "$TEST_DIR/test-002_autoRetrieve_1kn_3m.log" <<'EOF'
I (22000) src/main.cpp: Command received is testNotification
I (23000) src/main.cpp: Command received is autoRetrieve
I (23001) RetrievalManager: Starting auto-retrieve
I (24000) ChainController: Rode: 15.2m, Direction: retrieving, Slack: 12.6m
I (30000) ChainController: Rode: 2.1m, Direction: stationary, Slack: 0.5m
I (31000) RetrievalManager: Auto-retrieve complete, stopping
EOF

# Create sample autoDrop log (FAIL - target not reached)
cat > "$TEST_DIR/test-003_autoDrop_4kn_3m.log" <<'EOF'
I (32000) src/main.cpp: Command received is testNotification
I (33000) src/main.cpp: Command received is autoDrop
I (33001) ChainController: Starting autoDrop to 15.0m
I (34000) ChainController: Rode: 0.0m, Direction: deploying, Slack: 0.0m
I (40000) ChainController: Rode: 13.2m, Direction: stationary, Slack: 10.0m
I (41000) ChainController: autoDrop completed successfully at 13.2m
EOF

# Run analysis on each log
for log in "$TEST_DIR"/*.log; do
    echo "  Analyzing: $(basename "$log")"
    "$SCRIPT_DIR/analyze-test-result.sh" "$log" || true  # Don't fail on FAIL status
done

# Check analysis files were created
ANALYSIS_COUNT=$(find "$TEST_DIR" -name "*.analysis.txt" | wc -l | tr -d ' ')
if [ "$ANALYSIS_COUNT" -eq 3 ]; then
    echo "  ✓ analyze-test-result.sh works ($ANALYSIS_COUNT analysis files created)"
else
    echo "  ✗ analyze-test-result.sh failed (expected 3, got $ANALYSIS_COUNT analysis files)"
    exit 1
fi
echo ""

# Test 3: Generate summary
echo "[Test 3] Testing generate-test-summary.sh..."
"$SCRIPT_DIR/generate-test-summary.sh" "$TEST_DIR" > /dev/null

if [ -f "$TEST_DIR/summary.txt" ]; then
    echo "  ✓ generate-test-summary.sh works"

    # Show summary stats
    TOTAL=$(grep "Total Tests:" "$TEST_DIR/summary.txt" | awk '{print $3}')
    PASSED=$(grep "^Passed:" "$TEST_DIR/summary.txt" | awk '{print $2}')
    FAILED=$(grep "^Failed:" "$TEST_DIR/summary.txt" | awk '{print $2}')
    echo "    Total: $TOTAL, Passed: $PASSED, Failed: $FAILED"
else
    echo "  ✗ generate-test-summary.sh failed (no summary.txt created)"
    exit 1
fi
echo ""

# Test 4: Verify orchestrate-tests.sh exists and is executable
echo "[Test 4] Checking orchestrate-tests.sh..."
if [ -x "$SCRIPT_DIR/orchestrate-tests.sh" ]; then
    echo "  ✓ orchestrate-tests.sh is executable"
    echo "    Run with: $SCRIPT_DIR/orchestrate-tests.sh"
else
    echo "  ✗ orchestrate-tests.sh is not executable"
    exit 1
fi
echo ""

# Test 5: Verify directory structure
echo "[Test 5] Checking directory structure..."
if [ -d "$PROJECT_DIR/logs/test-runs" ]; then
    echo "  ✓ logs/test-runs directory exists"
else
    echo "  ✗ logs/test-runs directory missing"
    exit 1
fi

if [ -f "$PROJECT_DIR/logs/test-runs/.gitignore" ]; then
    echo "  ✓ .gitignore exists"
else
    echo "  ✗ .gitignore missing"
    exit 1
fi

if [ -f "$PROJECT_DIR/logs/test-runs/README.md" ]; then
    echo "  ✓ README.md exists"
else
    echo "  ✗ README.md missing"
    exit 1
fi
echo ""

# Show example output
echo "=========================================="
echo "Example Summary Output:"
echo "=========================================="
cat "$TEST_DIR/summary.txt" | head -30
echo ""
echo "=========================================="
echo "All Tests Passed!"
echo "=========================================="
echo ""
echo "Next Steps:"
echo "1. Connect your ESP32 device"
echo "2. Run: $SCRIPT_DIR/orchestrate-tests.sh"
echo "3. Start your test simulator"
echo "4. Monitor progress with: tail -f logs/test-runs/latest/summary.txt"
echo ""
