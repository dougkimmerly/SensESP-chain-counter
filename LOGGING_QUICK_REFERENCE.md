# SensESP Chain Counter - Logging Quick Reference

## Quick Start Commands

### Serial Monitoring (Interactive)
```bash
# BEST: Use VS Code PlatformIO Monitor (plug icon in toolbar)
# - Ctrl+C works properly
# - Has filtering and search
# - Remember to close before running test scripts!

# ALTERNATIVE: Use screen (traditional Unix terminal)
screen /dev/cu.usbserial-0001 115200  # Exit: Ctrl+A then K

# AVOID: Direct cat (Ctrl+C doesn't work, requires pkill to stop)
```
See [SERIAL_QUICKSTART.md](SERIAL_QUICKSTART.md) for details and troubleshooting.

### Log Capture (Background)
```bash
# Interactive menu (easiest way)
./scripts/log-helper.sh

# Start log capture
./scripts/start-log-capture.sh <session-name>

# Stop log capture
./scripts/stop-log-capture.sh

# List recent logs
./scripts/list-logs.sh

# Analyze a log
./scripts/analyze-log.sh logs/<filename>
```

### Automated Test Orchestration
```bash
# Run complete test suite with auto-capture and analysis
./scripts/orchestrate-tests.sh
```

## Common Session Names

- `retrieval-test` - Testing automatic retrieval
- `deployment-test` - Testing automatic deployment
- `slack-debug` - Debugging slack calculations
- `motor-test` - Testing motor control
- `signalk-test` - Testing Signal K integration
- `general` - General operation logs

## Log File Location

All logs are saved in: `/Users/doug/Programming/dkSRC/SenseESP/SensESP-chain-counter/logs/`

Format: `YYYY-MM-DD_HH-MM-SS_<session-name>.log`

## Common Grep Patterns

```bash
# Find errors
grep -i "error" logs/*.log

# Find state changes
grep -i "state" logs/*.log

# Find motor events
grep -i "motor" logs/*.log

# Find slack calculations
grep -i "slack" logs/*.log

# Find Signal K events
grep -i "signal\|sk" logs/*.log

# Count occurrences
grep -c "pattern" logs/filename.log

# Show context (2 lines before/after)
grep -C 2 "pattern" logs/filename.log
```

## Analysis Quick Checks

```bash
# Quick error count
grep -i -c "error" logs/latest.log

# Show all warnings
grep -i "warn" logs/latest.log

# State machine transitions
grep -i "deployment.*state\|retrieval.*state" logs/latest.log

# Motor activity summary
grep -i "motor" logs/latest.log | wc -l
```

## Typical Workflow

### 1. Testing a Feature
```bash
./scripts/start-log-capture.sh feature-test
# ... run your tests ...
./scripts/stop-log-capture.sh
./scripts/analyze-log.sh logs/2024-*_feature-test.log
```

### 2. Debugging an Issue
```bash
./scripts/start-log-capture.sh bug-investigation
# ... reproduce the issue ...
./scripts/stop-log-capture.sh
grep -i "error" logs/2024-*_bug-investigation.log
```

### 3. Long-term Monitoring
```bash
./scripts/start-log-capture.sh monitoring
# Let it run...
# Press Ctrl+C when done
./scripts/analyze-log.sh logs/2024-*_monitoring.log
```

## Troubleshooting

### "No such device"
ESP32 not connected. Check with:
```bash
ls /dev/cu.* /dev/ttyUSB* 2>/dev/null
```

### "Command not found: pio"
PlatformIO not in PATH. Install or add to PATH:
```bash
export PATH=$PATH:~/.platformio/penv/bin
```

### "Permission denied"
Make scripts executable:
```bash
chmod +x scripts/*.sh
```

### Can't stop capture
Force kill:
```bash
pkill -f "pio device monitor"
rm logs/.capture.pid
```

## File Management

```bash
# Check disk usage
du -sh logs/

# List by size
ls -lhS logs/*.log

# Delete old logs (>7 days)
find logs -name "*.log" -mtime +7 -delete

# Delete all logs
rm logs/*.log
```

## Advanced Tips

### Tail a log in real-time
```bash
tail -f logs/latest.log
```

### Compare two logs
```bash
diff logs/test1.log logs/test2.log
```

### Extract specific time range
```bash
sed -n '/2024-12-06 15:30/,/2024-12-06 16:00/p' logs/filename.log
```

### Count state transitions
```bash
grep -c "DeploymentState::" logs/filename.log
grep -c "RetrievalState::" logs/filename.log
```

## What to Look For

### Successful Retrieval
- State: CHECKING_SLACK → RAISING → WAITING_FOR_SLACK → COMPLETE
- No errors
- Motor stops when slack threshold reached
- Final pull phase when close to anchor

### Successful Deployment
- State: DROP → WAIT_TIGHT → HOLD_DROP → DEPLOY_30 → etc.
- Slack increases as chain deploys
- Motor stops at appropriate slack levels

### Common Errors
- "Negative slack" - Indicates anchor drag
- "Motor timeout" - Motor ran too long without stopping
- "No depth data" - Signal K not providing depth
- "No distance data" - Signal K not providing anchor distance

## Documentation

- Full guide: `logs/README.md`
- Script docs: `scripts/README.md`
- Architecture: `docs/ARCHITECTURE_MAP.md`
- Signal K paths: `docs/SIGNALK_GUIDE.md`

## Support Files Created

```
logs/
├── .gitignore              # Excludes *.log from git
├── README.md               # Full documentation
└── *.log                   # Your log files (not in git)

scripts/
├── README.md               # Script documentation
├── start-log-capture.sh    # Start capturing
├── stop-log-capture.sh     # Stop capturing
├── list-logs.sh            # List logs with stats
├── analyze-log.sh          # Detailed analysis
├── log-helper.sh           # Interactive menu
└── test-logging-system.sh  # Verify setup
```

---

**Quick Reference Version 1.0**
Updated: 2024-12-06
