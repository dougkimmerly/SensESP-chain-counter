# SensESP Chain Counter - Logs Directory

This directory contains serial output logs from the ESP32 device during testing and operation.

## Directory Structure

```
logs/
├── .gitignore              # Prevents log files from being committed
├── README.md               # This file
└── *.log                   # Timestamped log files (not committed)
```

## Log File Naming Convention

Log files follow this format:
```
YYYY-MM-DD_HH-MM-SS_<session-type>.log
```

**Session Types:**
- `test` - Testing specific features
- `debug` - Debugging sessions
- `deploy` - Deployment testing
- `retrieval` - Retrieval testing
- `general` - General operation logs

**Example:**
```
2024-12-06_15-30-45_auto-retrieval-test.log
```

## Quick Start

### Start Log Capture
```bash
# From project root
./scripts/start-log-capture.sh <session-name>

# Example:
./scripts/start-log-capture.sh retrieval-test
```

### Stop Log Capture
```bash
./scripts/stop-log-capture.sh
```

### List Recent Logs
```bash
./scripts/list-logs.sh
```

### Analyze a Log File
```bash
./scripts/analyze-log.sh logs/2024-12-06_15-30-45_retrieval-test.log
```

## Log File Structure

Each log file starts with a metadata header:

```
==========================================================
SensESP Chain Counter - Log Capture
Started: 2024-12-06 15:30:45
Session: auto-retrieve-test
Device: Auto-detected
Baud: 115200
==========================================================
```

Followed by raw serial output from the ESP32.

## File Rotation

New log files are created when:
- A new capture session starts
- An existing log file exceeds 5MB
- The device resets/restarts (detected in output)

## Tips

1. **Real-time Monitoring**: The capture scripts use `tee`, so you can watch logs in real-time while they're being saved.

2. **Finding Errors**: Use the analyze script to quickly find errors:
   ```bash
   ./scripts/analyze-log.sh logs/latest.log | grep -i error
   ```

3. **Comparing Sessions**: Keep descriptive session names to easily compare different test runs.

4. **Disk Space**: Monitor the logs directory size periodically. Logs are excluded from git but can accumulate locally.

## Troubleshooting

**No output in log file:**
- Verify the ESP32 is connected
- Check the baud rate (default: 115200)
- Ensure PlatformIO is installed and in PATH

**Permission denied:**
```bash
chmod +x scripts/*.sh
```

**Can't find device:**
```bash
# List available serial devices
ls /dev/cu.* /dev/ttyUSB* 2>/dev/null
```
