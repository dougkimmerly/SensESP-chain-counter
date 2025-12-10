# Architecture Context

> System design, state machines, and component interactions.

## System Components

```
┌─────────────────────────────────────────────────────────────┐
│                    SignalK Network                          │
└─────────────┬─────────────────────────────────┬─────────────┘
              │                                 │
   ┌──────────▼──────────┐          ┌──────────▼──────────┐
   │  Depth/Wind Data    │          │  Distance (GPS)     │
   └──────────┬──────────┘          └──────────┬──────────┘
              │                                 │
   ┌──────────▼─────────────────────────────────▼──────────┐
   │                  ChainController                       │
   │  - Catenary physics        - Relay control            │
   │  - Slack calculation       - Position tracking        │
   │  - Speed calibration       - Timeout handling         │
   └──────────┬─────────────────────────────────┬──────────┘
              │                                 │
   ┌──────────▼──────────┐          ┌──────────▼──────────┐
   │  DeploymentManager  │          │  main.cpp           │
   │  (Automation FSM)   │          │  (Commands/Loop)    │
   └──────────┬──────────┘          └──────────┬──────────┘
              │                                 │
   ┌──────────▼─────────────────────────────────▼──────────┐
   │              Relay Hardware (GPIO 12/13)              │
   └───────────────────────────────────────────────────────┘
```

## Deployment State Machine

```
    ┌─────────┐
    │  IDLE   │
    └────┬────┘
         │ autoDeploy command
         ▼
    ┌─────────┐
    │  DROP   │  Deploy to depth - 2m
    └────┬────┘
         │ chain >= targetDropDepth
         ▼
    ┌───────────┐
    │ WAIT_TIGHT│  Wait for chain tension
    └────┬──────┘
         │ distance stable
         ▼
    ┌───────────┐
    │ HOLD_DROP │  Brief hold for stability
    └────┬──────┘
         │ timer elapsed
         ▼
    ┌───────────┐
    │ DEPLOY_30 │  Deploy to 30% scope
    └────┬──────┘  (monitors slack, pauses if > 85% depth)
         │ chain >= chain30
         ▼
    ┌─────────┐
    │ WAIT_30 │  Wait for boat to drift back
    └────┬────┘
         │ distance >= target
         ▼
    ┌─────────┐
    │ HOLD_30 │
    └────┬────┘
         │
    (continues through DEPLOY_75, WAIT_75, HOLD_75)
         │
         ▼
    ┌───────────┐
    │DEPLOY_100 │  Final deployment
    └────┬──────┘
         │ complete
         ▼
    ┌──────────┐
    │ COMPLETE │
    └──────────┘
```

## Retrieval Flow

```
autoRetrieve command
         │
         ▼
    ┌─────────────────────────────┐
    │ Calculate: amountToRaise    │
    │ = currentRode - 2.0m        │
    └────────────┬────────────────┘
                 │
         ┌───────▼───────┐
         │ raiseAnchor() │
         └───────┬───────┘
                 │
    ┌────────────▼────────────┐
    │ ChainController handles │
    │ - Slack monitoring      │
    │ - Auto pause/resume     │
    │ - Built-in timeout      │
    └────────────┬────────────┘
                 │
         ┌───────▼───────┐
         │   COMPLETE    │
         └───────────────┘
```

## Slack Monitoring During Retrieval

```
While raising:
    │
    ├─ Check: Is this final pull phase?
    │  (rode <= depth + bow_height + 3m)
    │  └─ YES → Skip slack checks, pull continuously
    │
    └─ Check: slack < PAUSE_SLACK (0.2m)?
       └─ YES → Pause motor
                Wait for slack >= RESUME_SLACK (1.0m)
                Resume motor
                (3s cooldown between actions)
```

## Data Flow

### Every 100ms (main loop)
```
1. chainController->control(current_position)
   - Check timeouts
   - Update relay state
   - Track position
```

### Every 500ms (main loop)
```
1. Read depth from SignalK listener
2. Read distance from SignalK listener  
3. Calculate slack using catenary physics
4. Publish slack to SignalK
5. If deploying: check slack limits
```

## Key State Variables

### ChainController
| Variable | Type | Purpose |
|----------|------|---------|
| `state_` | ChainState | IDLE/LOWERING/RAISING |
| `target_` | float | Target position (meters) |
| `paused_for_slack_` | bool | Currently paused for slack |
| `movement_start_time_` | unsigned long | For timeout calculation |

### DeploymentManager
| Variable | Type | Purpose |
|----------|------|---------|
| `currentStage` | DeploymentStage | Current FSM state |
| `isRunning` | bool | Deployment in progress |
| `chain30/75/100` | float | Target chain lengths |
| `targetDistance30/75` | float | Target distances |

## Timing

| Event | Interval | Purpose |
|-------|----------|---------|
| Control loop | 100ms | Motor control, timeout checks |
| Slack calculation | 500ms | Physics update, SignalK publish |
| Deployment monitor | 500ms | Stage transitions, safety checks |
| Slack cooldown | 3000ms | Prevent rapid pause/resume |
