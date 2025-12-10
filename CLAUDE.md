# SensESP Chain Counter

ESP32-based anchor chain deployment/retrieval system with SignalK integration. C++/PlatformIO embedded project.

## Quick Reference

| Action | Command |
|--------|---------|
| Build | `pio run` |
| Upload | `pio run --target upload` |
| Monitor | `pio device monitor --baud 115200` |
| Clean build | `pio run -t clean && pio run` |

## Key Files

| To understand... | Read |
|------------------|------|
| Motor control & physics | `src/ChainController.cpp` |
| Deployment state machine | `src/DeploymentManager.cpp` |
| Main loop & commands | `src/main.cpp` |
| Hardware constants | `src/ChainController.h` |

## Critical Rules

> **These WILL break things if ignored.**

1. **Bow Height = 2m**: All slack calculations subtract 2m for bow-to-water distance. Changing this breaks physics.
2. **Slack thresholds**: Pause at 0.2m, resume at 1.0m. These prevent chain snapping during retrieval.
3. **Final pull phase**: When rode ≤ depth + bow + 3m, slack checks are skipped (anchor coming up).
4. **Relay pins**: DOWN=12, UP=13. Activating both simultaneously will damage hardware.

## Architecture at a Glance

```
SignalK Server
    ↓ depth, distance, wind
ESP32 (this firmware)
    ├── ChainController (physics, motor control)
    ├── DeploymentManager (automation FSM)
    └── main.cpp (commands, loop)
    ↓ relay signals
Windlass Motor
```

**Core flow**: Command received → Calculate slack → Control relay → Publish state

## Commands

| Command | When to use |
|---------|-------------|
| `/implement` | Adding features, modifying control logic |
| `/debug` | Diagnosing issues, analyzing serial logs |
| `/review` | Code review, safety checks |
| `/test` | Testing procedures, validation |

## Context Files

> **Load as needed, not upfront.** Located in `.claude/context/`

| File | Contains |
|------|----------|
| `architecture.md` | State machines, data flow, component interactions |
| `safety.md` | Hardware limits, timing constraints, failure modes |
| `domain.md` | SignalK paths, marine physics, catenary math |
| `patterns.md` | C++/SensESP patterns, coding standards |
| `sensesp-guide.md` | SensESP framework patterns, GPIO, producers/transforms |
| `signalk-guide.md` | SignalK API reference, paths, units |

## Hardware

| Component | Pin | Notes |
|-----------|-----|-------|
| Down Relay | GPIO 12 | Lowers chain |
| Up Relay | GPIO 13 | Raises chain |
| Hall Sensor | GPIO 27 | Counts gypsy rotations |

## Key Constants

| Parameter | Value | Location |
|-----------|-------|----------|
| Bow height | 2.0m | `ChainController.h` |
| Pause slack | 0.2m | `ChainController.h` |
| Resume slack | 1.0m | `ChainController.h` |
| Final pull threshold | 3.0m | `ChainController.h` |
| Max slack ratio | 85% of depth | `DeploymentManager.h` |
