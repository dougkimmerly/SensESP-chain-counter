#!/bin/bash
# parse-test-number.sh - Parse test number (1-56) into test parameters
#
# Usage: ./parse-test-number.sh <test_number>
# Output: testNumber=N testType=X depth=Y windSpeed=Z targetRode=A
#
# Test structure:
# - Tests 1-14: 3m depth, target 15m
# - Tests 15-28: 5m depth, target 25m
# - Tests 29-42: 8m depth, target 40m
# - Tests 43-56: 12m depth, target 60m
# - Wind pattern repeats: 1, 4, 8, 12, 18, 20, 25 kn
# - Odd test numbers = autoDrop
# - Even test numbers = autoRetrieve (target 2m)

set -euo pipefail

# Validate input
if [ $# -ne 1 ]; then
    echo "Usage: $0 <test_number>" >&2
    echo "  test_number: 1-56" >&2
    exit 1
fi

test=$1

# Validate range
if [ "$test" -lt 1 ] || [ "$test" -gt 56 ]; then
    echo "Error: Test number must be between 1 and 56" >&2
    exit 1
fi

# Determine depth block and base target
if [ "$test" -le 14 ]; then
    depth=3
    target=15.0
elif [ "$test" -le 28 ]; then
    depth=5
    target=25.0
elif [ "$test" -le 42 ]; then
    depth=8
    target=40.0
else
    depth=12
    target=60.0
fi

# Determine wind speed (pattern repeats every 14 tests)
pos=$(( (test - 1) % 14 ))
pair=$(( pos / 2 ))  # 0-6

# Wind speed array (7 values)
winds=(1 4 8 12 18 20 25)
windSpeed=${winds[$pair]}

# Determine test type (odd = autoDrop, even = autoRetrieve)
if [ $((test % 2)) -eq 1 ]; then
    testType="autoDrop"
    targetRode=$target
else
    testType="autoRetrieve"
    targetRode=2.0
fi

# Output in key=value format for easy sourcing
echo "testNumber=$test"
echo "testType=$testType"
echo "depth=$depth"
echo "windSpeed=$windSpeed"
echo "targetRode=$targetRode"

# Optional: JSON format output if --json flag is provided
if [ "${2:-}" = "--json" ]; then
    cat <<EOF
{
  "testNumber": $test,
  "testType": "$testType",
  "depth": $depth,
  "windSpeed": $windSpeed,
  "targetRode": $targetRode
}
EOF
fi
