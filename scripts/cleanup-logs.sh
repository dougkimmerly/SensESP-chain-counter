#!/bin/bash
# Log cleanup script for SensESP Chain Counter
# Keeps recent logs and analysis files, removes old device monitor logs and test runs

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LOGS_DIR="$PROJECT_ROOT/logs"

# Configuration
KEEP_DAYS=7                    # Keep logs from last N days
KEEP_LATEST_DEVICE_LOGS=2      # Keep N most recent device-monitor-*.log files
KEEP_LATEST_ANALYSIS=3         # Keep N most recent analysis markdown files

echo "=== Log Cleanup Script ==="
echo "Logs directory: $LOGS_DIR"
echo "Configuration:"
echo "  - Keep logs from last $KEEP_DAYS days"
echo "  - Keep latest $KEEP_LATEST_DEVICE_LOGS device monitor logs"
echo "  - Keep latest $KEEP_LATEST_ANALYSIS analysis reports"
echo ""

cd "$LOGS_DIR"

# Show current size
BEFORE_SIZE=$(du -sh . | cut -f1)
echo "Current size: $BEFORE_SIZE"
echo ""

# 1. Remove old device-monitor-*.log files (keep latest N)
echo "Cleaning device monitor logs..."
DEVICE_LOGS=$(ls -t device-monitor-*.log 2>/dev/null || true)
if [ -n "$DEVICE_LOGS" ]; then
    DEVICE_LOG_COUNT=$(echo "$DEVICE_LOGS" | wc -l | tr -d ' ')
    if [ "$DEVICE_LOG_COUNT" -gt "$KEEP_LATEST_DEVICE_LOGS" ]; then
        echo "  Found $DEVICE_LOG_COUNT device logs, keeping latest $KEEP_LATEST_DEVICE_LOGS"
        echo "$DEVICE_LOGS" | tail -n +$((KEEP_LATEST_DEVICE_LOGS + 1)) | while read file; do
            echo "  Removing: $file ($(du -h "$file" | cut -f1))"
            rm -f "$file"
        done
    else
        echo "  Found $DEVICE_LOG_COUNT device logs (within keep limit)"
    fi
else
    echo "  No device monitor logs found"
fi
echo ""

# 2. Remove old test-analysis-*.md files (keep latest N)
echo "Cleaning analysis reports..."
ANALYSIS_FILES=$(ls -t test-analysis-*.md 2>/dev/null || true)
if [ -n "$ANALYSIS_FILES" ]; then
    ANALYSIS_COUNT=$(echo "$ANALYSIS_FILES" | wc -l | tr -d ' ')
    if [ "$ANALYSIS_COUNT" -gt "$KEEP_LATEST_ANALYSIS" ]; then
        echo "  Found $ANALYSIS_COUNT analysis files, keeping latest $KEEP_LATEST_ANALYSIS"
        echo "$ANALYSIS_FILES" | tail -n +$((KEEP_LATEST_ANALYSIS + 1)) | while read file; do
            echo "  Removing: $file"
            rm -f "$file"
        done
    else
        echo "  Found $ANALYSIS_COUNT analysis files (within keep limit)"
    fi
else
    echo "  No analysis files found"
fi
echo ""

# 3. Remove old test-run directories (older than N days)
echo "Cleaning test-run directories..."
if [ -d "test-runs" ]; then
    OLD_TEST_RUNS=$(find test-runs -maxdepth 1 -type d -mtime +$KEEP_DAYS 2>/dev/null | grep -v "^test-runs$" || true)
    if [ -n "$OLD_TEST_RUNS" ]; then
        echo "$OLD_TEST_RUNS" | while read dir; do
            echo "  Removing: $dir"
            rm -rf "$dir"
        done
    else
        echo "  No test-run directories older than $KEEP_DAYS days"
    fi
else
    echo "  No test-runs directory found"
fi
echo ""

# 4. Remove empty log files
echo "Removing empty log files..."
EMPTY_LOGS=$(find . -maxdepth 1 -name "*.log" -type f -size 0 2>/dev/null || true)
if [ -n "$EMPTY_LOGS" ]; then
    echo "$EMPTY_LOGS" | while read file; do
        echo "  Removing empty: $file"
        rm -f "$file"
    done
else
    echo "  No empty log files found"
fi
echo ""

# Show final size
AFTER_SIZE=$(du -sh . | cut -f1)
echo "=== Cleanup Complete ==="
echo "Before: $BEFORE_SIZE"
echo "After:  $AFTER_SIZE"
echo ""

# List remaining files
echo "Remaining log files:"
ls -lh *.log 2>/dev/null || echo "  (no .log files)"
echo ""
echo "Remaining analysis files:"
ls -lh test-analysis-*.md 2>/dev/null || echo "  (no analysis files)"
