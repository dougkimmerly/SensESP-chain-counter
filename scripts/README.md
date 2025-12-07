# SensESP Chain Counter - Scripts Directory

This directory contains utility scripts for log capture and analysis.

## Quick Start

### Interactive Menu (Recommended)
```bash
./scripts/log-helper.sh
```

The interactive menu provides easy access to all logging functions.

## Individual Scripts

### 1. start-log-capture.sh
Starts capturing serial output from the ESP32 device.

**Usage:**
```bash
./scripts/start-log-capture.sh <session-name> [baud-rate]
```

**Examples:**
```bash
./scripts/start-log-capture.sh retrieval-test
./scripts/start-log-capture.sh deployment-debug 115200
./scripts/start-log-capture.sh general
```

**Features:**
- Real-time display in terminal (using `tee`)
- Automatic timestamped log file creation
- Metadata header with session info
- Background process management

**Session Name Conventions:**
- `test` - Feature testing
- `debug` - Debugging sessions
- `deploy` - Deployment testing
- `retrieval` - Retrieval testing
- `general` - General operation

### 2. stop-log-capture.sh
Stops the current log capture session.

**Usage:**
```bash
./scripts/stop-log-capture.sh
```

**Features:**
- Clean shutdown of capture process
- Displays final log file location and size
- Removes temporary PID files

### 3. list-logs.sh
Lists recent log files with statistics.

**Usage:**
```bash
./scripts/list-logs.sh [number-of-logs]
```

**Examples:**
```bash
./scripts/list-logs.sh        # Show 10 most recent
./scripts/list-logs.sh 20     # Show 20 most recent
```

**Displays:**
- Filename
- File size
- Error count (color-coded)
- Last modified time
- Total directory size
- Active capture status

### 4. analyze-log.sh
Performs detailed analysis of a log file.

**Usage:**
```bash
./scripts/analyze-log.sh <log-file>
```

**Examples:**
```bash
./scripts/analyze-log.sh logs/2024-12-06_15-30-45_retrieval-test.log
./scripts/analyze-log.sh logs/latest.log
```

**Analysis Includes:**
- File statistics (size, lines, modification time)
- Event counts (errors, warnings, info, debug)
- State transitions (deployment, retrieval)
- Chain events (motor activations, slack calculations)
- Signal K events (publishes, subscribes, updates)
- System events (resets, WiFi connections)
- Recent errors and warnings

### 5. log-helper.sh
Interactive menu for all log operations.

**Usage:**
```bash
./scripts/log-helper.sh
```

**Menu Options:**
1. Start new log capture
2. Stop current log capture
3. List recent logs
4. Analyze a log file
5. Tail latest log (real-time)
6. Search logs for pattern
7. Clean old logs (interactive)
8. Show disk usage
9. Help
0. Exit

## Common Workflows

### Testing a Feature
```bash
# Start capture with descriptive name
./scripts/start-log-capture.sh auto-retrieval-test

# ... perform tests on device ...

# Stop capture
./scripts/stop-log-capture.sh

# Analyze the results
./scripts/list-logs.sh
./scripts/analyze-log.sh logs/2024-12-06_15-30-45_auto-retrieval-test.log
```

### Debugging an Issue
```bash
# Start capture
./scripts/start-log-capture.sh slack-calculation-debug

# ... reproduce the issue ...

# Watch in real-time while capturing
# (output is already visible via tee)

# Stop and analyze
./scripts/stop-log-capture.sh
./scripts/analyze-log.sh logs/2024-12-06_16-45-30_slack-calculation-debug.log
```

### Finding Specific Events
```bash
# Search all logs for a pattern
grep -i "error" logs/*.log

# Search with line numbers and filenames
grep -n -H "slack calculation" logs/*.log

# Use the analyze script to find patterns
./scripts/analyze-log.sh logs/latest.log | grep -i "retrieval"
```

## Tips & Tricks

### Real-time Monitoring
The start-log-capture script uses `tee`, so you see output in real-time while it's being saved. No need for separate tail commands.

