# Architecture Map - System Overview

**Visual guide to how components interact**

---

## High-Level System Flow

```
┌──────────────────────────────────────────────────────────────────┐
│                        Signal K Network                           │
│  (Marine Instrument Data Bus)                                    │
└──────────────────┬───────────────────────────────┬───────────────┘
                   │                               │
        ┌──────────▼──────────┐        ┌──────────▼──────────┐
        │  Depth Data         │        │  Distance Data      │
        │  Wind Speed Data    │        │  (GPS)              │
        └──────────┬──────────┘        └──────────┬──────────┘
                   │                               │
        ┌──────────▼──────────────────────────────▼──────────┐
        │                                                    │
        │           ChainController                         │
        │    (Physics & Motor Control)                      │
        │                                                    │
        │  - Catenary physics calculations                  │
        │  - Slack calculation (w/ bow height offset)       │
        │  - Relay control (up/down)                        │
        │  - Position tracking (from accumulator)           │
        │  - Speed calibration                              │
        │                                                    │
        │  Output: Chain length, slack, depth, distance     │
        └──────────┬──────────────────────────────┬──────────┘
                   │                               │
        ┌──────────▼──────────┐        ┌──────────▼──────────┐
        │  DeploymentManager  │        │  autoRetrieve       │
        │  (Automation FSM)   │        │  (direct command)   │
        └──────────┬──────────┘        └──────────┬──────────┘
                   │                               │
        ┌──────────▼──────────────────────────────▼──────────┐
        │                                                    │
        │         main.cpp (Firmware Loop)                  │
        │  - 100ms: Call chainController->control()         │
        │  - 500ms: Calculate & publish slack               │
        │  - Handle deployment/retrieval state machines     │
        │                                                    │
        └──────────┬──────────────────────────────┬──────────┘
                   │                               │
        ┌──────────▼─────────────────────────────▼───────────┐
        │      Relay Hardware (Motor Control)                │
        │  - Pin 12: Down Relay (lower chain)               │
        │  - Pin 13: Up Relay (raise chain)                 │
        └────────────────────────────────────────────────────┘
```

---

## Component Interactions

### ChainController → Signal K

**ChainController publishes:**
```
┌─ navigation.anchor.rodeDeployed
│  └─ Current chain length in meters
├─ navigation.anchor.chainSlack
│  └─ Calculated horizontal slack
├─ navigation.anchor.chainDirection
│  └─ "free fall" / "deployed" / "retrieving"
└─ (Also republishes received depth & distance)
```

**ChainController listens to:**
```
┌─ environment.depth.belowSurface
│  └─ Water depth (for slack calculation)
├─ navigation.anchor.distanceFromBow
│  └─ GPS distance to anchor
└─ environment.wind.speedTrue
   └─ Wind speed (for catenary force estimation)
```

---

## Deployment State Flow

```
START → isAutoAnchorValid()? ──NO──→ (do nothing)
         │
        YES
         │
         ▼
    ┌─────────────────────┐
    │  DROP               │    Deploy to targetDropDepth
    │  ↓ chain >= depth   │    (typically depth - 2m)
    └─────────────────────┘
         │
         ▼
    ┌─────────────────────┐
    │  WAIT_TIGHT         │    Wait for chain to come tight
    │  ↓ distance checks  │    (monitor movement)
    └─────────────────────┘
         │
         ▼
    ┌─────────────────────┐
    │  HOLD_DROP          │    Hold period for stability
    │  ↓ timer elapsed    │    (1-2 seconds typical)
    └─────────────────────┘
         │
         ▼
    ┌─────────────────────────────────────────┐
    │  DEPLOY_30 (continuous w/ monitoring)   │
    │  ↓ chain >= chain30                     │
    │  Monitor: slack ≤ 0.85 × depth?        │
    │  - Excessive slack? → Pause & resume    │
    └─────────────────────────────────────────┘
         │
         ▼
    ┌─────────────────────┐
    │  WAIT_30            │    Wait for horizontal distance
    │  ↓ distance >= target │  to reach target
    └─────────────────────┘
         │
         ▼
    ┌─────────────────────┐
    │  HOLD_30            │    Hold period
    │  ↓ timer elapsed    │
    └─────────────────────┘
         │
         ▼
    ┌─────────────────────────────────────────┐
    │  DEPLOY_75 (continuous w/ monitoring)   │
    │  ↓ chain >= chain75                     │
    │  (same monitoring as DEPLOY_30)         │
    └─────────────────────────────────────────┘
         │
         ▼
    ┌─────────────────────┐
    │  WAIT_75            │
    │  ↓ distance >= target │
    └─────────────────────┘
         │
         ▼
    ┌─────────────────────┐
    │  HOLD_75            │
    │  ↓ timer elapsed    │
    └─────────────────────┘
         │
         ▼
    ┌─────────────────────────────────────────┐
    │  DEPLOY_100 (continuous to full length) │
    │  ↓ chain >= totalChainLength            │
    │  (final deployment with monitoring)     │
    └─────────────────────────────────────────┘
         │
         ▼
    ┌─────────────────────┐
    │  COMPLETE           │
    │  → stop()           │    Deployment finished
    └─────────────────────┘
```

