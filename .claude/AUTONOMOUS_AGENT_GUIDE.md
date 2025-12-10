# Autonomous Agent System Guide

## Overview

This project uses a **hybrid agentic system** combining:
1. **Skills** - Auto-discovered capabilities that Claude invokes proactively
2. **Subagents** - Specialized agents for orchestrated workflows
3. **Background Execution** - Parallel agent execution for efficiency

## Quick Reference

| Type | When to Use | Invocation | Example |
|------|-------------|------------|---------|
| **Skill** | Auto-discovery, common tasks | Automatic by Claude | test-validation, log-analysis |
| **Subagent** | Specialized expertise, orchestration | Manual or automatic | cpp-developer, marine-expert |
| **Background** | Parallel work, async tasks | `run_in_background: true` | Multiple agents working simultaneously |

---

## 1. Skills (Auto-Discovery)

### Available Skills

#### `test-validation`
**Auto-invoked when:**
- Code is modified
- Build completes
- User says "test this"
- Safety-critical changes made

**Location**: `.claude/skills/test-validation/SKILL.md`

**What it does:**
- Runs build validation
- Executes P0 test suite
- Checks RAM/Flash usage
- Reports pass/fail status

**Example usage by Claude:**
```
User: "I just fixed the slack calculation bug"

Claude: [Automatically invokes test-validation skill]
"Let me validate those changes..."
[Runs build, checks tests, reports results]
```

#### `log-analysis`
**Auto-invoked when:**
- Test failures occur
- User reports bugs
- Errors appear in logs
- User says "debug" or "what happened"

**Location**: `.claude/skills/log-analysis/SKILL.md`

**What it does:**
- Captures serial logs
- Parses state transitions
- Identifies errors/warnings
- Provides root cause analysis

**Example usage by Claude:**
```
User: "The auto-retrieve stopped prematurely"

Claude: [Automatically invokes log-analysis skill]
"Let me analyze the logs..."
[Captures logs, finds pause/resume timing issue, reports findings]
```

---

## 2. Subagents (Specialized Expertise)

### Available Subagents

Located in `.claude/agents/`:

1. **cpp-sensesp-developer** - Code implementation
2. **embedded-systems-architect** - System design
3. **marine-domain-expert** - Domain validation
4. **log-capture-agent** - Debugging (now also a Skill!)
5. **test-engineer** - Testing (now also a Skill!)
6. **documentation-writer** - Documentation

### Manual Invocation

```markdown
User: "Please ask the marine expert to validate the new slack thresholds"

Claude: [Spawns marine-domain-expert subagent]
```

### Automatic Delegation

Claude will auto-delegate to subagents when:
- Task matches agent expertise
- User explicitly mentions agent role
- Complex analysis required

---

## 3. Background Execution (Parallel Work)

### How It Works

Multiple agents can work simultaneously using `run_in_background: true`:

```python
# Claude spawns multiple agents in parallel
Task(subagent_type="test-engineer", run_in_background=True)
Task(subagent_type="documentation-writer", run_in_background=True)
Task(subagent_type="log-capture-agent", run_in_background=True)

# Claude continues working on other tasks
# Later: Check results with AgentOutputTool
```

### Example Workflow

**Scenario**: User commits code changes

```markdown
User: "I've committed the retrieval bug fix, please validate everything"

Claude orchestrator:
1. [Spawns test-engineer in BACKGROUND] - Runs full test suite
2. [Spawns log-capture-agent in BACKGROUND] - Monitors for errors
3. [Spawns documentation-writer in BACKGROUND] - Updates docs
4. [CONTINUES WORKING] - Reviews code changes locally
5. [Checks agent results] - When all complete, reports status

Total time: ~5 minutes (parallel)
vs. Sequential: ~15 minutes
```

### Checking Background Agent Status

```python
# Non-blocking check (see if agent is done)
AgentOutputTool(agentId="agent-123", block=false)

# Blocking check (wait for results)
AgentOutputTool(agentId="agent-123", block=true)
```

---

## Practical Examples

### Example 1: Feature Development (Hybrid Approach)