### Multiple Sessions
You can't run multiple capture sessions simultaneously (the script prevents this). Stop the current session before starting a new one.

### Log Rotation
New files are created for each session. If a file exceeds 5MB during capture, consider stopping and starting a new session with a descriptive name.

### Searching Logs
```bash
# Find all errors
grep -i "error" logs/*.log

# Find state transitions
grep -i "state" logs/*.log | grep -i "deployment\|retrieval"

# Count occurrences
grep -c "slack" logs/2024-12-06_15-30-45_retrieval-test.log

# Context around matches (2 lines before and after)
grep -C 2 "error" logs/latest.log
```

### Cleaning Up
```bash
# Automated cleanup (recommended)
./scripts/cleanup-logs.sh

# Manual cleanup - Remove logs older than 7 days
find logs -name "*.log" -mtime +7 -delete

# Remove all logs (keep directory)
rm logs/*.log

# Or use the interactive menu (option 7)
./scripts/log-helper.sh
```

## Troubleshooting

### "Command not found: pio"
PlatformIO is not in your PATH. Install PlatformIO or add it to PATH:
```bash
export PATH=$PATH:~/.platformio/penv/bin
```

### "Permission denied"
Make scripts executable:
```bash
chmod +x scripts/*.sh
```

### "No such device"
ESP32 not connected or wrong port. List available devices:
```bash
ls /dev/cu.* /dev/ttyUSB* 2>/dev/null
```

### Capture won't stop
Force kill:
```bash
pkill -f "pio device monitor"
rm logs/.capture.pid
```

## Log File Format

### Metadata Header
```
==========================================================
SensESP Chain Counter - Log Capture
Started: 2024-12-06 15:30:45
Session: auto-retrieve-test
Baud: 115200
==========================================================
```

### Body
Raw serial output from ESP32, including:
- System initialization messages
- State transitions
- Signal K publishes/subscribes
- Debug output
- Error messages
- Motor control commands
- Sensor readings

## Advanced Usage

### Custom Baud Rates
```bash
./scripts/start-log-capture.sh my-session 921600
```

### Piping to Other Tools
```bash
# Capture and filter in real-time
./scripts/start-log-capture.sh test | grep -i "error"

# Note: This will prevent tee from working correctly
# Better to filter the saved log file afterward
```

### Background Capture
The script already manages background processes. Just start it and continue working in another terminal.

## Directory Structure

```
scripts/
├── README.md                  # This file
├── start-log-capture.sh       # Start capture
├── stop-log-capture.sh        # Stop capture
├── list-logs.sh               # List logs
├── analyze-log.sh             # Analyze logs
├── cleanup-logs.sh            # Automated log cleanup
└── log-helper.sh              # Interactive menu
```

### 6. cleanup-logs.sh
Automated cleanup of old log files and test runs.

**Usage:**
```bash
./scripts/cleanup-logs.sh
```

**Features:**
- Keeps latest 2 device monitor logs (by default)
- Keeps latest 3 analysis markdown files
- Removes test-run directories older than 7 days
- Removes empty log files
- Shows before/after disk usage

**Configuration:**
Edit the script to adjust retention settings:
```bash
KEEP_DAYS=7                    # Keep logs from last N days
KEEP_LATEST_DEVICE_LOGS=2      # Keep N most recent device-monitor-*.log files
KEEP_LATEST_ANALYSIS=3         # Keep N most recent analysis markdown files
```

**Recommended Schedule:**
Run weekly or after major test sessions:
```bash
# Run cleanup after completing test session
./scripts/cleanup-logs.sh

# Or add to crontab for weekly cleanup
0 0 * * 0 /path/to/scripts/cleanup-logs.sh
```

## See Also

- [logs/README.md](../logs/README.md) - Log files directory documentation
- [CODEBASE_OVERVIEW.md](../docs/CODEBASE_OVERVIEW.md) - Project architecture
- [ARCHITECTURE_MAP.md](../docs/ARCHITECTURE_MAP.md) - System diagrams
