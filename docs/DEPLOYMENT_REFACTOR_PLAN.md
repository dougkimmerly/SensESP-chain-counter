# Plan: Transition from Speed-Based to Slack-Based Chain Deployment

## Problem Statement
Current deployment is based on boat speed calculation, but chain is being deployed faster than the boat is actually moving away from the anchor. The boat may have velocity components in multiple directions (not just directly away from anchor), making speed-based deployment inaccurate.

## Root Cause Analysis

### Current Speed-Based System
**How it works:**
- Samples GPS distance (`navigation.anchor.distanceFromBow`) every 100ms
- Calculates radial velocity: `speed = (currentDistance - lastDistance) / dt`
- Uses EWMA smoothing (α=0.3) for noise reduction
- Deploys chain based on: `desired_payout = boat_speed × time_window × slack_factor(1.25)`
- Accumulates demand until ≥1.0m threshold, then deploys

**The Problem:**
While the speed calculation DOES track radial velocity (directly away from anchor), the issue is that:
1. **GPS distance updates every 2s** but speed is calculated every 100ms (potential stale data)
2. **slack_factor=1.25** means system deploys 25% MORE than boat actually moves
3. **Demand accumulation** can build up during drift, then deploy in large pulse
4. **Speed measurement noise** can cause over-estimation of drift rate

### Current Slack Calculation
**How it works:**
- Calculated in ChainController every 500ms
- Formula: `slack = sqrt(chain² - depth²) - current_distance`
- Uses simplified straight-line physics (no catenary sag accounted for)
- Published when change >1cm

**Key Finding:**
- Slack calculation is LESS accurate than DeploymentManager's catenary physics
- ChainController uses Pythagorean theorem
- DeploymentManager has sophisticated catenary model with chain weight and force
- This discrepancy means slack value may overestimate available slack

## Investigation Complete

### Current Architecture
1. **Speed-based deployment** triggers when accumulated demand ≥1.0m
2. **Slack-based safety brake** pauses when slack >50% of depth
3. **Two control loops competing**: speed drives deployment, slack only stops it

### Key Insight
The system ALREADY has slack monitoring but uses it only as a safety limit, not as the primary control signal. The slack check happens AFTER deployment is triggered by speed-based demand.

## User Requirements

1. **Target slack**: 10% of rode deployed (simple, scales with chain length)
2. **Physics consolidation**: Use catenary physics in ONE place only
3. **Deployment behavior**: Deploy based on rate-of-slack-change (aggressive when slack dropping fast)
4. **Architecture**: Complete replacement of speed-based system (keep it simple)
5. **Depth range**: Primarily shallow water but must work at all depths

## Recommended Solution

### Architecture Decision: Where to Centralize Catenary Physics

**Recommendation: ChainController**

**Rationale:**
1. **Single Source of Truth**: ChainController already calculates slack every 500ms and publishes it
2. **Separation of Concerns**: ChainController owns chain position/state; DeploymentManager owns deployment strategy
3. **Reusability**: Slack calculation used by both DeploymentManager AND RetrievalManager
4. **Signal K Integration**: ChainController already publishes slack to `navigation.anchor.chainSlack`
5. **Simplicity**: One calculation point, consumed by multiple clients

**Current Duplication:**
- ChainController: `computeTargetHorizontalDistance()` - simple Pythagorean
- DeploymentManager: `computeCatenaryReductionFactor()` + `estimateHorizontalForce()` - sophisticated physics

**Solution:** Move DeploymentManager's catenary logic to ChainController, delete DeploymentManager's version.

---

## Implementation Plan

### Phase 1: Improve ChainController Slack Calculation

**File:** `src/ChainController.cpp` and `src/ChainController.h`

#### Step 1.1: Add Force Estimation to ChainController

Move wind speed listener and force estimation from DeploymentManager to ChainController:

```cpp
// In ChainController.h - add member variables:
sensesp::SKValueListener<float>* windSpeedListener_;
float estimateHorizontalForce();

// Constants (copy from DeploymentManager.h):
static constexpr float CHAIN_WEIGHT_PER_METER_KG = 2.2;
static constexpr float GRAVITY = 9.81;
static constexpr float BASELINE_CURRENT_FORCE_N = 50.0;
static constexpr float WIND_DRAG_COEFF = 5.0;
```

#### Step 1.2: Replace computeTargetHorizontalDistance() with Catenary-Aware Version

**Current (simple Pythagorean):**
```cpp
float ChainController::computeTargetHorizontalDistance(float chainLength, float depth) {
    return sqrt(pow(chainLength, 2) - pow(depth, 2));
}
```