**User Request**: "Add a new command to raise chain to a specific slack value"

**Claude's Autonomous Workflow**:

```markdown
Step 1: ORCHESTRATOR analyzes request
  ↓
Step 2: Spawn ARCHITECT (background) - Design the feature
  ↓
Step 3: Continue reviewing existing code
  ↓
Step 4: Check ARCHITECT results → Get design
  ↓
Step 5: Spawn C++ DEVELOPER (background) - Implement design
  ↓
Step 6: Spawn MARINE EXPERT (background) - Validate safety
  ↓
Step 7: Check both results → Merge if both approve
  ↓
Step 8: AUTO-INVOKE test-validation Skill - Test the changes
  ↓
Step 9: AUTO-INVOKE log-analysis Skill - Verify logs clean
  ↓
Step 10: Spawn DOCUMENTATION WRITER (background) - Update docs
  ↓
Step 11: Report completion to user
```

**Key Points**:
- ARCHITECT + MARINE EXPERT run **in parallel** (both independent)
- Skills **auto-invoke** at appropriate times
- Total time: **~10 minutes** vs **~30 minutes** sequential

---

### Example 2: Debugging (Auto-Discovery)

**User Report**: "Auto-retrieve is pausing too frequently"

**Claude's Autonomous Workflow**:

```markdown
Step 1: ORCHESTRATOR receives report
  ↓
Step 2: AUTO-INVOKE log-analysis Skill (detects "debugging" context)
  ↓
Step 3: Skill captures logs, analyzes pause/resume cycles
  ↓
Step 4: Skill reports: "Pause threshold too conservative"
  ↓
Step 5: Spawn MARINE EXPERT - "Is 0.2m slack threshold safe?"
  ↓
Step 6: MARINE EXPERT: "Yes, can increase to 0.3m"
  ↓
Step 7: Spawn C++ DEVELOPER - Modify threshold
  ↓
Step 8: AUTO-INVOKE test-validation Skill - Verify change
  ↓
Step 9: Report fix to user
```

**Key Points**:
- log-analysis **auto-invoked** (no manual trigger needed)
- test-validation **auto-invoked** after code change
- User gets **proactive** debugging

---

### Example 3: Continuous Monitoring (Background Agents)

**User Setup**: "Monitor the system while I work on other things"

**Claude's Workflow**:

```markdown
Step 1: Spawn LOG-CAPTURE-AGENT (background, continuous)
  ↓
Step 2: Agent captures serial output to logs/monitor_TIMESTAMP.log
  ↓
Step 3: User continues other work
  ↓
Step 4: Agent detects WARNING in logs
  ↓
Step 5: Agent reports: "Warning detected: slack < 0 for 45s"
  ↓
Step 6: ORCHESTRATOR checks agent output
  ↓
Step 7: AUTO-INVOKE log-analysis Skill - Analyze the warning
  ↓
Step 8: Report findings to user
```

**Key Points**:
- Agent runs **independently** in background
- Claude **checks periodically** for alerts
- User gets **asynchronous** notifications

---

## Best Practices

### When to Use Skills
✅ **Use Skills for:**
- Common, repetitive tasks (testing, log analysis)
- Tasks Claude should recognize and auto-invoke
- Standard workflows (build validation, debugging)

❌ **Don't use Skills for:**
- Rarely used operations
- Highly specialized one-off tasks
- Tasks requiring user confirmation

### When to Use Subagents
✅ **Use Subagents for:**
- Deep domain expertise (marine procedures)
- Complex architectural decisions
- Orchestrated multi-step workflows
- Tasks requiring focused context

❌ **Don't use Subagents for:**
- Simple one-line changes
- Tasks Skills can handle
- Purely informational queries

### When to Use Background Execution
✅ **Use Background for:**
- Independent parallel tasks (test + docs update)
- Long-running operations (full test suite)
- Monitoring tasks
- When you have other work to do

