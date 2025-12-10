# /review - Code Review

> Use for code review, safety checks, and quality assessment.

## Workflow

### Phase 1: Understand the Change

1. **Identify what changed** - files, functions, scope
2. **Understand the intent** - what problem does this solve?
3. **Load relevant context:**
   - `.claude/context/safety.md` - For safety implications
   - `.claude/context/patterns.md` - For style compliance

### Phase 2: Safety Check

**CRITICAL - Hardware Safety:**

| Check | Requirement |
|-------|-------------|
| Relay control | Never activate both GPIO 12 and 13 simultaneously |
| Direction change | Always call `stop()` before changing direction |
| Chain limits | Respect MIN_LENGTH and MAX_LENGTH |
| Timeout logic | Never remove or bypass timeouts |

**Physics Constants:**

| Constant | Value | Can Change? |
|----------|-------|-------------|
| BOW_HEIGHT_M | 2.0 | NO - physical measurement |
| PAUSE_SLACK_M | 0.2 | Carefully - affects safety |
| RESUME_SLACK_M | 1.0 | Carefully - affects safety |
| FINAL_PULL_THRESHOLD_M | 3.0 | Carefully - affects final pull |

**State Machine Integrity:**
- Do state transitions make sense?
- Are all states reachable?
- Can we get stuck in a state?
- Is there always a way to stop?

### Phase 3: Code Quality

**Style:**
- [ ] 4-space indentation
- [ ] Trailing underscore for members (`target_`)
- [ ] `constexpr` for constants
- [ ] `const` on getter methods
- [ ] Enum class (not plain enum)

**Patterns:**
- [ ] Null checks before pointer use
- [ ] Division by zero prevention
- [ ] NaN/Inf checks on calculations
- [ ] Proper relay control sequence

**Memory:**
- [ ] No memory leaks (SensESP objects use `new`)
- [ ] No stack overflow (large arrays on heap)
- [ ] Persistent objects for callbacks

### Phase 4: Testing

**Was it tested?**
- [ ] Builds without warnings
- [ ] Uploads successfully
- [ ] Serial output looks correct
- [ ] Physical test (if hardware change)

**Test scenarios:**
- Normal operation
- Edge cases (empty chain, full chain)
- Error conditions (no depth, no GPS)

### Phase 5: Report

**Summary format:**

```markdown
## Review: [Brief description]

### Safety: ✅ / ⚠️ / ❌
[Any safety concerns]

### Quality: ✅ / ⚠️ / ❌
[Style and pattern compliance]

### Testing: ✅ / ⚠️ / ❌
[Test coverage assessment]

### Recommendations
1. [Specific actionable items]

### Approved: Yes / No / Conditional
```

---

## Review Checklists

### For ChainController.cpp Changes

- [ ] Relay activation follows safe pattern
- [ ] Timeout logic preserved
- [ ] Speed calibration not broken
- [ ] Slack thresholds not arbitrarily changed
- [ ] BOW_HEIGHT not modified

### For DeploymentManager.cpp Changes

- [ ] State transitions are valid
- [ ] Can always reach IDLE/stop
- [ ] Slack monitoring not bypassed (except final pull)
- [ ] Distance/chain targets calculated correctly

### For main.cpp Changes

- [ ] Commands validate state before acting
- [ ] Loop timing not disrupted
- [ ] SignalK connections correct
- [ ] No blocking delays in loop

### For Header File Changes

- [ ] Constants have sensible values
- [ ] New members properly initialized
- [ ] Public/private correctly assigned
- [ ] Include guards present

---

## Red Flags

**Immediate rejection:**
- Both relays can activate simultaneously
- Timeout logic removed/bypassed
- BOW_HEIGHT changed without physical reason
- Blocking `delay()` in main loop
- Memory allocation in loop without free

**Needs discussion:**
- Changing safety thresholds
- New state transitions
- Modifying physics calculations
- Adding new commands

---

## Task

$ARGUMENTS
