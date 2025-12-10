# Serial Monitoring Quick Start

## Two Critical Problems

### Problem 1: Port Locking
Your serial monitoring wasn't working because **VS Code's PlatformIO monitor had the port locked**.

Process PID 58721 was holding an exclusive lock on `/dev/cu.usbserial-0001`, preventing any other program from accessing it.

### Problem 2: Claude Can't Read Serial Directly
**Claude (the AI) cannot reliably access the serial monitor** because:
- Serial reads **block indefinitely** until data arrives
- macOS `timeout` command doesn't work reliably with device files
- Claude's tool execution model can't handle long-running interactive processes
- Background tasks get stuck waiting for data that may never come

**Bottom line:** Claude should NEVER try to read from the serial port directly - only check if it's available with `./scripts/read-serial.sh --check`

## The Solution

### 1. Close VS Code Serial Monitor

In VS Code:
- Click the trash can icon in the PlatformIO toolbar (Serial Monitor)
- Or run: `pkill -f "platformio device monitor"`

### 2. Use Our Fixed Scripts

We've updated the orchestration scripts to:
- Detect port locks automatically
- Provide clear error messages
- Use `stty + cat` instead of `pio device monitor` (more reliable on macOS)

## Quick Commands

### Check Port Status
```bash
./scripts/check-serial-port.sh
```

This diagnostic script will:
- Verify port exists
- Check for locks
- Test configuration
- Show sample output
- Offer to kill locking processes

### Monitor Serial Output (Manual)

**RECOMMENDED - Use VS Code PlatformIO Monitor:**
- Click the plug icon in PlatformIO toolbar
- Ctrl+C works properly
- Has filtering and search features

**Command Line Options (WARNING: Ctrl+C may not work!):**
```bash
# Simple monitoring
stty -f /dev/cu.usbserial-0001 115200 raw -echo && cat /dev/cu.usbserial-0001

# With timestamps
stty -f /dev/cu.usbserial-0001 115200 raw -echo && \
cat /dev/cu.usbserial-0001 | while IFS= read -r line; do \
  echo "[$(date '+%H:%M:%S')] $line"; \
done
```

**When Ctrl+C doesn't work (common), use this:**
```bash
# In another terminal, kill the process
pkill -f "monitor-serial"
pkill -f "cat /dev/cu.usbserial"

# Or if you know the PID
kill -9 <PID>
```

**Why Ctrl+C doesn't work:**
- Serial device reads block at the kernel level
- Bash signal traps don't fire until blocking calls return
- This is a macOS/Unix limitation, not a bug in our scripts

### Run Test Orchestration
```bash
./scripts/orchestrate-tests.sh
```

Now includes automatic port lock detection!

## Why It Failed Before

### Root Causes
1. **Port Locking**: Serial ports on macOS are exclusive-access
2. **PIO Monitor in Pipes**: `pio device monitor` doesn't work reliably when piped/backgrounded
3. **TTY Detection**: PIO monitor behaves differently in non-interactive contexts

### What We Fixed

**Before:**
```bash
# This fails when piped or backgrounded
pio device monitor --port "$SERIAL_PORT" --baud "$BAUD_RATE" --raw | process
```

**After:**
```bash
# This works everywhere - foreground, background, piped
stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo
cat "$SERIAL_PORT" | process
```

## Troubleshooting

### No Output From ESP32?

1. **Check power** - Power LED should be lit
2. **Press reset button** - Should see boot messages
3. **Try different USB cable** - Some are power-only
4. **Verify baud rate** - Must be 115200 in code:
   ```cpp
   Serial.begin(115200);
   ```

### Port Still Locked?

```bash
# See what's locking it
lsof /dev/cu.usbserial-0001

# Kill PlatformIO monitors
pkill -f "platformio device monitor"

# Kill all screen sessions
killall screen
```

## Best Practices

### For Claude (AI Assistant)
**DO:**
- Check port availability: `./scripts/read-serial.sh --check`
- Read existing log files: `cat logs/test-runs/latest/*.log`
- Analyze test results from saved logs
- Tell user to run `./scripts/orchestrate-tests.sh`

**DON'T:**
- Try to read serial output directly (will hang!)
- Use `pio device monitor` (blocks indefinitely)
- Use `timeout` with serial reads (doesn't work on macOS)
- Run `cat /dev/cu.usbserial-*` (blocks until data arrives)

### For Automated Testing (Scripts)
- Always use `stty + cat` (not `pio device monitor`)
- Check for port locks before starting
- Use the provided scripts (`orchestrate-tests.sh`)
- Process output line-by-line in a while loop

### For Human Interactive Development
- **BEST**: VS Code PlatformIO monitor (Ctrl+C works, but close before running scripts)
- **GOOD**: `screen /dev/cu.usbserial-0001 115200` (Ctrl+A then K to exit)
- **AVOID**: `cat` commands (Ctrl+C doesn't work, requires `pkill` to stop)

## Files Created/Updated

### Updated Files
- `/scripts/orchestrate-tests.sh` - Now uses `stty + cat` and detects port locks

### New Files
- [/scripts/monitor-serial.sh](scripts/monitor-serial.sh) - **RECOMMENDED** interactive monitor with Ctrl+C support
- [/scripts/read-serial.sh](scripts/read-serial.sh) - Port availability checker (for Claude/automation)
- [/scripts/serial-monitor-fix.sh](scripts/serial-monitor-fix.sh) - Standalone serial monitor
- [/scripts/check-serial-port.sh](scripts/check-serial-port.sh) - Diagnostic tool
- [/docs/SERIAL_MONITORING_MACOS.md](docs/SERIAL_MONITORING_MACOS.md) - Complete troubleshooting guide

## Summary

**The Golden Rules:**

1. Before running automated tests, **close VS Code's serial monitor** or any other serial terminal
2. **Ctrl+C doesn't reliably stop serial reads** - use `pkill` in another terminal to stop
3. **Claude should never read serial directly** - only check availability or read existing logs
4. **Use VS Code PlatformIO Monitor for interactive work** - it handles Ctrl+C properly

**The Best Commands:**

**For interactive monitoring:**
```bash
# Use VS Code PlatformIO Monitor (click plug icon) - Ctrl+C works!
# Or use screen (traditional Unix way)
screen /dev/cu.usbserial-0001 115200  # Ctrl+A then K to exit
```

**For automated testing:**
```bash
./scripts/orchestrate-tests.sh
```

**For quick command-line check:**
```bash
# WARNING: Ctrl+C won't work! Stop with pkill from another terminal
stty -f /dev/cu.usbserial-0001 115200 raw -echo && cat /dev/cu.usbserial-0001

# To stop from another terminal:
pkill -f "cat /dev/cu.usbserial"
```
