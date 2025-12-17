# Task Queue

<!-- PM writes tasks here. CC picks them up with msg -->

## TASK-001
**Created:** 2025-12-17 13:15
**Priority:** Medium
**Title:** Make project fully compliant with claude-project-framework

### Context
The project is missing the Message Queue Protocol and PM GitHub Access sections in CLAUDE.md. These are required per PROJECT-STANDARDS.md.

### Required Changes

#### 1. Add Message Queue Protocol section to CLAUDE.md

Add this section before the Hardware section:

```markdown
## Message Queue Protocol

For async PM ↔ CC communication:

| File | Direction | Purpose |
|------|-----------|--------|
| `.claude/handoff/task-queue.md` | PM → CC | Tasks to execute |
| `.claude/handoff/response-queue.md` | CC → PM | Task responses |
| `.claude/handoff/task-archive.md` | CC | Audit trail of completed tasks |
| `.claude/handoff/response-archive.md` | PM | Archived responses |

**When you see `msg`:**
1. `git pull`
2. Read `task-queue.md`
3. Execute task fully
4. Write RESPONSE to `response-queue.md` (NOT task-queue!)
5. Archive task to `task-archive.md` with pickup date
6. Clear `task-queue.md` (leave header only)
7. `git add .claude/handoff/ && git commit -m "Handoff: ..." && git push`

**Response format:**
```
## RESPONSE-XXX
**Task:** TASK-XXX
**Status:** COMPLETE | BLOCKED | NEEDS_INFO
**Commit:** abc1234

### Summary
Brief description.

### Changes Made
- File: change

### Verification
- ✅ Criterion met
```
```

#### 2. Add PM GitHub Access section to CLAUDE.md

Add this section after Message Queue Protocol:

```markdown
## PM GitHub Access

| Field | Value |
|-------|-------|
| Owner | `dougkimmerly` |
| Repo | `SensESP-chain-counter` |
| Branch | `main` |
```

### Acceptance Criteria
- [ ] Message Queue Protocol section added to CLAUDE.md
- [ ] PM GitHub Access section added to CLAUDE.md
- [ ] CLAUDE.md still under 150 lines
- [ ] All 4 handoff files exist in `.claude/handoff/`

### Verification
```bash
wc -l CLAUDE.md  # Should be < 150
ls .claude/handoff/  # Should show 4 files
```
