# Message Queue Handoff System

Async communication between Project Manager (PM/Web Claude) and Claude Code (CC) sessions.

## Files

| File | Who Writes | Purpose |
|------|-----------|--------|
| `task-queue.md` | PM | Tasks to be executed |
| `response-queue.md` | CC | Responses from completed tasks |
| `task-archive.md` | CC | Completed tasks (audit trail) |
| `response-archive.md` | PM | Archived responses (audit trail) |

## Claude Code Instructions

When you see `msg` from user:

1. **Pull latest** → `git pull`
2. **Check tasks** → Read `task-queue.md`
3. **Execute** the task fully
4. **Write RESPONSE** → Add to `response-queue.md` (CRITICAL - not task-queue.md!)
5. **Archive task** → Move to `task-archive.md` with pickup date
6. **Clear queue** → Leave `task-queue.md` with header comment only
7. **Commit & push:**
   ```bash
   git add .claude/handoff/
   git commit -m "Handoff: Archive TASK-XXX and write RESPONSE-XXX"
   git push
   ```