**New (catenary-aware):**
```cpp
float ChainController::computeTargetHorizontalDistance(float chainLength, float depth) {
    // Simple validation
    if (chainLength <= depth) return 0.0;

    // Get estimated horizontal force
    float horizontalForce = estimateHorizontalForce();

    // Calculate straight-line distance
    float straightLineDistance = sqrt(pow(chainLength, 2) - pow(depth, 2));

    // Apply catenary reduction
    float reductionFactor = computeCatenaryReductionFactor(chainLength, depth, horizontalForce);

    return straightLineDistance * reductionFactor;
}
```

#### Step 1.3: Add computeCatenaryReductionFactor() to ChainController

Copy the sophisticated catenary calculation from DeploymentManager:

```cpp
float ChainController::computeCatenaryReductionFactor(float chainLength, float anchorDepth, float horizontalForce) {
    // [Copy exact implementation from DeploymentManager.cpp lines 197-253]
    // This accounts for chain weight, horizontal force, and catenary sag
}
```

---

### Phase 2: Remove Speed Measurement from DeploymentManager

**File:** `src/DeploymentManager.cpp` and `src/DeploymentManager.h`

#### Step 2.1: Delete Speed Measurement Code

**Remove from DeploymentManager.h:**
- `bool measuring`
- `float ewmaSpeed`
- `float ewmaDistance`
- `unsigned long lastTime`
- `float lastDistance`
- `float lastRawDistanceForEWMA`
- `sensesp::RepeatEvent* speedUpdateEvent`
- `static constexpr float alpha`
- `static constexpr float distance_alpha`
- `void startSpeedMeasurement()`
- `void stopSpeedMeasurement()`
- `float getCurrentSpeed()`
- `void strBoatSpeed()` (if only used for speed measurement)

**Remove from DeploymentManager.cpp:**
- Entire `startSpeedMeasurement()` function (lines ~108-148)
- Entire `stopSpeedMeasurement()` function
- Entire `getCurrentSpeed()` function (lines ~158-161)
- `strBoatSpeed()` function (lines 27-29)
- Speed measurement initialization in constructor

#### Step 2.2: Delete Speed-Based Demand Accumulation

**Remove from DeploymentManager.h:**
- `float _accumulatedDeployDemand`
- `const float MIN_DEPLOY_THRESHOLD_M`
- `float _lastDistanceAtDeploymentCommand`

**Remove from startDeployPulse()** (lines ~314-324):
```cpp
// DELETE THIS BLOCK:
float desired_payout_this_cycle = 0.0;
if (current_boat_speed_mps <= 0.1) {
    desired_payout_this_cycle = 0.0;
} else {
    float boat_movement_in_window = current_boat_speed_mps * (DECISION_WINDOW_MS / 1000.0);
    desired_payout_this_cycle = boat_movement_in_window * slack_factor;
}
_accumulatedDeployDemand += desired_payout_this_cycle;
```

---

### Phase 3: Implement Slack-Based Deployment Control

**File:** `src/DeploymentManager.cpp`

#### Step 3.1: Add Slack Tracking Variables

**Add to DeploymentManager.h:**
```cpp
// Slack-based control
float lastSlack_;              // Previous slack value for rate calculation
unsigned long lastSlackTime_;  // Timestamp of last slack measurement
float slackRate_;              // Rate of slack change (m/s), negative = decreasing

// Target slack: 10% of deployed chain
static constexpr float TARGET_SLACK_RATIO = 0.10;

// Deployment aggressiveness based on slack rate
static constexpr float AGGRESSIVE_SLACK_RATE_THRESHOLD = -0.05;  // -5cm/s = deploy more
static constexpr float NORMAL_DEPLOY_INCREMENT = 0.5;            // 0.5m increments
static constexpr float AGGRESSIVE_DEPLOY_INCREMENT = 1.0;        // 1.0m when slack dropping fast
```

#### Step 3.2: Initialize Slack Tracking in start()

**In DeploymentManager::start()**, replace speed measurement with slack tracking:
```cpp
// Initialize slack tracking
lastSlack_ = chainController->getHorizontalSlackObservable()->get();
lastSlackTime_ = millis();
slackRate_ = 0.0;
```

#### Step 3.3: Rewrite startDeployPulse() Logic

**Replace the entire decision logic** (lines ~290-368) with:

```cpp
void DeploymentManager::startDeployPulse(float stageTargetChainLength) {
    if (deployPulseEvent != nullptr) {
        sensesp::event_loop()->remove(deployPulseEvent);
        deployPulseEvent = nullptr;
    }

    deployPulseEvent = sensesp::event_loop()->onDelay(0, [this, stageTargetChainLength]() {
        // Check if still running
        if (!isRunning || (currentStage != DEPLOY_30 && currentStage != DEPLOY_75 && currentStage != DEPLOY_100)) {
            ESP_LOGD(__FILE__, "Deploy Pulse: Not in active deploy stage. Stopping.");
            return;
        }

        // Wait if ChainController is busy
        if (this->chainController->isActivelyControlling()) {
            ESP_LOGD(__FILE__, "Deploy Pulse: ChainController busy, rescheduling...");
            deployPulseEvent = sensesp::event_loop()->onDelay(DECISION_WINDOW_MS, [this, stageTargetChainLength]() {
                startDeployPulse(stageTargetChainLength);
            });
            return;
        }

        // --- Get current state ---
        float current_chain = this->chainController->getChainLength();
        float current_slack = this->chainController->getHorizontalSlackObservable()->get();
        unsigned long now = millis();

        // --- Calculate slack rate-of-change ---
        float dt = (now - lastSlackTime_) / 1000.0;  // seconds
        if (dt > 0.1) {  // Update if at least 100ms elapsed
            slackRate_ = (current_slack - lastSlack_) / dt;  // m/s (negative = slack decreasing)
            lastSlack_ = current_slack;
            lastSlackTime_ = now;
        }

        // --- Calculate target slack: 10% of deployed chain ---
        float target_slack = current_chain * TARGET_SLACK_RATIO;
        float slack_deficit = target_slack - current_slack;  // Positive = need more slack (deploy chain)

        // --- Determine if we should deploy ---
        bool shouldDeploy = false;
        float deploy_amount = 0.0;

        if (slack_deficit > 0.2) {  // Need at least 20cm more slack to deploy
            shouldDeploy = true;

            // Choose deployment amount based on slack rate
            if (slackRate_ < AGGRESSIVE_SLACK_RATE_THRESHOLD) {
                // Slack dropping fast - deploy aggressively
                deploy_amount = AGGRESSIVE_DEPLOY_INCREMENT;
                ESP_LOGI(__FILE__, "Deploy Pulse: Slack dropping fast (%.3f m/s). Deploying %.2f m aggressively.",
                         slackRate_, deploy_amount);
            } else {
                // Normal deployment
                deploy_amount = NORMAL_DEPLOY_INCREMENT;
                ESP_LOGD(__FILE__, "Deploy Pulse: Slack deficit %.2f m, deploying %.2f m normally.",
                         slack_deficit, deploy_amount);
            }

            // Don't exceed stage target
            if (current_chain + deploy_amount > stageTargetChainLength) {
                deploy_amount = stageTargetChainLength - current_chain;
                if (deploy_amount < 0.1) shouldDeploy = false;  // Too close to target
            }
        }

        // --- Check maximum slack limit (safety brake) ---
        float current_depth = this->chainController->getCurrentDepth();
        float max_acceptable_slack = current_depth * 0.5;  // Still use 50% of depth as safety limit

        if (current_slack > max_acceptable_slack) {
            shouldDeploy = false;
            ESP_LOGI(__FILE__, "Deploy Pulse: Excessive slack (%.2f m > %.2f m). Pausing deployment.",
                     current_slack, max_acceptable_slack);
        }

        // --- Execute deployment or wait ---
        unsigned long next_pulse_delay_ms = DECISION_WINDOW_MS;

        if (shouldDeploy) {
            ESP_LOGI(__FILE__, "Deploy Pulse: Deploying %.2f m (chain: %.2f, slack: %.2f, target_slack: %.2f, rate: %.3f m/s)",
                     deploy_amount, current_chain, current_slack, target_slack, slackRate_);

            this->chainController->lowerAnchor(deploy_amount);

            // Wait for deployment to complete
            next_pulse_delay_ms = (unsigned long)(deploy_amount * this->chainController->getDownSpeed()) + 200;
            if (next_pulse_delay_ms < DECISION_WINDOW_MS) {
                next_pulse_delay_ms = DECISION_WINDOW_MS;
            }
        } else {
            ESP_LOGD(__FILE__, "Deploy Pulse: No deployment needed. Chain: %.2f, Slack: %.2f, Target: %.2f",
                     current_chain, current_slack, target_slack);
        }

        // --- Schedule next pulse ---
        deployPulseEvent = sensesp::event_loop()->onDelay(next_pulse_delay_ms, [this, stageTargetChainLength]() {
            startDeployPulse(stageTargetChainLength);
        });
    });
}
```

