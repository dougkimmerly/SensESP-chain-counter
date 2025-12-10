# /debug - Issue Diagnosis

> Use when diagnosing issues, analyzing serial logs, or investigating unexpected behavior.

## Workflow

### Phase 1: Gather Information

1. **Get symptom description** - What's not working?

2. **Connect serial monitor:**
   ```bash
   pio device monitor --baud 115200
   ```

3. **Check build status:**
   ```bash
   pio run
   ```

4. **Check SignalK data** (from another device):
   ```bash
   curl http://signalk-server/signalk/v1/api/vessels/self/navigation/anchor
   ```

### Phase 2: Identify Category

| Symptom | Likely Cause | Check |
|---------|--------------|-------|
| Won't build | Syntax error | Build output |
| Won't upload | Port/connection | USB cable, port |
| Motor doesn't move | Relay/wiring | GPIO pins, power |
| Wrong direction | Pin swap | DOWN=12, UP=13 |
| Chain count wrong | Calibration | Gypsy circumference |
| Slack always 0 | No depth data | SignalK subscription |
| Negative slack | GPS/depth issue | Sensor values |
| Timeout errors | Speed calibration | Stored speeds |

### Phase 3: Investigate

**Read safety context first:**
- `.claude/context/safety.md` - Known failure modes

**For motor issues:**
- Check relay pins (GPIO 12/13)
- Check `isActive()` state
- Look for timeout messages

**For physics issues:**
- Check depth listener value
- Check distance listener value
- Verify BOW_HEIGHT = 2.0m

**For state machine issues:**
- Check current stage
- Look for stage transition logs
- Verify trigger conditions

### Phase 4: Test Hypothesis

1. **Add debug logging:**
   ```cpp
   Serial.printf("[DEBUG] state=%d target=%.2f current=%.2f\n",
                 state_, target_, current);
   ```

2. **Rebuild and upload:**
   ```bash
   pio run -t upload
   ```

3. **Monitor output:**
   ```bash
   pio device monitor --baud 115200
   ```

4. **Trigger the issue** and observe logs

### Phase 5: Fix & Verify

1. Make minimal fix
2. Rebuild and upload
3. Confirm issue resolved
4. Remove debug logging (if temporary)
5. Document root cause

---

## Common Issues Quick Reference

### Motor Won't Move

```cpp
// Check in serial monitor:
// 1. Is command received?
Serial.printf("Command: %s\n", command.c_str());

// 2. Is controller idle?
Serial.printf("Active: %d\n", chainController->isActive());

// 3. Is relay activating?
// Add to lowerAnchor/raiseAnchor:
Serial.printf("Activating relay pin %d\n", pin);
```

### Chain Count Drifting

Possible causes:
- Hall sensor missing pulses
- Gypsy circumference wrong
- Accumulator reset unexpectedly

```cpp
// Log every pulse
Serial.printf("Pulse! Count: %.2f\n", accumulator->get());
```

### Slack Calculation Wrong

```cpp
// In calculateAndPublishHorizontalSlack():
Serial.printf("[SLACK] chain=%.2f depth=%.2f dist=%.2f\n",
              chainLength, depth, distance);
Serial.printf("[SLACK] effective_depth=%.2f factor=%.2f\n",
              effectiveDepth, catenaryFactor);
Serial.printf("[SLACK] result=%.2f\n", slack);
```

### SignalK Not Receiving Data

1. Check WiFi connected (serial shows IP)
2. Check SignalK server reachable
3. Check path spelling matches exactly
4. Check listener callbacks connected

### Deployment Stuck in Stage

```cpp
// In DeploymentManager::updateDeployment():
Serial.printf("[DEPLOY] stage=%d chain=%.2f target=%.2f\n",
              currentStage, currentChain, targetChain);
Serial.printf("[DEPLOY] distance=%.2f target_dist=%.2f\n",
              currentDistance, targetDistance);
```

---

## Serial Log Patterns

### Normal Operation
```
[INFO] ChainController initialized
[INFO] Lowering 5.00m
[INFO] Movement complete
```

### Warning Signs
```
[WARN] Movement timeout - recalibrating speed
[WARN] Pausing for slack (0.15m < 0.20m threshold)
[WARN] Negative slack detected: -1.5m
```

### Errors
```
[ERROR] Invalid state transition
[ERROR] Depth listener returned NaN
[ERROR] Accumulator pointer null
```

---

## Useful Debug Commands

```bash
# Monitor with timestamp
pio device monitor --baud 115200 --filter time

# Monitor with log to file
pio device monitor --baud 115200 --filter log2file

# Clean build (if weird issues)
pio run -t clean && pio run

# Check connected port
pio device list
```

---

## Task

$ARGUMENTS
