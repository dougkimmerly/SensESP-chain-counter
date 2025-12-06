# Quick Start Guide for Agent Team

## For Doug: How to Use Your Agent Team

### Starting a New Session

**1. Choose Your Primary Agent:**

For coding tasks:
```markdown
Load: .claude/agents/cpp-sensesp-developer.md

I need to add a new feature: [describe feature]
```

For architecture questions:
```markdown
Load: .claude/agents/embedded-systems-architect.md

I'm considering two approaches for [problem]:
- Approach A: [details]
- Approach B: [details]

Which is better for our constraints?
```

For debugging:
```markdown
Load: .claude/agents/log-capture-agent.md

Here's a log from a failed test: [paste log]

Can you analyze what went wrong?
```

**2. For Multi-Agent Tasks:**

Start with orchestrator:
```markdown
Load: .claude/agents/README.md

I need to implement [feature]. This will require:
- Code changes (C++ developer)
- Safety validation (Marine expert)
- Testing (Test engineer)

Please coordinate the workflow.
```

### Quick Reference

| I want to... | Use this agent |
|-------------|----------------|
| Write C++ code | cpp-sensesp-developer |
| Optimize performance | embedded-systems-architect |
| Validate marine safety | marine-domain-expert |
| Analyze logs | log-capture-agent |
| Create tests | test-engineer |
| Update documentation | documentation-writer |
| Coordinate multiple agents | README (orchestrator) |

### Example Session: Add New Feature

```markdown
Session Start:

Load: .claude/agents/cpp-sensesp-developer.md

## Task: Add anchor drag detection

**Requirement**: Monitor negative slack and alert if anchor dragging

**Behavior**:
- Calculate slack every 5s
- If slack negative for >30s, set dragAlert = true
- Publish to Signal K: navigation.anchor.dragAlert

**Files to Modify**:
- ChainController.h: Add dragAlert_ ObservableValue
- ChainController.cpp: Add drag detection logic
- main.cpp: Connect to Signal K output

**Safety**: Must not interfere with normal operations

**Testing**: Simulate by setting distance > chain length

Can you implement this?
```

### Tips for Effective Agent Use

1. **Be Specific**: Give clear requirements and constraints
2. **Provide Context**: Mention relevant existing code
3. **Set Priorities**: P0 (critical), P1 (important), P2 (nice-to-have)
4. **Request Testing**: Always ask for verification steps
5. **Save Logs**: Keep logs of important test runs for analysis

### Common Workflows

**Bug Fix Workflow:**
```
1. log-capture-agent: Analyze the failure log
2. cpp-sensesp-developer: Implement the fix
3. test-engineer: Validate the fix
4. documentation-writer: Update troubleshooting docs
```

**New Feature Workflow:**
```
1. embedded-systems-architect: Review design approach
2. marine-domain-expert: Validate safety (if applicable)
3. cpp-sensesp-developer: Implement code
4. test-engineer: Create and run tests
5. documentation-writer: Document the feature
```

**Performance Issue Workflow:**
```
1. log-capture-agent: Measure current performance
2. embedded-systems-architect: Propose optimization
3. cpp-sensesp-developer: Implement changes
4. test-engineer: Verify improvement, check regressions
```

### Emergency: Critical Bug

```markdown
Load: .claude/agents/README.md

CRITICAL BUG: [Describe issue]

User impact: [Severity]

Logs: [Attach or paste]

Need immediate analysis and fix.
```

The orchestrator will coordinate:
- log-capture-agent for diagnosis
- cpp-sensesp-developer for hotfix
- test-engineer for quick validation

### Preserving Context Between Sessions

Each agent file contains:
- ✅ Full project context
- ✅ Architecture knowledge
- ✅ Critical constants and patterns
- ✅ Known gotchas
- ✅ Safety requirements

Just load the relevant agent file at the start of each session!

### Agent Files Location

All agent prompts are in:
```
.claude/agents/
├── README.md                          # Orchestrator (coordinates team)
├── cpp-sensesp-developer.md          # Implements code
├── embedded-systems-architect.md     # Designs architecture
├── marine-domain-expert.md           # Validates marine safety
├── log-capture-agent.md              # Analyzes logs
├── test-engineer.md                  # Creates tests
└── documentation-writer.md           # Maintains docs
```

### Getting Help

If unsure which agent to use:
```markdown
Load: .claude/agents/README.md

I need help with [problem]. Which agent should I use?
```

---

**Pro Tip**: You can load multiple agent files in sequence if you need combined expertise. For example, load embedded-systems-architect first for design, then hand off to cpp-sensesp-developer for implementation.

**Remember**: These agents have deep context about your project. They know about:
- The Version 2.0 refactor (RetrievalManager elimination)
- The critical bug fix (autoRetrieve during deployment)
- All constants, patterns, and safety requirements
- The marine domain requirements

You don't need to re-explain the project each time!