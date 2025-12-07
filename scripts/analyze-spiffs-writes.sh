#!/bin/bash
# analyze-spiffs-writes.sh - Count SPIFFS writes per test
#
# Usage: ./analyze-spiffs-writes.sh <log-file>

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Usage: $0 <log-file>" >&2
    exit 1
fi

LOG_FILE="$1"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: Log file not found: $LOG_FILE" >&2
    exit 1
fi

echo "=== SPIFFS Write Analysis ==="
echo "Log: $LOG_FILE"
echo ""

# Extract test boundaries and writes
awk '
BEGIN {
    test_num = 0
    test_name = "startup"
    write_count = 0
    total_writes = 0
}

# Match test notification
/Command received is testNotification:test/ {
    # Print previous test results
    if (test_num > 0) {
        printf "Test %2d (%-30s): %3d writes\n", test_num, test_name, write_count
        total_writes += write_count
    }

    # Parse new test info using split
    split($0, parts, ":")
    for (i in parts) {
        if (parts[i] ~ /^test[0-9]+/) {
            sub(/test/, "", parts[i])
            sub(/\/56/, "", parts[i])
            test_num = parts[i]
            test_type = parts[i+1]
            wind = parts[i+2]
            depth = parts[i+3]
            test_name = sprintf("%s %s %s", test_type, wind, depth)
            break
        }
    }
    write_count = 0
}

# Count writes (both normal and force-saved)
/Chain position saved:/ {
    write_count++
}

/Chain position force-saved:/ {
    write_count++
}

END {
    # Print last test
    if (test_num > 0) {
        printf "Test %2d (%-30s): %3d writes\n", test_num, test_name, write_count
        total_writes += write_count
    }

    print ""
    print "=== Summary ==="
    printf "Total tests: %d\n", test_num
    printf "Total writes: %d\n", total_writes
    printf "Average per test: %.1f\n", total_writes / test_num
}
' "$LOG_FILE"

# Calculate breakdown by operation type
echo ""
echo "=== Breakdown by Operation ==="

AUTODROP_WRITES=$(awk '
BEGIN { test_type = ""; write_count = 0; total = 0 }
/testNotification:test[0-9]+\/56:autoDrop:/ { test_type = "autoDrop"; write_count = 0 }
/testNotification:test[0-9]+\/56:autoRetrieve:/ {
    if (test_type == "autoDrop") {
        total += write_count
    }
    test_type = "autoRetrieve"
    write_count = 0
}
/Chain position (saved|force-saved):/ {
    if (test_type == "autoDrop") write_count++
}
END {
    if (test_type == "autoDrop") total += write_count
    print total
}
' "$LOG_FILE")

AUTORETRIEVE_WRITES=$(awk '
BEGIN { test_type = ""; write_count = 0; total = 0 }
/testNotification:test[0-9]+\/56:autoDrop:/ {
    if (test_type == "autoRetrieve") {
        total += write_count
    }
    test_type = "autoDrop"
    write_count = 0
}
/testNotification:test[0-9]+\/56:autoRetrieve:/ { test_type = "autoRetrieve"; write_count = 0 }
/Chain position (saved|force-saved):/ {
    if (test_type == "autoRetrieve") write_count++
}
END {
    if (test_type == "autoRetrieve") total += write_count
    print total
}
' "$LOG_FILE")

AUTODROP_COUNT=28
AUTORETRIEVE_COUNT=28

echo "autoDrop tests: $AUTODROP_WRITES total (avg: $(echo "scale=1; $AUTODROP_WRITES / $AUTODROP_COUNT" | bc) per test)"
echo "autoRetrieve tests: $AUTORETRIEVE_WRITES total (avg: $(echo "scale=1; $AUTORETRIEVE_WRITES / $AUTORETRIEVE_COUNT" | bc) per test)"