---

## Retrieval Flow

```
START → autoRetrieve command received
         │
         ▼
    ┌─────────────────────────────────────────┐
    │  Calculate amount to raise              │
    │  amountToRaise = currentRode - 2.0m     │
    └──────────────┬──────────────────────────┘
                   │
         ┌─────────▼──────────┐
         │  Single raise      │
         │  command           │
         │                    │
         │  raiseAnchor(amt)  │
         └─────────┬──────────┘
                   │
         ┌─────────▼──────────┐
         │  ChainController   │
         │  handles movement  │
         │  (with built-in    │
         │  timeout & slack-  │
         │  based pause/      │
         │  resume)           │
         └─────────┬──────────┘
                   │
    (Movement completes or ChainController timeout occurs)
                   │
         ┌─────────▼──────────┐
         │  COMPLETE          │
         │  stop()            │
         └────────────────────┘
```

Note: Slack-based pause/resume logic is handled automatically by ChainController during raising. No external timeout needed - ChainController has built-in movement timeout.

---

## Slack Calculation Flow

```
Input: chain_length, depth (from sensor)
       │
       ▼
Account for bow height
  effective_depth = depth - 2.0m
       │
       ▼
Validate inputs
  chain > 0.01 && depth > 0.01?
  ├─ NO  → return slack = 0
  └─ YES
       │
       ▼
Calculate horizontal force
  ├─ Get wind speed from Signal K
  ├─ Estimate wind drag: F_wind = 0.5 × ρ × Cd × A × v²
  └─ Add baseline for current: F_total = F_wind + 30N
       │
       ▼
Calculate catenary reduction
  ├─ Compute scope ratio: chain / effective_depth
  ├─ Based on force & ratio, get reduction factor (0.80-0.99)
  └─ Multiply by factor
       │
       ▼
Calculate straight-line distance
  horizontal_dist_taut = sqrt(chain² - effective_depth²) × factor
       │
       ▼
Calculate slack
  if distance_from_gps == 0:
    slack = horizontal_dist_taut  (all chain sag is on seabed)
  else:
    slack = horizontal_dist_taut - distance_from_gps
       │
       ▼
Output: slack (meters) → publish to Signal K
```

---

## Continuous Deployment Monitoring

```
START DEPLOY_30:
  │
  ├─ startContinuousDeployment(chain30)
  │  ├─ Calculate amount: chain30 - current_chain
  │  ├─ Call: chainController->lowerAnchor(amount)
  │  └─ Schedule: monitorDeployment() every 500ms
  │
  └─ While isRunning && in DEPLOY_30 stage:
     │
     ├─ monitorDeployment() runs every 500ms:
     │  ├─ Check if chain >= target
     │  │  └─ If yes: transition to WAIT_30
     │  │
     │  └─ Check slack level:
     │     ├─ If slack > 0.85 × depth:
     │     │  └─ Stop relay (pause deployment)
     │     │
     │     └─ If slack ≤ threshold && relay inactive:
     │        └─ Resume: chainController->lowerAnchor(remaining)
     │
     └─ When target reached or stopped:
        └─ transitionTo(WAIT_30)
```

---

## When Things Go Wrong

