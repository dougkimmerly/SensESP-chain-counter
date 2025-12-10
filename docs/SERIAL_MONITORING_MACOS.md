# Serial Monitoring on macOS - Troubleshooting Guide

## The Problem

When monitoring ESP32 serial output on macOS for automated testing, several issues can occur:

1. **Port Locking**: Only one process can access a serial port at a time
2. **PIO Monitor Limitations**: `pio device monitor` detects TTY and behaves differently when piped/backgrounded
3. **Baud Rate**: Must be set explicitly before reading from the port

## Root Cause Analysis

### Why `pio device monitor` Fails in Pipes

PlatformIO's monitor is designed for interactive use and checks if it's running in a TTY:
- **Foreground/Interactive**: Works fine, handles terminal control
- **Piped/Backgrounded**: May not flush output properly, buffer differently, or exit unexpectedly
- **TTY Detection**: Changes behavior based on `isatty()` check

### Why Port Locking Occurs

On macOS/Unix, serial ports are exclusive-access devices:
- **First Process Wins**: First process to open the port locks it
- **Common Culprits**:
  - VS Code PlatformIO serial monitor
  - Arduino IDE serial monitor
  - Previous monitoring scripts that didn't clean up
  - Screen/minicom sessions

## The Solution

### Use `stty` + `cat` Instead of `pio device monitor`

This is the most reliable approach on macOS:

```bash
# 1. Configure the serial port
stty -f /dev/cu.usbserial-0001 115200 cs8 -cstopb -parenb raw -echo

# 2. Read from the port
cat /dev/cu.usbserial-0001
```

**Why this works:**
- `stty` sets baud rate and port configuration (raw mode, no echo)
- `cat` is simple and reliable - no TTY detection issues
- Works identically in foreground, background, or when piped
- Minimal overhead and buffering

### Port Configuration Explained

```bash
stty -f /dev/cu.usbserial-0001 \
  115200      # Baud rate
  cs8         # 8 data bits
  -cstopb     # 1 stop bit (- means disable 2 stop bits)
  -parenb     # No parity (- means disable)
  raw         # Raw mode (no input processing)
  -echo       # Don't echo input back
```

## Checking for Port Locks

Before starting monitoring, check if the port is in use:

```bash
lsof /dev/cu.usbserial-0001
```

Output example:
```
COMMAND     PID USER   FD   TYPE DEVICE SIZE/OFF NODE NAME
python3.1 58721 doug    3u   CHR    9,7   0t2268 6983 /dev/cu.usbserial-0001
```

This shows PlatformIO's Python monitor has the port locked.

## Solutions for Port Lock Issues

### 1. Close VS Code Serial Monitor

In VS Code with PlatformIO:
- Click the "Serial Monitor" icon in the toolbar (trash can icon)
- Or close the Serial Monitor panel
- Or close VS Code entirely

### 2. Kill PlatformIO Monitor Processes

```bash
# Kill all PlatformIO device monitor processes
pkill -f "platformio device monitor"

# Or kill specific process by PID
kill 58721
```

### 3. Check for Other Processes

```bash
# List all processes using USB serial ports
lsof /dev/cu.* | grep usbserial

# Kill all screen sessions
killall screen

# Kill all cu sessions
killall cu
```

## Complete Working Examples

### Example 1: Basic Monitoring to File

```bash
#!/bin/bash
SERIAL_PORT="/dev/cu.usbserial-0001"
BAUD_RATE=115200
LOG_FILE="serial.log"

# Configure port
stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo

# Monitor to file
cat "$SERIAL_PORT" > "$LOG_FILE"
```

### Example 2: Display and Log (tee)

```bash
#!/bin/bash
SERIAL_PORT="/dev/cu.usbserial-0001"
BAUD_RATE=115200
LOG_FILE="serial.log"

stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo
cat "$SERIAL_PORT" | tee "$LOG_FILE"
```

### Example 3: Line-by-Line Processing

```bash
#!/bin/bash
SERIAL_PORT="/dev/cu.usbserial-0001"
BAUD_RATE=115200

process_line() {
    local line="$1"
    echo "[$(date '+%H:%M:%S')] $line"

    # Example: Detect test markers
    if echo "$line" | grep -q "Test #"; then
        echo "TEST DETECTED!"
    fi
}

stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo

while IFS= read -r line; do
    process_line "$line"
done < "$SERIAL_PORT"
```

### Example 4: Background Monitoring

