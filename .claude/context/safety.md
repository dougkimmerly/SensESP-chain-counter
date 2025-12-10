# Safety Context

> Hardware limits, timing constraints, and failure modes. **Read before modifying motor control.**

## Critical Hardware Limits

### Relay Pins - NEVER ACTIVATE BOTH

```cpp
DOWN_RELAY = GPIO 12
UP_RELAY   = GPIO 13
```

**DANGER**: Activating both relays simultaneously will:
- Short circuit the motor
- Potentially damage the windlass
- Trip circuit breakers

**Always** call `stop()` before changing direction.

### Chain Length Limits

| Limit | Value | Purpose |
|-------|-------|---------|
| MIN_LENGTH | 2.0m | Stop before chain fully up (safety) |
| MAX_LENGTH | 80m | Total chain aboard |
| STOP_BEFORE_MAX | 5.0m | Stop 5m before max (prevent jamming) |

## Slack Safety Thresholds

### During Retrieval

| Threshold | Value | Action |
|-----------|-------|--------|
| PAUSE_SLACK | 0.2m | Pause raising - chain too tight |
| RESUME_SLACK | 1.0m | Resume raising - slack built up |
| COOLDOWN | 3000ms | Wait between pause/resume actions |

**Why this matters**: If slack drops below 0.2m while raising, the chain is nearly taut. Continuing to raise will either:
- Drag the anchor (if it's set)
- Snap the chain (extreme case)
- Overload the windlass motor

### During Deployment

| Threshold | Value | Action |
|-----------|-------|--------|
| MAX_SLACK_RATIO | 0.85 | Max slack = 85% of depth |

**Why this matters**: Excessive slack during deployment means chain is piling up, not laying out properly.

### Final Pull Phase

When `rode <= depth + bow_height + 3.0m`, slack monitoring is **disabled**.

**Why**: At this point the anchor is coming up. The geometry means slack calculations become unreliable, and we need to pull regardless.

## Physics Constants - DO NOT CHANGE

| Constant | Value | Why |
|----------|-------|-----|
| BOW_HEIGHT_M | 2.0m | Physical distance from bow roller to waterline |
| CHAIN_WEIGHT_KG | 2.2 kg/m | Weight in water (buoyancy-adjusted) |
| GRAVITY | 9.81 m/s² | Physics constant |

**Changing BOW_HEIGHT** will break all slack calculations. The anchor appears 2m deeper than the depth sensor reads because the sensor is at waterline, but chain exits at bow.

## Timeout Safety

The ChainController has built-in movement timeouts:

```cpp
timeout = (amount / speed) * 1.5 + 5000ms
```

If the relay is active for longer than expected:
1. Relay is deactivated (motor stops)
2. Actual speed is calculated
3. Speed estimates are updated
4. Warning is logged

**Never remove timeout logic** - it prevents motor burnout if chain jams.

## Failure Modes

### Motor Doesn't Move

| Check | Fix |
|-------|-----|
| Relay clicking? | Check GPIO pins, wiring |
| Timeout too short? | Check speed calibration |
| Chain at limit? | Check MIN/MAX length |

### Chain Keeps Piling Up (Deployment)

| Check | Fix |
|-------|-----|
| Slack > 85% depth? | System should auto-pause |
| Boat not drifting? | Wind/current too weak |
| Distance not updating? | Check GPS/SignalK |

### Chain Snapping Tight (Retrieval)

| Check | Fix |
|-------|-----|
| Pausing at 0.2m slack? | Check pause logic |
| Cooldown working? | Check 3s cooldown |
| In final pull phase? | Slack checks disabled - expected |

### Negative Slack Calculated

Negative slack means: `GPS distance > catenary-adjusted chain reach`

**Meaning**: The boat is further from anchor than the chain can physically reach.

**Possible causes**:
1. Anchor is dragging
2. GPS error
3. Depth reading wrong
4. Chain length calibration off

## Safe Patterns

### Changing Direction

```cpp
// CORRECT
chainController->stop();
delay(100);  // Let relay settle
chainController->lowerAnchor(amount);

// WRONG - may activate both relays briefly
chainController->raiseAnchor(amount);  // while still lowering
```

### Checking Before Action

```cpp
// CORRECT
if (!chainController->isActive()) {
    chainController->lowerAnchor(amount);
}

// WRONG - may interrupt ongoing movement
chainController->lowerAnchor(amount);
```

### Emergency Stop

```cpp
// The stop() method is always safe to call
chainController->stop();  // Immediately deactivates all relays
```

## Serial Logging for Debugging

Enable verbose logging to diagnose issues:

```
CORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_VERBOSE
```

Key log messages to watch:
- `"Pausing for slack"` - Normal during retrieval
- `"Resuming after slack"` - Normal recovery
- `"Movement timeout"` - Possible jam or calibration issue
- `"Negative slack"` - Possible anchor drag
