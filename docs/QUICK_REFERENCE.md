# Quick Reference Guide

**Fast lookup for common questions and code locations**

## File Locations by Function

### Motor Control & Hardware
- **Chain position tracking**: [src/ChainController.cpp:100-150](../src/ChainController.cpp#L100-L150) - `getChainLength()`
- **Relay control (lower/raise)**: [src/ChainController.cpp:150-250](../src/ChainController.cpp#L150-L250) - `lowerAnchor()`, `raiseAnchor()`
- **Timeout handling**: [src/ChainController.cpp:250-300](../src/ChainController.cpp#L250-L300) - `updateTimeout()`, `control()`

### Slack Calculation & Physics
- **Main slack calculation**: [src/ChainController.cpp:299-350](../src/ChainController.cpp#L299-L350) - `calculateAndPublishHorizontalSlack()`
- **Bow height adjustment**: [src/ChainController.cpp:305-307](../src/ChainController.cpp#L305-L307) - `BOW_HEIGHT_M = 2.0`
- **Catenary reduction factor**: [src/ChainController.cpp:380-435](../src/ChainController.cpp#L380-L435) - `computeCatenaryReductionFactor()`
- **Wind force estimation**: [src/ChainController.cpp:353-378](../src/ChainController.cpp#L353-L378) - `estimateHorizontalForce()`

### Deployment Logic
- **Deployment state machine**: [src/DeploymentManager.cpp:154-387](../src/DeploymentManager.cpp#L154-L387) - `updateDeployment()`
- **Continuous deployment**: [src/DeploymentManager.cpp:92-110](../src/DeploymentManager.cpp#L92-L110) - `startContinuousDeployment()`
- **Deployment monitoring**: [src/DeploymentManager.cpp:112-151](../src/DeploymentManager.cpp#L112-L151) - `monitorDeployment()`
- **Safety brake**: [src/DeploymentManager.cpp:132-140](../src/DeploymentManager.cpp#L132-L140) - Slack limit check (85% of depth)

### Retrieval Logic
- **Retrieval state machine**: [src/RetrievalManager.cpp:90-232](../src/RetrievalManager.cpp#L90-L232) - `updateRetrieval()`
- **Cooldown period**: [src/RetrievalManager.h:45](../src/RetrievalManager.h#L45) - `COOLDOWN_AFTER_RAISE_MS = 3000`
- **Minimum raise amount**: [src/RetrievalManager.h:44](../src/RetrievalManager.h#L44) - `MIN_RAISE_AMOUNT_M = 1.0`
- **Raise hysteresis**: [src/RetrievalManager.h:43](../src/RetrievalManager.h#L43) - `MIN_SLACK_TO_START_M = 1.5`

### Main Firmware Loop
- **Entry point**: [src/main.cpp](../src/main.cpp)
- **Control loop timing**: ~100ms for chain position updates
- **Slack calculation**: Every 500ms in main loop
- **Speed calibration**: After each completed movement

---

## Constants Reference

### Safety Parameters (DeploymentManager)
```cpp
MAX_SLACK_RATIO = 0.85              // 85% of depth is max acceptable slack
MONITOR_INTERVAL_MS = 500            // Check conditions every 500ms
```

### Safety Parameters (RetrievalManager)
```cpp
MIN_SLACK_TO_START_M = 1.5           // Start considering raises at 1.5m slack
MIN_RAISE_AMOUNT_M = 1.0             // Only raise if at least 1.0m available
COOLDOWN_AFTER_RAISE_MS = 3000       // 3-second wait between raises
COMPLETION_THRESHOLD_M = 2.0         // Retrieval complete when rode ≤ 2.0m
FINAL_PULL_THRESHOLD_M = 10.0        // Switch to continuous pull when rode < depth + 10m
```

### Physics Constants (ChainController)
```cpp
CHAIN_WEIGHT_PER_METER_KG = 2.2      // Chain weight in water (buoyancy-adjusted)
GRAVITY = 9.81                        // m/s²
BOAT_WINDAGE_AREA_M2 = 15.0          // Effective windage area
AIR_DENSITY = 1.225                   // kg/m³ at sea level
DRAG_COEFFICIENT = 1.2                // Typical for hull + rigging
BOW_HEIGHT_M = 2.0                    // Bow height above waterline
```

---

## State Machine Diagrams

### Deployment States
```
IDLE
  ↓
DROP (initial drop to targetDropDepth)
  ↓
WAIT_TIGHT (wait for chain to come tight)
  ↓
HOLD_DROP (hold for stability)
  ↓
DEPLOY_30 (deploy 30% of total chain)
  ↓
WAIT_30 (wait for distance target)
  ↓
HOLD_30 (hold period)
  ↓
DEPLOY_75 (deploy 75% of total chain)
  ↓
WAIT_75 (wait for distance target)
  ↓
HOLD_75 (hold period)
  ↓
DEPLOY_100 (deploy full chain)
  ↓
COMPLETE
```

### Retrieval States
```
IDLE
  ↓
CHECKING_SLACK (check if slack ≥ threshold)
  ├─ If yes → RAISING
  └─ If no → WAITING_FOR_SLACK
  ↓
RAISING (chain moving up)
  ├─ Check for negative slack (stop if found)
  └─ When complete → WAITING_FOR_SLACK
  ↓
WAITING_FOR_SLACK (wait for slack to build up)
  ├─ Check if in final pull phase
  └─ When slack threshold met → CHECKING_SLACK
  ↓
COMPLETE
```

---

## Common Parameters

### Scope Ratio (Chain / Depth)
- **3:1**: Minimal, dangerous in strong wind
- **5:1**: Good for most conditions (our target)
- **7:1**: Excellent holding power, more chain deployed

### Catenary Reduction Factors
- **0.80**: Heavy sag (light forces, deep water)
- **0.85**: Moderate sag (typical conditions)
- **0.90**: Light sag (strong forces, shallow water)
- **0.99**: Nearly straight (extreme conditions)

### Slack Budget (by depth)
For deployment with MAX_SLACK_RATIO = 0.85:
- **3m depth**: Max slack = 2.55m
- **5m depth**: Max slack = 4.25m
- **10m depth**: Max slack = 8.5m
- **20m depth**: Max slack = 17.0m

---

## Debugging Tips

### Check Current Chain Length
```cpp
float current = chainController->getChainLength();  // meters
```

### Check Current Slack
```cpp
float slack = chainController->getHorizontalSlackObservable()->get();  // meters
```

### Check Current Depth
```cpp
float depth = chainController->getCurrentDepth();  // meters (absolute)
```

### Check Effective Depth (for slack calculation)
```cpp
float effective = depth - 2.0;  // Subtract 2m bow height
```

### Check if Relay is Active
```cpp
bool isMoving = chainController->isActive();  // true if LOWERING or RAISING
```

### Check if DeploymentManager is Running
```cpp
bool deploying = deploymentManager->isRunning;  // Check from DeploymentManager
```

### Check Tide-Adjusted Depth (for deployment)
```cpp
float tideAdjusted = chainController->getTideAdjustedDepth();  // meters at high tide
float tideNow = chainController->getTideHeightNow();           // current tide height
float tideHigh = chainController->getTideHeightHigh();         // high tide height
```

---

## Signal K Paths

**Published by ChainController:**
- `navigation.anchor.rodeDeployed` - Current chain length (meters)
- `navigation.anchor.chainSlack` - Calculated horizontal slack (meters)
- `navigation.anchor.chainDirection` - free fall / deployed / retrieving
- `environment.depth.belowSurface` - Water depth (meters)
- `navigation.anchor.distanceFromBow` - GPS distance to anchor

**Listened to by ChainController:**
- `environment.depth.belowSurface` - Water depth updates
- `navigation.anchor.distanceFromBow` - Distance updates
- `environment.wind.speedTrue` - Wind speed for catenary calculation
- `environment.tide.heightNow` - Current tide height for tide-adjusted deployment
- `environment.tide.heightHigh` - High tide height for tide-adjusted deployment

---

## Build Commands

```bash
# Full clean build
~/.platformio/penv/bin/platformio run -t clean && ~/.platformio/penv/bin/platformio run

# Quick rebuild (no clean)
~/.platformio/penv/bin/platformio run

# Monitor serial output
~/.platformio/penv/bin/platformio device monitor --baud 115200

# Build and upload
~/.platformio/penv/bin/platformio run --target upload
```

---

## When to Use Each Doc

| Question | Document |
|----------|----------|
| "How does the overall system work?" | [CODEBASE_OVERVIEW.md](CODEBASE_OVERVIEW.md) |
| "What's the deployment/retrieval logic?" | [DEPLOYMENT_REFACTOR_PLAN.md](DEPLOYMENT_REFACTOR_PLAN.md) |
| "How do I use ChainController API?" | [mcp_chaincontroller.md](mcp_chaincontroller.md) |
| "How do I build and run the firmware?" | [README.md](../README.md) |
| "Where's the code for X feature?" | This file (QUICK_REFERENCE.md) |
| "What should I read first?" | [claude.md](../claude.md) |

---

Last Updated: 2025-12-02
