# /resp - PM Response Check

> **PM ONLY** - This command is for Project Manager (Web Claude) sessions.
> CC uses `/msg` instead.

When PM runs `resp`, check for CC responses and manage the queue.

## When User Says `resp`

1. **Fetch response queue** via GitHub MCP:
   ```
   github:get_file_contents
     path: .claude/handoff/response-queue.md
   ```

2. **If response exists:**
   - Summarize to user
   - **Archive immediately** (no confirmation needed)
   - Move to `response-archive.md` with timestamp
   - Clear `response-queue.md` (leave header only)

3. **If empty:** Tell user "No pending responses"

## Auto-Archive Rule

**Always archive after reading. Never ask "do you want me to archive?" - just do it.**

## Queuing New Tasks

After archiving (or if queue empty), PM can queue new tasks to `task-queue.md`.
Then tell user: "Task queued. Have CC run `msg` in the project terminal."