```bash
#!/bin/bash
SERIAL_PORT="/dev/cu.usbserial-0001"
BAUD_RATE=115200
LOG_FILE="serial.log"

# Start monitoring in background
stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo
cat "$SERIAL_PORT" > "$LOG_FILE" &
MONITOR_PID=$!

echo "Serial monitor started (PID: $MONITOR_PID)"
echo "Log file: $LOG_FILE"

# Later, to stop:
# kill $MONITOR_PID
```

## Diagnostic Checklist

When serial monitoring isn't working:

- [ ] **Port Exists**: `ls -la /dev/cu.usbserial-*`
- [ ] **Port Not Locked**: `lsof /dev/cu.usbserial-*`
- [ ] **USB Cable Connected**: Check physical connection
- [ ] **ESP32 Powered**: Check power LED on board
- [ ] **Correct Baud Rate**: Verify in platformio.ini or code
- [ ] **ESP32 Transmitting**: Try different terminal (screen/minicom) to verify
- [ ] **USB Driver**: On macOS, some ESP32 boards need CH340/CP2102 drivers

## Quick Reference Commands

```bash
# List all USB serial devices
ls -la /dev/cu.*

# Check what's using the port
lsof /dev/cu.usbserial-0001

# Kill all PlatformIO monitors
pkill -f "platformio device monitor"

# Configure and monitor (one-liner)
stty -f /dev/cu.usbserial-0001 115200 raw -echo && cat /dev/cu.usbserial-0001

# Monitor with timestamps
stty -f /dev/cu.usbserial-0001 115200 raw -echo && \
cat /dev/cu.usbserial-0001 | while IFS= read -r line; do \
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] $line"; \
done

# Background monitor with log rotation
stty -f /dev/cu.usbserial-0001 115200 raw -echo && \
cat /dev/cu.usbserial-0001 | tee -a "serial-$(date +%Y%m%d-%H%M%S).log" &
```

## Alternative Tools (Not Recommended for Automation)

While these work interactively, they're not ideal for automated testing:

### Screen
```bash
screen /dev/cu.usbserial-0001 115200
# Ctrl+A, K to exit
```

### Cu (macOS built-in)
```bash
cu -l /dev/cu.usbserial-0001 -s 115200
# ~. to exit
```

### Minicom (requires installation)
```bash
brew install minicom
minicom -D /dev/cu.usbserial-0001 -b 115200
```

**Why not for automation:**
- Interactive features (menus, special key sequences)
- Complex exit procedures
- Additional buffering layers
- Not designed for piping/redirection

## Troubleshooting No Output

If you're getting no serial output even after fixing port locks:

1. **Verify ESP32 is Running**
   ```bash
   # ESP32 should have power LED on
   # Try pressing reset button - should see boot messages
   ```

2. **Check Baud Rate Match**
   ```bash
   # In your code (main.cpp):
   Serial.begin(115200);

   # Must match stty setting
   ```

3. **Test with Screen First**
   ```bash
   screen /dev/cu.usbserial-0001 115200
   # Press ESP32 reset button
   # Should see boot messages
   # Ctrl+A, K to exit
   ```

4. **Check USB Cable**
   - Some USB cables are power-only (no data)
   - Try a different cable

5. **Verify platformio.ini**
   ```ini
   [env:esp32dev]
   monitor_speed = 115200
   upload_speed = 921600
   ```

6. **Add Debug Output**
   ```cpp
   void setup() {
     Serial.begin(115200);
     delay(2000);  // Wait for serial to initialize
     Serial.println("=== ESP32 Serial Output Started ===");
     Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
   }
   ```

## Integration with Test Orchestration

The `orchestrate-tests.sh` script now uses this approach:

```bash
# Check for port lock before starting
if lsof "$SERIAL_PORT" 2>/dev/null | grep -q .; then
    echo "Error: Serial port is in use"
    lsof "$SERIAL_PORT"
    exit 1
fi

# Configure and monitor
stty -f "$SERIAL_PORT" "$BAUD_RATE" cs8 -cstopb -parenb raw -echo
cat "$SERIAL_PORT" 2>&1 | while IFS= read -r line; do
    process_line "$line"
done
```

This ensures:
- Early detection of port conflicts
- Clear error messages with solutions
- Reliable monitoring in all contexts
- Proper cleanup on exit

## Summary

**The Golden Rule for macOS ESP32 Serial Monitoring:**

```bash
stty -f /dev/cu.usbserial-0001 115200 raw -echo && cat /dev/cu.usbserial-0001
```

This works everywhere, every time, with no surprises.
