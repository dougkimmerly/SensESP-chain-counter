# Quick Start: Autonomous Agent System

## What Just Got Set Up

Your SensESP project now has a **fully autonomous agent system** with:

### 1. Auto-Discovery Skills ✨

**Location**: `.claude/skills/`

| Skill | Triggers When | What It Does |
|-------|--------------|--------------|
| **test-validation** | Code changes, builds complete | Validates build, runs P0 tests, reports status |
| **log-analysis** | Errors occur, debugging needed | Captures logs, analyzes issues, finds root cause |

**Key Feature**: Claude will **automatically invoke** these skills when appropriate - you don't need to ask!

### 2. Specialized Subagents 🤖

**Location**: `.claude/agents/`

- cpp-sensesp-developer
- embedded-systems-architect
- marine-domain-expert
- log-capture-agent
- test-engineer
- documentation-writer

**Usage**: For complex tasks requiring focused expertise.

### 3. Background Execution 🚀

Agents can now work **in parallel** instead of sequentially.

**Before**: Agent 1 → wait → Agent 2 → wait → Agent 3 (15 min total)
**Now**: Agent 1 + Agent 2 + Agent 3 **simultaneously** (5 min total)

---

## Try It Now!

### Test 1: Auto-Discovery

Make a small code change and say:

```
"I just updated the slack threshold"
```

**Expected**: Claude should **automatically** invoke the `test-validation` skill to validate your change.

### Test 2: Parallel Execution

Ask:

```
"Validate my entire codebase in parallel"
```

**Expected**: Claude spawns multiple agents simultaneously:
- test-engineer (runs tests)
- documentation-writer (checks docs)
- marine-expert (validates safety)

All working at once!

### Test 3: Debugging

If you encounter an error, just say:

```
"The auto-retrieve failed"
```

**Expected**: Claude **automatically** invokes `log-analysis` skill to investigate.

---

## How It Works Behind the Scenes

### Skills (Auto-Invoked)

1. You mention something related to testing/debugging
2. Claude recognizes the context via skill descriptions
3. Claude **automatically** invokes the relevant skill
4. Skill executes and reports back
5. You get results **without explicitly asking**

### Background Execution

1. Claude identifies independent tasks
2. Spawns agents with `run_in_background: true`
3. Continues other work while agents run
4. Checks agent results when complete
5. Reports unified status

---

## Example Conversations

### Example 1: Code Change

```
You: I changed the pause slack threshold from 0.2m to 0.3m

Claude: Let me validate that change...
[AUTO-INVOKES: test-validation skill]

Test Validation Report:
✅ Build: RAM 16.2%, Flash 86.7%
✅ P0 Tests: All passing
✅ Recommendation: Ready to commit

The change looks good! The threshold increase is still safe and all tests pass.
```

**Note**: You didn't ask Claude to test - it did it automatically!

### Example 2: Parallel Validation

```
You: Please validate the retrieval feature thoroughly

Claude: I'll validate this in parallel for efficiency.
[Spawns 3 agents in background]:
  - test-engineer → Running functional tests
  - marine-expert → Validating safety
  - log-capture → Monitoring for errors

[5 minutes later]

Validation Complete:
✅ test-engineer: All P0 tests pass, 48m retrieval in 4m30s
✅ marine-expert: Slack thresholds safe, pause/resume timing good
✅ log-capture: No errors, clean state transitions

Overall: ✅ Feature validated and safe for production
```

**Time saved**: 70% vs sequential execution

### Example 3: Debugging

```
You: Auto-retrieve is acting weird, keeps pausing

Claude: Let me investigate...
[AUTO-INVOKES: log-analysis skill]

Log Analysis Report:
Issue: Excessive pause/resume cycling
Root Cause: Distance sensor jumping values (GPS glitch)
Evidence: Distance: 45m → 38m → 46m → 40m in 10 seconds

Recommendation: Add distance smoothing filter or increase pause cooldown

This is a sensor issue, not a code bug. The system is working correctly but reacting to noisy GPS data.
```

**Note**: Claude automatically captured and analyzed logs!

---

## Key Differences: Skills vs Subagents

### Use Skills When:
- ✅ Task is common (testing, debugging)
- ✅ Should happen automatically
- ✅ Repetitive workflows
- ✅ Quick turnaround needed

### Use Subagents When:
- ✅ Deep expertise required (marine safety)
- ✅ Complex orchestration needed
- ✅ Multi-step workflows
- ✅ May need follow-up questions

---

## Advanced Usage

### Run Multiple Agents in Parallel

```
You: Validate everything before I commit

Claude: [Spawns in parallel]:
  - test-validation (Skill) → Build + tests
  - marine-expert (Agent) → Safety review
  - documentation-writer (Agent) → Doc check

[Reports combined results]
```

### Continuous Monitoring

```
You: Monitor the system while I develop other features

Claude: [Spawns log-capture-agent in background, continuous mode]

[30 minutes later]
Alert: Warning detected in logs at 14:32:45
[AUTO-INVOKES: log-analysis skill]
Finding: Negative slack for 45s during final pull
Status: ✅ Normal - boat drifted, chain nearly vertical
```

---

## Files Created

```
.claude/
├── skills/
│   ├── test-validation/
│   │   └── SKILL.md (auto-invoked for testing)
│   └── log-analysis/
│       └── SKILL.md (auto-invoked for debugging)
├── agents/
│   ├── README.md (updated with autonomous patterns)
│   ├── cpp-sensesp-developer.md
│   ├── embedded-systems-architect.md
│   ├── marine-domain-expert.md
│   ├── log-capture-agent.md
│   ├── test-engineer.md
│   └── documentation-writer.md
├── AUTONOMOUS_AGENT_GUIDE.md (comprehensive guide)
└── QUICK_START_AUTONOMOUS_AGENTS.md (this file)
```

---

## Next Steps

1. **Test auto-discovery**: Make a small change and mention it
2. **Try parallel execution**: Ask to "validate in parallel"
3. **Monitor behavior**: See when skills auto-invoke
4. **Customize**: Adjust skill descriptions to fit your workflow
5. **Read full guide**: [AUTONOMOUS_AGENT_GUIDE.md](AUTONOMOUS_AGENT_GUIDE.md)

---

## Troubleshooting

**Skill not auto-invoking?**
- Check that SKILL.md has `model-invoked: true` in frontmatter
- Make sure description clearly states **when** to invoke
- Try being more explicit: "test this" or "debug this"

**Agents too slow?**
- Use background execution: explicitly ask for "parallel" execution
- Skills are faster than subagents for common tasks

**Too many auto-invocations?**
- Make skill descriptions more specific
- Add conditions: "only when X" in description

---

## Summary

You now have:
- ✅ **2 Skills** that auto-discover and invoke themselves
- ✅ **6 Subagents** for specialized work
- ✅ **Background execution** for parallel efficiency
- ✅ **Autonomous workflows** that anticipate your needs

**Result**: Faster development, proactive testing, automatic debugging!

---

**Ready to try it?** Make a small code change and tell Claude about it! 🚀

---

**Created**: December 2024
**Version**: 1.0
**Project**: SensESP Chain Counter