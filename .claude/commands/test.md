# /test - Testing Workflow

> Use for testing procedures, validation, and verification.

## Workflow

### Phase 1: Identify Test Scope

**What needs testing?**

| Change Type | Test Approach |
|-------------|---------------|
| Motor control | Physical test with relays |
| Physics/slack | Serial output verification |
| State machine | Command sequence test |
| SignalK paths | SignalK server verification |
| Build system | Clean build test |

### Phase 2: Build Verification

```bash
# Clean build (catches stale object files)
pio run -t clean && pio run

# Check for warnings
# Look for: "warning:" in output

# Upload to device
pio run -t upload
```

### Phase 3: Serial Monitor Testing

```bash
# Start monitor
pio device monitor --baud 115200

# With timestamp (helpful for timing issues)
pio device monitor --baud 115200 --filter time

# Log to file for analysis
pio device monitor --baud 115200 --filter log2file
```

**What to look for:**
- Startup messages (WiFi, SignalK connection)
- No error messages
- Periodic slack calculations (every 500ms)
- Command acknowledgments

### Phase 4: Functional Tests

#### Test: Motor Control
1. Send `lower10` command via SignalK
2. Verify DOWN relay activates (GPIO 12)
3. Verify chain count increases
4. Send `STOP` command
5. Verify relay deactivates immediately

#### Test: Slack Calculation
1. Monitor serial output
2. Look for `[SLACK]` log entries
3. Verify values change with depth changes
4. Check: slack = 0 when chain = depth + bow_height

#### Test: Deployment Sequence
1. Send `autoDrop` command
2. Monitor stage transitions in serial
3. Verify pauses when slack > 85% depth
4. Verify completion at full deployment

#### Test: Retrieval Sequence
1. With chain deployed, send `autoRetrieve`
2. Monitor slack-based pause/resume
3. Verify final pull phase (no pauses when close)
4. Verify stop at ~2m remaining

#### Test: Emergency Stop
1. During any operation, send `STOP`
2. Verify immediate relay deactivation
3. Verify state returns to IDLE

### Phase 5: SignalK Integration Test

**From SignalK server or app:**

1. Check published values updating:
   - `navigation.anchor.rodeDeployed`
   - `navigation.anchor.chainSlack`
   - `navigation.anchor.chainDirection`

2. Send commands and verify response:
   - PUT to `navigation.anchor.command`
   - Check acknowledgment

3. Verify subscriptions working:
   - Change depth on server
   - See new depth in serial log

### Phase 6: Report Results

```markdown
## Test Results

### Build
- Clean build: ✅ / ❌
- Warnings: [list any]
- Upload: ✅ / ❌

### Motor Control
- Lower: ✅ / ❌
- Raise: ✅ / ❌
- Stop: ✅ / ❌

### State Machine
- Deployment sequence: ✅ / ❌
- Retrieval sequence: ✅ / ❌
- Emergency stop: ✅ / ❌

### SignalK
- Publishing: ✅ / ❌
- Subscribing: ✅ / ❌
- Commands: ✅ / ❌

### Issues Found
- [List any issues]
```

---

## Test Scenarios

### Scenario: Normal Deployment

**Setup:**
- Depth: 5m
- Starting chain: 0m

**Steps:**
1. Send `autoDrop`
2. Wait for DROP stage complete
3. Observe DEPLOY_30 with monitoring
4. Continue through all stages
5. Verify COMPLETE state

**Expected:**
- Chain deploys in stages
- Pauses if slack > 4.25m (85% of 5m)
- Completes with full chain out

### Scenario: Retrieval with Slack Management

**Setup:**
- Chain: 25m deployed
- Depth: 5m

**Steps:**
1. Send `autoRetrieve`
2. Observe raising
3. Watch for pause when slack < 0.2m
4. Watch for resume when slack > 1.0m
5. Observe final pull (no pauses)

**Expected:**
- Raises in segments
- Pauses to let slack build
- Final continuous pull when close

### Scenario: Emergency Stop

**Setup:**
- Any operation in progress

**Steps:**
1. Send `STOP` command
2. Observe immediate stop
3. Check state is IDLE

**Expected:**
- Relay deactivates within 100ms
- State returns to IDLE
- Can issue new commands

### Scenario: Edge Case - No Depth Data

**Setup:**
- Disconnect depth sensor / no SignalK depth

**Steps:**
1. Attempt deployment
2. Observe behavior

**Expected:**
- Should not deploy without valid depth
- Or use safe default behavior

---

## Physical Test Checklist

Before real-world testing:

- [ ] Verify relay wiring (DOWN=12, UP=13)
- [ ] Test relays with multimeter (click test)
- [ ] Verify chain counter sensor working
- [ ] Confirm gypsy circumference setting
- [ ] Test emergency stop button (if equipped)
- [ ] Have manual override ready

---

## Task

$ARGUMENTS
