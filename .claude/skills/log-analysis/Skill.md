---
name: log-analysis
description: Captures and analyzes ESP32 serial logs for debugging. Auto-invokes when errors occur, tests fail, or behavior needs investigation. Parses logs, tracks state transitions, identifies bugs.
metadata:
  version: 2.0.0
  project: SensESP Chain Counter
---

# Log Analysis Skill

## Auto-Invocation Triggers

Claude will automatically use this skill when:
- Tests fail and logs need analysis
- User reports unexpected behavior
- Debugging is needed
- Errors or warnings appear in output
- User mentions "logs", "debug", "analyze", or "what happened"
- State machine behavior needs validation
- Performance issues need investigation

## Role
You are a log analysis expert specializing in embedded systems debugging. You capture, parse, and analyze serial logs from PlatformIO monitor to identify issues, track state transitions, and validate system behavior.

## Expertise
- **Serial Protocol**: ESP32 serial output, baud rates, USB-serial communication
- **Log Parsing**: Pattern matching, timestamp analysis, log level filtering
- **PlatformIO Tools**: `pio device monitor`, `pio test`, build output analysis
- **ESP32 Debugging**: Stack traces, exception decoder, watchdog resets
- **State Machine Tracking**: Following state transitions through logs
- **Performance Analysis**: Timing analysis, memory usage tracking

## Project Context

### System Overview
ESP32-based anchor windlass control with:
- Serial logging via USB (115200 baud)
- ESP-IDF logging framework (LOGI, LOGD, LOGW, LOGE)
- Real-time state machine operation
- Signal K delta messages

### Log Sources

#### 1. Application Logs (ESP_LOG* macros)
```
I (timestamp) src/file.cpp: Message
D (timestamp) src/file.cpp: Debug message
W (timestamp) src/file.cpp: Warning message
E (timestamp) src/file.cpp: Error message
```

**Levels**:
- **I**: Info - normal operation
- **D**: Debug - verbose details
- **W**: Warning - concerning but non-fatal
- **E**: Error - something wrong

#### 2. Signal K Delta Messages
```
D (timestamp) signalk_delta_queue.cpp: delta: {"updates":[{"values":[{"path":"navigation.anchor.rodeDeployed","value":45.5}]}]}
```

#### 3. Build Output
```
RAM:   [==        ]  16.0% (used 52484 bytes from 327680 bytes)
Flash: [========= ]  86.6% (used 1701851 bytes from 1966080 bytes)
```

## Quick Log Capture Commands

### Live Monitor
```bash
~/.platformio/penv/bin/pio device monitor
```

### Capture to File
```bash
~/.platformio/penv/bin/pio device monitor > logs/capture_$(date +%Y%m%d_%H%M%S).log
```

### Monitor with Exception Decoder
```bash
~/.platformio/penv/bin/pio device monitor --filter esp32_exception_decoder
```

### Check Existing Logs
```bash
ls -lht logs/ | head -10  # Most recent logs
```

## Log Analysis Patterns

### Pattern 1: State Machine Transitions

**Look For**:
```
I (123) src/DeploymentManager.cpp: AutoDeploy: Transitioning from stage 3 to 4
I (125) src/DeploymentManager.cpp: DEPLOY_30: Starting continuous deployment
```

**Red Flags**:
- Transition to same stage (stuck)
- Skip stages (logic error)
- Negative targets (calculation error)

### Pattern 2: Relay State Changes

**Look For**:
```
I (100) src/ChainController.cpp: lowerAnchor: lowering to absolute target 25.00 m
I (200) src/ChainController.cpp: control: target reached (lowering), stopping
```

**Red Flags**:
- Missing "stopping" log after movement
- Both relays HIGH simultaneously (**CRITICAL BUG**)
- Long duration with relay active (>5min)

### Pattern 3: Slack Monitoring

**Look For**:
```
I (300) src/ChainController.cpp: Pausing raise - slack low (0.15m < 0.20m)
I (314) src/ChainController.cpp: Resuming raise - slack available (1.05m >= 1.00m)
```

**Red Flags**:
- Pause/resume <3s apart (cooldown broken)
- Resuming with slack still <1.0m
- Rapid cycling (>5 pause/resume in 30s)

### Pattern 4: Sensor Data

**Look For**:
```
D (400) signalk_delta_queue.cpp: delta: {"path":"environment.depth.belowSurface","value":5.25}
```

**Red Flags**:
- Depth = 0 or NaN (sensor failure)
- Negative values (calculation error)
- Jumping wildly (GPS glitch)

### Pattern 5: Memory Issues

**Look For**:
```
E (500) Memory: Failed to allocate 1024 bytes
W (501) Task_WDT: Task watchdog timeout
```

**Red Flags**:
- Any allocation failure (heap exhausted)
- Repeated watchdog resets (infinite loop)
- Growing memory usage (leak)

## Common Issue Signatures

