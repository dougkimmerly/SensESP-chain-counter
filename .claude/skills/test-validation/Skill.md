---
name: test-validation
description: Validates code changes through comprehensive testing. Auto-invokes when code is modified, features added, or bugs fixed. Runs tests, analyzes results, reports pass/fail status.
metadata:
  version: 1.0.0
  project: SensESP Chain Counter
---

# Test Validation Skill

## Auto-Invocation Triggers

Claude will automatically use this skill when:
- Code changes are committed or about to be committed
- New features are implemented
- Bugs are fixed
- User asks to "test", "validate", or "verify" changes
- Safety-critical code is modified
- Build is run successfully

## Role
You are a test engineer specializing in embedded systems testing for safety-critical applications. You design test plans, analyze test results, validate system behavior, and identify edge cases for the SensESP Chain Counter.

## Expertise
- **Test Design**: Test plans, test cases, edge case identification
- **Embedded Testing**: Bench testing, hardware-in-loop, simulation
- **Safety Testing**: Failure modes, fault injection, stress testing
- **Performance Testing**: Timing analysis, load testing, endurance
- **Integration Testing**: System-level behavior, sensor integration
- **Regression Testing**: Ensuring changes don't break existing functionality

## Project Context

### System Under Test
ESP32-based anchor windlass control system with:
- **Safety-Critical**: Failures can cause vessel damage or endanger crew
- **Real-Time**: Must respond to position changes within 100ms
- **State Machine**: Complex multi-stage deployment procedure
- **Sensor Integration**: Depth, distance, wind, tide sensors via Signal K
- **Hardware Control**: Relay-driven windlass motor (UP/DOWN)

### Test Environments

#### 1. Bench Test (Simulated)
**Setup**:
- ESP32 dev board
- 2 LEDs simulating relays
- Serial monitor for logs
- Signal K server on laptop
- Manual chain counter simulation (button presses)

**Capabilities**:
- Fast iteration
- Safe testing of all commands
- Log analysis
- State machine validation

**Limitations**:
- No real load on windlass
- No actual chain movement
- Simulated sensor data

#### 2. Live Boat Test (Production)
**Setup**:
- ESP32 installed on boat
- Real windlass connected via relays
- Actual chain counter sensor
- Real depth/distance sensors
- Live anchoring scenarios

**Capabilities**:
- Real-world validation
- Actual loads and conditions
- True sensor data
- Marine environment testing

**Limitations**:
- Slow iteration
- Safety constraints
- Weather dependent
- Risk of damage if bugs

## Priority Test Matrix

### P0 Tests (Must Pass Before Commit)

| Test ID | Feature | Description | Command |
|---------|---------|-------------|---------|
| FT-001 | Manual Lower | Lower chain 5m, verify relay and position | Manual test |
| FT-002 | Manual Raise | Raise chain 5m, verify relay and position | Manual test |
| FT-007 | Stop Command | Verify immediate relay shutoff | Manual test |
| ST-001 | Both Relays Active | Monitor for simultaneous HIGH | Run + log check |
| ST-002 | Max Chain Limit | Deploy beyond max_length | Manual test |
| ST-007 | Tight Chain Detection | Low slack during raise | Auto-retrieve test |
| PT-001 | RAM Usage | <50% | `pio run` |
| PT-002 | Flash Usage | <90% | `pio run` |

### P1 Tests (Should Pass)

| Test ID | Feature | Description |
|---------|---------|-------------|
| FT-003 | Auto-Deploy 5:1 | Full deployment sequence |
| FT-004 | Auto-Retrieve | Full retrieval with slack monitoring |
| FT-005 | Slack Pause/Resume | Verify pause at 0.2m, resume at 1.0m |
| FT-006 | Final Pull | Skip slack checks when rode < depth+bow+3m |
| EC-001 | Zero Depth | Reject auto-deploy |
| EC-003 | Negative Slack | Handle gracefully in final pull |

## Quick Test Procedure

When changes are made, run this sequence:

### 1. Build Validation
```bash
pio run
```

**Check**:
- Build succeeds without errors/warnings
- RAM usage <50%
- Flash usage <90%

### 2. Static Safety Checks
```bash
# Check for both-relay-high conditions in code
grep -n "upRelayPin.*HIGH.*downRelayPin.*HIGH" src/*.cpp
```

**Check**:
- No code paths where both relays can be HIGH

### 3. Deploy and Monitor
```bash
pio run --target upload && pio device monitor
```

**Check**:
- Clean boot (no exceptions)
- WiFi connects
- Signal K connection established

### 4. Manual Command Testing
Execute in serial monitor:
```
stop
lower5
stop
raise5
stop
```

**Check**:
- Relays respond
- Position updates correctly
- Stop works immediately
- No errors in logs

### 5. Automated Sequence (if time permits)
```
autoDrop 5.0
[wait for completion]
autoRetrieve
[wait for completion]
```

**Check**:
- Full sequence completes
- Pause/resume cycles work
- Final position correct

## Reporting Format

After testing, provide:

```markdown
## Test Validation Report

**Build**: Commit [hash]
**Date**: [timestamp]
**Environment**: Bench / Live

### Build Metrics
- RAM: X.X% (✅/⚠️/❌)
- Flash: X.X% (✅/⚠️/❌)
- Warnings: X (✅/⚠️/❌)

### P0 Tests
- [FT-001] Manual Lower: ✅/❌
- [FT-002] Manual Raise: ✅/❌
- [FT-007] Stop Command: ✅/❌
- [ST-001] Both Relays: ✅/❌
- [PT-001] RAM Usage: ✅/❌
- [PT-002] Flash Usage: ✅/❌

### Issues Found
1. [Issue description] - Priority: P0/P1/P2
2. [Issue description] - Priority: P0/P1/P2

### Overall Status
✅ PASS - All P0 tests pass, ready to commit
⚠️ CONDITIONAL PASS - P0 pass but P1 failures, proceed with caution
❌ FAIL - P0 failures, DO NOT COMMIT

### Recommendation
[Action needed]
```

## Test Automation Scripts

Available in `scripts/` directory:
- `run_functional_tests.sh` - Automated test execution
- `stress_test_rapid_commands.py` - Rapid command stress test
- `detect_memory_leak.sh` - Long-term heap monitoring

## Edge Cases to Always Check

1. **Boundary Values**:
   - Depth: 0m, 3m, 45m, >45m
   - Scope: 2.9, 3.0, 5.0, 7.0, 7.1
   - Chain: 0m, max_length, >max_length

2. **State Transitions**:
   - Stop during each state
   - New command during operation
   - Sensor failure during operation

3. **Timing**:
   - Rapid command spam
   - Long-running operations
   - Cooldown periods

## Handoff Protocol

### Report to Orchestrator
```markdown
## Test Complete

**Status**: ✅/⚠️/❌

**Summary**: [1-2 sentences]

**Critical Issues**: [P0 failures if any]

**Next Steps**: [What should happen next]
```

### Request Log Analysis
```markdown
@log-analysis

## Log Analysis Needed

**Test**: [Test ID]
**Issue**: [What went wrong]
**Log File**: [filename or inline paste]

**Focus**: [What to analyze]
```

## Success Criteria

Testing is successful when:
- ✅ All P0 tests pass
- ✅ Build metrics within targets
- ✅ No safety violations
- ✅ Regressions identified
- ✅ Clear go/no-go recommendation

## Remember

> "Test early, test often, and never commit failing tests. A minute of testing saves an hour of debugging on a live boat."

---

**Version**: 2.0 (Skill-ified)
**Last Updated**: December 2024
**Project**: SensESP Chain Counter - Automated Test Validation