### Problem: Slack exceeds safety limit during deployment

**Trigger:** `slack > 0.85 × effective_depth`

**Response:**
```
1. monitorDeployment() detects excessive slack
2. Calls: chainController->stop()
3. Relay deactivates (motor stops)
4. Stays paused in DEPLOY_XX stage
5. Monitoring continues every 500ms
6. When slack drops back below limit:
   → Automatically resumes: chainController->lowerAnchor(remaining)
```

### Problem: Negative slack during retrieval

**Trigger:** `slack < 0` while raising (chain became taut)

**Response:**
```
1. ChainController's slack monitoring detects negative slack
2. Calls: stop() to pause raising
3. Relay deactivates (windlass motor stops)
4. Waits for slack to build up (boat drifts)
5. Resumes raising when slack ≥ threshold
6. This pause/resume happens automatically within the raiseAnchor() operation
```

### Problem: Relay gets stuck (timeout)

**Trigger:** Movement timeout exceeded

**Response:**
```
1. control() loop detects timeout
2. Deactivates relay (force stop)
3. Calculates actual speed (vs expected)
4. Updates speed estimates for future movements
5. Logs warning if significantly different
```

---

## Data Flow During Deployment

```
Time = t0: Deployment starts
  │
  ├─ Read from ChainController:
  │  ├─ Current chain length (from accumulator)
  │  ├─ Current depth (from Signal K listener)
  │  ├─ Current distance (from Signal K listener)
  │  └─ Current slack (calculated)
  │
  ├─ DeploymentManager evaluates:
  │  ├─ Current stage & target
  │  ├─ Safety conditions (slack < limit?)
  │  └─ Stage completion (reached target?)
  │
  ├─ Possible actions:
  │  ├─ Call lowerAnchor() to deploy more
  │  ├─ Call stop() to pause
  │  └─ Transition to next stage
  │
  └─ Publish to Signal K:
     ├─ navigation.anchor.rodeDeployed
     ├─ navigation.anchor.chainSlack
     └─ navigation.anchor.chainDirection

Time = t0+500ms: Next monitoring cycle
  └─ (same flow repeats)
```

---

## Safety Limits & Thresholds

| Parameter | Value | Purpose |
|-----------|-------|---------|
| MAX_SLACK_RATIO | 0.85 | Max slack = 85% of depth (deployment safety) |
| MONITOR_INTERVAL_MS | 500 | Check conditions every 500ms |
| PAUSE_SLACK_M | 0.2 | Pause raising when slack drops below this |
| RESUME_SLACK_M | 1.0 | Resume raising when slack builds to this |
| SLACK_COOLDOWN_MS | 3000 | 3s cooldown between pause/resume actions |
| FINAL_PULL_THRESHOLD_M | 3.0 | Skip slack checks when rode < depth+bow+3m |
| BOW_HEIGHT_M | 2.0 | Bow height above water (slack calculation) |
| CHAIN_WEIGHT_PER_METER_KG | 2.2 | Chain weight in water (catenary) |

---

## How to Add a Feature

### Scenario: Add a "holding power" indicator

1. **Identify what you need:**
   - Scope ratio: `chain_length / effective_depth`
   - Display: "Poor" (<3:1), "Good" (3-5:1), "Excellent" (5-7:1), "Excessive" (>7:1)

2. **Decide where to calculate:**
   - Option A: In ChainController (low-level, reusable)
   - Option B: In main.cpp (high-level, one-time use)
   - Choose: ChainController (more reusable)

3. **Implement:**
   ```cpp
   // In ChainController.h:
   float computeScopeRatio(float chainLength, float depth);

   // In ChainController.cpp:
   float ChainController::computeScopeRatio(float chainLength, float depth) {
       float effective_depth = fmax(0.0, depth - 2.0);  // bow height
       if (effective_depth <= 0.01) return 0.0;
       return chainLength / effective_depth;
   }
   ```

4. **Publish to Signal K:**
   - Add to main.cpp: publish computed ratio to Signal K path
   - Example: `navigation.anchor.scopeRatio`

5. **Test:**
   - Verify at various chain lengths
   - Verify at various depths
   - Check Signal K publication

---

Last Updated: 2025-12-02