❌ **Don't use Background for:**
- Tasks with dependencies (B needs A's result)
- Quick operations (<30 seconds)
- Critical tasks needing immediate review

---

## Configuration Tips

### Making Skills More Autonomous

Edit `.claude/skills/<skill-name>/SKILL.md` frontmatter:

```yaml
---
name: my-skill
description: Clear, specific description of WHEN to auto-invoke
model-invoked: true  # CRITICAL for auto-discovery
---
```

**Key**: The `description` field tells Claude **when** to use the skill.

Good description:
> "Automatically validates code changes through testing. Invoked when code is modified, builds complete, or user requests validation."

Bad description:
> "A testing skill."

### Restricting Skill Tools

Limit which tools a skill can use:

```yaml
---
name: my-skill
model-invoked: true
allowed-tools:
  - Read
  - Bash
  - Grep
---
```

This prevents skills from making unintended changes.

---

## Monitoring Agent Activity

### Check Running Agents

```bash
# Claude shows active background agents
/tasks
```

### View Agent Output

```markdown
# In conversation
Claude: [Uses AgentOutputTool to check agent progress]
```

### Kill Stuck Agents

```bash
# If an agent is stuck
/kill-agent <agent-id>
```

---

## Troubleshooting

### Skill Not Auto-Invoking

**Check**:
1. `model-invoked: true` in SKILL.md frontmatter?
2. `description` field clear about when to invoke?
3. File named exactly `SKILL.md` (not `skill.md`)?
4. File in `.claude/skills/<skill-name>/SKILL.md`?

### Background Agent Not Responding

**Check**:
1. Use AgentOutputTool with `block=false` to check status
2. Check agent hasn't timed out (default ~10min)
3. Look for error messages in agent output

### Too Many Auto-Invocations

**Fix**:
1. Make skill descriptions more specific
2. Add conditions to description ("only when X")
3. Consider converting to regular subagent

---

## Advanced Patterns

### Cascading Agents

Agent A spawns Agent B:

```markdown
cpp-developer implements feature
  ↓ (spawns)
test-engineer validates implementation
  ↓ (spawns if bugs found)
log-capture-agent debugs failures
  ↓ (reports back to)
cpp-developer fixes bugs
```

### Parallel Validation

Multiple validators run simultaneously:

```markdown
Code change committed
  ↓ (parallel spawn)
  ├─ test-engineer → Build + functional tests
  ├─ marine-expert → Domain validation
  └─ documentation-writer → Doc updates
  ↓ (all complete)
Orchestrator merges results → Report to user
```

### Continuous Integration Pattern

```markdown
User: "Monitor and test continuously while I develop"

Claude orchestrator:
1. Spawn log-capture (background, continuous)
2. Watch for file changes
3. On change detected:
   → Auto-invoke test-validation skill
   → Check results
   → Report pass/fail
4. Loop
```

---

## Quick Start Checklist

To set up your autonomous agent environment:

- [x] Skills created in `.claude/skills/`
  - [x] test-validation
  - [x] log-analysis
- [x] Subagents available in `.claude/agents/`
  - [x] cpp-sensesp-developer
  - [x] embedded-systems-architect
  - [x] marine-domain-expert
  - [x] test-engineer
  - [x] log-capture-agent
  - [x] documentation-writer
- [x] SKILL.md files have `model-invoked: true`
- [x] Skill descriptions clearly state when to invoke
- [ ] Test auto-invocation (make a code change, see if test-validation triggers)
- [ ] Test background execution (spawn multiple agents)
- [ ] Review orchestrator README for workflows

---

## Next Steps

1. **Try auto-discovery**: Make a small code change and see if test-validation auto-triggers
2. **Test background execution**: Ask Claude to "validate everything in parallel"
3. **Monitor activity**: Use `/tasks` to see what's running
4. **Customize**: Adjust skill descriptions based on your workflow

---

## Resources

- **Skills Documentation**: `.claude/skills/*/SKILL.md`
- **Agent Documentation**: `.claude/agents/*.md`
- **Orchestrator Guide**: `.claude/agents/README.md`
- **Project Context**: `CLAUDE.md`

---

**Version**: 1.0
**Last Updated**: December 2024
**Maintained By**: Orchestrator
**Project**: SensESP Chain Counter - Autonomous Agent System
