# Agent Team Guide for SensESP Chain Counter

This directory contains specialized agent prompts for collaborative work on the SensESP Chain Counter project.

## Agent Team Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    ORCHESTRATOR (You)                       │
│              Coordinates team, manages workflow             │
└──────────────┬──────────────────────────────────────────────┘
               │
       ┌───────┴───────┬──────────────┬───────────────┬──────────────┐
       │               │              │               │              │
┌──────▼──────┐ ┌─────▼─────┐ ┌──────▼──────┐ ┌──────▼──────┐ ┌────▼────┐
│ C++ SensESP │ │ Embedded  │ │   Marine    │ │     Log     │ │  Test   │
│  Developer  │ │ Architect │ │   Domain    │ │   Capture   │ │Engineer │
│  (coding)   │ │ (design)  │ │   Expert    │ │  (debug)    │ │(validate│
└──────┬──────┘ └─────┬─────┘ └──────┬──────┘ └──────┬──────┘ └────┬────┘
       │               │              │               │              │
       └───────┬───────┴──────────────┴───────────────┴──────────────┘
               │
        ┌──────▼──────┐
        │Documentation│
        │   Writer    │
        │  (sync docs)│
        └─────────────┘
```

## Available Agents

### 1. **cpp-sensesp-developer**
**File**: [cpp-sensesp-developer.md](cpp-sensesp-developer.md)

**When to Use**:
- Implementing new features
- Fixing bugs
- Modifying C++ code
- Adding Signal K paths
- Changing constants

**Expertise**:
- C++, SensESP framework, ESP32
- Signal K protocol
- State machines
- Embedded best practices

**Input Format**:
```markdown
## Task: [Brief description]

**Requirement**: [What needs to be done]

**Files to Modify**:
- File1.cpp: [Changes needed]
- File2.h: [Changes needed]

**Constraints**:
- Memory budget: <50% RAM
- Must maintain relay safety
- Signal K paths: [list if relevant]

**Testing**: [How to verify]
```

### 2. **embedded-systems-architect**
**File**: [embedded-systems-architect.md](embedded-systems-architect.md)

**When to Use**:
- Before major refactors
- Performance optimization needed
- Architecture decisions required
- Design review requested

**Expertise**:
- System architecture
- Memory/CPU optimization
- Safety-critical design
- State machine design

**Input Format**:
```markdown
## Architecture Decision Needed

**Problem**: [Current issue]

**Constraints**:
- RAM: [usage]
- Flash: [usage]
- Real-time: [latency requirements]

**Options Considered**:
1. Option A: [pros/cons]
2. Option B: [pros/cons]

**Recommendation Request**: Which approach is better?
```

### 3. **marine-domain-expert**
**File**: [marine-domain-expert.md](marine-domain-expert.md)

**When to Use**:
- Validating anchoring procedures
- Checking safety thresholds
- Reviewing catenary calculations
- Verifying marine terminology

**Expertise**:
- Anchoring procedures
- Catenary physics
- Marine safety
- Nautical terminology

**Input Format**:
```markdown
## Marine Safety Validation

**Change**: [What's being modified]

**Current Behavior**: [How it works now]

**Proposed Behavior**: [How it will work]

**Safety Questions**:
- Is scope ratio appropriate?
- Are slack thresholds safe?
- Does this match standard practice?

**Validation Needed**: [Specific concerns]
```

### 4. **log-capture-agent**
**File**: [log-capture-agent.md](log-capture-agent.md)

**When to Use**:
- Debugging runtime issues
- Analyzing failures
- Tracking state transitions
- Performance profiling

**Expertise**:
- Serial log parsing
- PlatformIO monitoring
- Pattern recognition
- Timing analysis

**Input Format**:
```markdown
## Log Analysis Request

**Issue**: [What went wrong]

**Log File**: [Filename or paste inline]

**Focus Areas**:
- State transitions around [time]
- Relay behavior during [period]
- Slack values during [operation]

**Questions**:
- Why did X happen at timestamp Y?
- Were there any errors?
- What was the sequence of events?
```

### 5. **test-engineer**
**File**: [test-engineer.md](test-engineer.md)

**When to Use**:
- After feature implementation
- Before releasing changes
- Regression testing needed
- Edge case validation

**Expertise**:
- Test design
- Edge case identification
- Validation procedures
- Test automation

**Input Format**:
```markdown
## Test Plan Request

**Feature**: [What was changed]

**Test Scope**:
- Functional: [What to verify]
- Safety: [Critical checks]
- Edge cases: [Boundary conditions]

**Environment**: Bench / Live Boat

**Priority**: P0 / P1 / P2

**Acceptance Criteria**: [Pass conditions]
```

### 6. **documentation-writer**
**File**: [documentation-writer.md](documentation-writer.md)

**When to Use**:
- After code changes
- New features added
- API changes made
- Version releases

**Expertise**:
- Technical writing
- Markdown documentation
- API documentation
- Changelog management

**Input Format**:
```markdown
## Documentation Update Needed

**Code Changes**:
- [File]: [What changed]

**Documentation Impact**:
- API reference: [New methods?]
- User guide: [New procedures?]
- Architecture: [Diagrams need update?]
- Changelog: [Version and category]

**Priority**: High / Medium / Low
```

## Typical Workflows

### Workflow 1: Implement New Feature

```
1. ORCHESTRATOR receives feature request from user
   ↓
2. Consult EMBEDDED ARCHITECT for design approach
   ↓
3. Hand to C++ DEVELOPER for implementation
   ↓
4. C++ DEVELOPER builds and tests
   ↓
5. Consult MARINE EXPERT if feature affects anchoring
   ↓
6. Hand to TEST ENGINEER for validation
   ↓
7. TEST ENGINEER runs test suite, reports results
   ↓
8. If bugs found → back to C++ DEVELOPER
   ↓
9. Hand to DOCUMENTATION WRITER for docs update
   ↓
10. ORCHESTRATOR reviews and commits
```

### Workflow 2: Debug Production Issue

```
1. User reports issue to ORCHESTRATOR
   ↓
2. Request LOG CAPTURE AGENT to analyze logs
   ↓
3. LOG CAPTURE identifies root cause
   ↓
4. If design flaw → consult EMBEDDED ARCHITECT
   ↓
5. C++ DEVELOPER implements fix
   ↓
6. TEST ENGINEER validates fix
   ↓
7. DOCUMENTATION WRITER updates troubleshooting
   ↓
8. ORCHESTRATOR commits and releases
```

### Workflow 3: Performance Optimization

```
1. Performance issue identified
   ↓
2. LOG CAPTURE AGENT measures current performance
   ↓
3. EMBEDDED ARCHITECT proposes optimization strategy
   ↓
4. C++ DEVELOPER implements changes
   ↓
5. LOG CAPTURE AGENT verifies improvement
   ↓
6. TEST ENGINEER ensures no regressions
   ↓
7. DOCUMENTATION WRITER updates performance notes
```

### Workflow 4: Safety-Critical Change

```
1. Change affecting safety proposed
   ↓
2. EMBEDDED ARCHITECT reviews architecture impact
   ↓
3. MARINE EXPERT validates marine safety
   ↓
4. Both approve → C++ DEVELOPER implements
   ↓
5. TEST ENGINEER runs full safety test suite
   ↓
6. LOG CAPTURE validates all safety checks pass
   ↓
7. DOCUMENTATION WRITER adds safety notes
   ↓
8. ORCHESTRATOR final review before commit
```

## Orchestrator Role (You)

As the orchestrator, you:

### Coordinate Workflow
- Receive user requests
- Route to appropriate agents
- Manage handoffs
- Track progress

### Ensure Quality
- Review all changes
- Verify safety
- Check documentation sync
- Validate test coverage

### Make Decisions
- Prioritize tasks
- Approve designs
- Resolve conflicts
- Schedule work

### Communicate
- Update user on progress
- Request clarifications
- Report blockers
- Summarize results

## Decision Matrix

Use this to route requests:

| Request Type | Primary Agent | Secondary Agent | Reviewer |
|--------------|---------------|-----------------|----------|
| Add feature | C++ Developer | Embedded Architect | Test Engineer |
| Fix bug | C++ Developer | Log Capture | Test Engineer |
| Optimize performance | Embedded Architect | C++ Developer | Log Capture |
| Debug issue | Log Capture | C++ Developer | - |
| Validate safety | Marine Expert | Test Engineer | Embedded Architect |
| Write tests | Test Engineer | C++ Developer | - |
| Update docs | Documentation Writer | C++ Developer | - |
| Design review | Embedded Architect | Marine Expert | C++ Developer |

## Agent Communication Protocol

### Requesting Work
```markdown
@agent-name

## Task: [Title]

[Details]

**Dependencies**: [What's needed first]
**Priority**: P0/P1/P2
**Deadline**: [If any]
```

### Reporting Results
```markdown
## Task Complete: [Title]

**Status**: ✅ Done / ⚠️ Issues / ❌ Blocked

**Changes Made**:
- [List]

**Artifacts**:
- [Files, logs, reports]

**Issues Encountered**:
- [If any]

**Next Steps**:
- [What needs to happen next]
```

### Requesting Help
```markdown
@agent-name

## Help Needed: [Topic]

**Context**: [Background]

**Question**: [Specific question]

**Blocking**: [What's waiting on this]
```

## Quality Gates

Before committing, verify:

### Code Quality
- [ ] Builds without errors/warnings
- [ ] RAM usage <50%
- [ ] Flash usage <90%
- [ ] Follows coding standards
- [ ] Relay safety maintained

### Testing
- [ ] All P0 tests pass
- [ ] No regressions
- [ ] Edge cases handled
- [ ] Logs captured and analyzed

### Safety
- [ ] Marine expert approved (if applicable)
- [ ] Architect reviewed (if major change)
- [ ] Safety tests pass
- [ ] Failure modes considered

### Documentation
- [ ] API docs updated
- [ ] User guide updated (if needed)
- [ ] Changelog entry added
- [ ] Architecture docs current

## Emergency Procedures

### Critical Bug in Production
1. **IMMEDIATE**: Issue stop command if user in danger
2. **LOG CAPTURE**: Capture failure logs
3. **C++ DEVELOPER**: Identify root cause
4. **EMBEDDED ARCHITECT**: Review if design flaw
5. **C++ DEVELOPER**: Implement hotfix
6. **TEST ENGINEER**: Quick validation
7. **ORCHESTRATOR**: Emergency release

### Memory/Performance Crisis
1. **LOG CAPTURE**: Measure current usage
2. **EMBEDDED ARCHITECT**: Identify bottleneck
3. **C++ DEVELOPER**: Implement optimization
4. **LOG CAPTURE**: Verify improvement
5. **TEST ENGINEER**: Regression check
6. **ORCHESTRATOR**: Release

### Design Conflict
1. **EMBEDDED ARCHITECT**: Present option A
2. **MARINE EXPERT**: Present safety view
3. **C++ DEVELOPER**: Present implementation view
4. **ORCHESTRATOR**: Make final decision
5. Document decision rationale

## File Organization

```
.claude/agents/
├── README.md (this file)               # Orchestrator guide
├── cpp-sensesp-developer.md           # Implementation agent
├── embedded-systems-architect.md      # Design agent
├── marine-domain-expert.md            # Domain agent
├── log-capture-agent.md               # Debug agent
├── test-engineer.md                   # Validation agent
└── documentation-writer.md            # Documentation agent
```

## Version History

- **v1.0** (2024-12-06): Initial agent team setup
  - Created 6 specialized agents
  - Defined workflows and protocols
  - Established quality gates

## Remember

> "A good orchestrator knows when to delegate, when to coordinate, and when to decide. The team is only as effective as the communication between its members. Keep handoffs clean, expectations clear, and quality high."

---

**Maintained By**: Orchestrator Agent
**Last Updated**: December 2024
**Project**: SensESP Chain Counter - Agent Team Management