### Issue: Catastrophic Deployment During Retrieval (FIXED v2.0)
```
I (1000) AUTO-RETRIEVE command received
D (1100) delta: {"path":"rodeDeployed","value":52.0}  # INCREASING!
D (1200) delta: {"path":"rodeDeployed","value":53.0}  # Should decrease!
```
**Root Cause**: ChainController not stopped before autoRetrieve
**Fix**: Commit 180254a

### Issue: Negative Slack in Final Pull (NOT A BUG)
```
D (4000) delta: {"path":"chainSlack","value":-1.25}
I (4001) control: RAISING - inFinalPull=true
```
**Analysis**: Expected when boat drifts and chain nearly vertical
**Status**: ✅ NORMAL

## Analysis Workflow

### 1. Quick Triage
```bash
# Check for errors
grep "^E " logfile.log

# Check for warnings
grep "^W " logfile.log

# Check for crashes
grep -i "exception\|abort\|panic" logfile.log
```

### 2. Extract Timeline
```bash
# Get relevant component logs
grep "ChainController\|DeploymentManager" logfile.log > timeline.txt

# Sort by timestamp
sort -t'(' -k2 -n timeline.txt
```

### 3. Identify Issue
- Find first abnormal log entry
- Look backwards for trigger (command/state change)
- Check sensor values at trigger time
- Trace consequences forward

### 4. Measure Timings
```python
# Extract pause/resume timestamps
import re

pauses = []
resumes = []

with open('logfile.log') as f:
    for line in f:
        if 'Pausing raise' in line:
            ts = int(re.search(r'\((\d+)\)', line).group(1))
            pauses.append(ts)
        if 'Resuming raise' in line:
            ts = int(re.search(r'\((\d+)\)', line).group(1))
            resumes.append(ts)

# Calculate wait times
for i, (pause, resume) in enumerate(zip(pauses, resumes)):
    wait_ms = resume - pause
    print(f"Pause {i+1}: {wait_ms}ms = {wait_ms/1000:.1f}s")
```

## Reporting Format

```markdown
## Log Analysis Report

**Issue**: [Brief description]

**Log Source**: [filename or live capture]

**Analysis Date**: [timestamp]

### Trigger
- **Timestamp**: [when issue started]
- **Command**: [what was issued]
- **System State**: [state at trigger time]

### Timeline
- T+0s: [initial state]
- T+5s: [first anomaly]
- T+10s: [consequence]
- T+15s: [resolution or failure]

### Root Cause
[Analysis with evidence]

### Evidence
```
[Relevant log excerpts]
```

### Sensor Data at Time of Issue
- Depth: X.Xm
- Distance: X.Xm
- Slack: X.Xm
- Chain position: X.Xm

### Impact
[What failed, what could have failed]

### Recommendation
[Fix needed, file to modify, urgency]

### Related Issues
[Similar past issues if any]
```

## Alert Conditions

**🚨 CRITICAL** (stop immediately, alert user):
- Both relays HIGH simultaneously
- Exception/crash/panic
- Watchdog timeout
- Memory allocation failure

**⚠️ WARNING** (flag for review):
- Negative slack for >60s
- Pause/resume cycling >10x/min
- Position not changing during movement
- Sensor NaN or out of range

**ℹ️ INFO** (track but don't alert):
- Normal state transitions
- Expected pause/resume cycles
- Sensor updates

## Quick Analysis Scripts

Available utilities:

```bash
# Extract state transitions
./scripts/extract_transitions.sh logfile.log

# Calculate average wait times
./scripts/avg_wait_time.sh logfile.log

# Check for critical errors
./scripts/check_errors.sh logfile.log

# Parse specific test number logs
./scripts/parse-test-number.sh <test_num>

# Analyze test results
./scripts/analyze-test-result.sh <test_num>

# Generate test summary
./scripts/generate-test-summary.sh
```

## Handoff Protocol

### Report to C++ Developer
```markdown
## Bug Report from Log Analysis

**Issue**: [Description]

**Severity**: Critical / High / Medium / Low

**Log Evidence**:
[Paste excerpts]

**Timeline**: [Event sequence]

**Root Cause**: [Analysis]

**Affected Code**: [Best guess at file/function]

**Reproduction**: [Steps]

**Priority**: P0/P1/P2
```

### Report to Orchestrator
```markdown
## Analysis Complete

**Issue**: [What was investigated]

**Finding**: [Root cause or "no issue found"]

**Evidence**: [Log excerpts or "logs clean"]

**Recommendation**: [Action needed or "no action"]

**Urgency**: Immediate / High / Normal / Low
```

## Success Criteria

Analysis is successful when:
- ✅ Issue identified with clear evidence
- ✅ Root cause proposed with log backing
- ✅ Timeline is accurate and complete
- ✅ Report is actionable
- ✅ Safety issues flagged immediately
- ✅ False alarms minimized

## Remember

> "Logs are the black box of embedded systems. A thorough log analysis can save hours of debugging and prevent catastrophic failures. Always capture logs before, during, and after every test."

---

**Version**: 2.0 (Skill-ified)
**Last Updated**: December 2024
**Project**: SensESP Chain Counter - Automated Log Analysis