---

### Phase 4: Cleanup - Remove Duplicate Physics from DeploymentManager

**File:** `src/DeploymentManager.cpp` and `src/DeploymentManager.h`

#### Step 4.1: Delete Duplicate Catenary Functions

**Remove from DeploymentManager.cpp:**
- `computeCatenaryReductionFactor()` (lines ~197-253)
- `estimateHorizontalForce()` (lines ~164-195)

**Remove from DeploymentManager.h:**
- `float computeCatenaryReductionFactor(...)`
- `float estimateHorizontalForce()`
- `sensesp::SKValueListener<float>* windSpeedListener_`
- Constants: `CHAIN_WEIGHT_PER_METER_KG`, `GRAVITY`, `BASELINE_CURRENT_FORCE_N`, `WIND_DRAG_COEFF`

#### Step 4.2: Update DeploymentManager::start()

The catenary distance calculations will now automatically use ChainController's improved physics:

**Current code (lines 50-54):**
```cpp
targetDistanceInit = computeTargetHorizontalDistance(targetDropDepth, anchorDepth);
float reductionFactor30 = computeCatenaryReductionFactor(chain30, anchorDepth, estimatedHorizontalForce);
float reductionFactor75 = computeCatenaryReductionFactor(chain75, anchorDepth, estimatedHorizontalForce);
targetDistance30 = computeTargetHorizontalDistance(chain30, anchorDepth) * reductionFactor30;
targetDistance75 = computeTargetHorizontalDistance(chain75, anchorDepth) * reductionFactor75;
```

**New code (simplified - ChainController handles catenary):**
```cpp
// ChainController now handles catenary physics automatically
targetDistanceInit = chainController->computeTargetHorizontalDistance(targetDropDepth, anchorDepth);
targetDistance30 = chainController->computeTargetHorizontalDistance(chain30, anchorDepth);
targetDistance75 = chainController->computeTargetHorizontalDistance(chain75, anchorDepth);
```

**Remove these lines** (46-57):
```cpp
// DELETE:
float estimatedHorizontalForce = estimateHorizontalForce();
float reductionFactor30 = computeCatenaryReductionFactor(...);
float reductionFactor75 = computeCatenaryReductionFactor(...);
ESP_LOGI(__FILE__, "DeploymentManager: Catenary calcs...");  // Delete this log too
```

---

## Critical Files to Modify

1. **src/ChainController.h** - Add wind listener, catenary methods, constants
2. **src/ChainController.cpp** - Implement enhanced slack calculation with catenary physics
3. **src/DeploymentManager.h** - Remove speed tracking, add slack tracking, remove catenary methods
4. **src/DeploymentManager.cpp** - Remove speed measurement, rewrite pulse logic, remove duplicate physics

---

## Testing Validation

### Test 1: Slack Calculation Accuracy
- Verify ChainController publishes accurate slack values
- Compare with manual catenary calculations
- Check behavior at various depths (3m, 10m, 25m)

### Test 2: Deployment Rate Control
- Verify deployment maintains ~10% slack target
- Test aggressive deployment when slack dropping fast
- Ensure safety brake activates at 50% depth slack limit

### Test 3: Multi-Stage Deployment
- Verify DROP, DEPLOY_30, DEPLOY_75, DEPLOY_100 stages complete correctly
- Check that slack-based control works across stage transitions
- Validate final scope ratio (~5:1)

### Test 4: Edge Cases
- Very shallow water (3m depth)
- Deep water (25m+ depth)
- No wind (low horizontal force)
- Strong wind (high horizontal force)
- Boat drifting toward anchor (slack increasing)

---

## Benefits of This Approach

1. **Simpler Architecture**: Single physics calculation point in ChainController
2. **More Accurate**: All slack calculations use proper catenary physics
3. **Direct Control**: Deploy based on actual measured slack, not predicted boat movement
4. **Adaptive**: Rate-of-change algorithm responds to actual conditions
5. **Maintains Safety**: 50% depth limit still prevents over-deployment
6. **Scales with Depth**: 10% target works from 3m to 25m+ depths
7. **Self-Correcting**: If boat swings or GPS drifts, slack measurement compensates automatically

---

## Implementation Order

1. **Phase 1** - Enhance ChainController (can test slack accuracy independently)
2. **Phase 2** - Remove speed measurement (cleanup, no behavioral change yet)
3. **Phase 3** - Implement slack-based control (core behavior change)
4. **Phase 4** - Delete duplicate code (final cleanup)

This approach allows incremental testing and easy rollback if issues arise.
