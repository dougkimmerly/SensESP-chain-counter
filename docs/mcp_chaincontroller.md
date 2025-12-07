# ChainController MCP Documentation

This document describes the ChainController class and its interface for use by other Claude instances and AI systems. It serves as a Model Context Protocol (MCP) specification.

## Overview

**ChainController** is the core component that manages anchor deployment and retrieval on a marine vessel. It controls a motorized windlass (anchor winch) with relays, calculates chain position, and maintains horizontal slack (the amount of chain on the seafloor due to catenary sag).

**Location:** `src/ChainController.h` and `src/ChainController.cpp`

**Key Responsibility:** Control physical relay hardware and track chain position using catenary physics to account for chain weight and wind forces.

---

## Public Interface

### Constructor
```cpp
ChainController(
    float min_length,           // Minimum chain length (usually 0m)
    float max_length,           // Maximum chain length (fully deployed)
    float stop_before_max,      // Safety limit (stop before reaching max)
    sensesp::Integrator<float, float>* acc,  // Chain length accumulator
    int downRelayPin,           // GPIO pin for "lower" relay
    int upRelayPin              // GPIO pin for "raise" relay
);
```

### Anchor Movement Commands

#### `void lowerAnchor(float amount)`
- **Purpose:** Deploy chain (lower anchor)
- **Parameter:** `amount` - How many meters to deploy
- **Behavior:**
  - Sets target position: `current_chain + amount`
  - Activates down relay
  - Monitors position via accumulator
  - Stops when target reached or timeout occurs
- **Limits Applied:**
  - Clamps to `max_length_`
  - Clamps to `stop_before_max_` (safety)
  - Timeout: `(amount / downSpeed_) * 1000 + 200ms` buffer

#### `void raiseAnchor(float amount)`
- **Purpose:** Retrieve chain (raise anchor)
- **Parameter:** `amount` - How many meters to retrieve
- **Behavior:**
  - Sets target position: `current_chain - amount`
  - Activates up relay
  - Monitors position via accumulator
  - Stops when target reached or timeout occurs
- **Limits Applied:**
  - Clamps to `min_length_`
  - Timeout: `(amount / upSpeed_) * 1000 + 200ms` buffer

#### `void stop()`
- **Purpose:** Immediately stop all chain movement
- **Behavior:**
  - Deactivates both relays
  - Sets state to IDLE
  - No target position validation

### Movement Control

#### `bool isActive() const`
- **Returns:** `true` if either relay is currently active (LOWERING or RAISING state)
- **Use Case:** Check if windlass is moving before issuing new commands

#### `bool isActivelyControlling() const`
- **Returns:** `true` if state is LOWERING or RAISING
- **Difference from `isActive()`:** Explicitly checks state machine, not relay status
- **Use Case:** Critical for preventing command race conditions

#### `void control(float current_pos)`
- **Purpose:** Main control loop (called every ~100ms from main.cpp)
- **Parameter:** `current_pos` - Current chain length from accumulator
- **Behavior:**
  - If state is IDLE: do nothing
  - If LOWERING: check if `current_pos >= target_`
  - If RAISING: check if `current_pos <= target_`
  - If target reached or timeout: deactivate relay, set to IDLE
  - Updates speed calculations if movement completed

#### `unsigned long getTimeout() const`
- **Returns:** Remaining time (ms) before movement times out
- **Use Case:** Detect stuck relays or timing issues

### Chain Position and Depth Information

#### `float getChainLength() const`
- **Returns:** Current chain deployed (meters)
- **Source:** From accumulator (integrated from flow sensor)
- **Range:** Typically 0m to 50m depending on vessel

#### `float getCurrentDepth() const`
- **Returns:** Current water depth below surface (meters)
- **Source:** Signal K listener on `environment.depth.belowSurface`
- **Reliability:** Updated every 2 seconds; returns last known value

#### `float getCurrentDistance() const`
- **Returns:** Current horizontal distance from boat bow to anchor (meters)
- **Source:** Signal K listener on `navigation.anchor.distanceFromBow`
- **Reliability:** Updated every 2 seconds; returns last known value

#### `float getDownSpeed() const`
- **Returns:** Lowering speed in milliseconds per meter
- **Use Case:** Calculate timeout for deployment commands
- **Example:** `getDownSpeed()` = 1000ms/m means 1 meter takes 1 second

### Signal K Integration

#### `sensesp::SKValueListener<float>* getDepthListener() const`
- **Returns:** Depth listener object
- **Use Case:** Other components can subscribe to depth changes

#### `sensesp::SKValueListener<float>* getDistanceListener() const`
- **Returns:** Distance listener object
- **Use Case:** Other components can subscribe to distance changes

#### `sensesp::ObservableValue<float>* getHorizontalSlackObservable() const`
- **Returns:** Observable horizontal slack value (meters)
- **Use Case:** Connect to Signal K output for monitoring/control
- **Value:** Amount of chain on seafloor due to catenary sag

### Slack Calculation

#### `void calculateAndPublishHorizontalSlack()`
- **Purpose:** Recalculate and publish slack to Signal K
- **Called By:** main.cpp every 500ms
- **Formula:**
  ```
  slack = sqrt(chain² - depth²) × catenary_reduction - current_distance
  ```
- **Catenary Physics:**
  - Accounts for chain weight (2.2 kg/m in water)
  - Accounts for horizontal forces (wind drag + current)
  - Returns slack as meters of chain on seafloor
- **Published Path:** `navigation.anchor.chainSlack` in Signal K

#### `float computeTargetHorizontalDistance(float chainLength, float depth)`
- **Purpose:** Calculate expected horizontal distance given chain length and depth
- **Parameters:**
  - `chainLength` - Total chain deployed (meters)
  - `depth` - Water depth (meters)
- **Returns:** Horizontal distance accounting for catenary sag (meters)
- **Physics:**
  1. Straight-line distance: `sqrt(chain² - depth²)`
  2. Apply catenary reduction: `straightDist × reduction_factor`
  3. Reduction factor accounts for chain weight and wind force
- **Used By:** DeploymentManager to set distance targets

### Listener Access

#### `void setDepthBelowSurface(float depth)`
- **Purpose:** Manually set depth (called by Signal K listener)
- **Use Case:** Internal; called automatically

#### `void setDistanceBowToAnchor(float distance)`
- **Purpose:** Manually set distance (called by Signal K listener)
- **Use Case:** Internal; called automatically

### Speed Calibration

#### `void calcSpeed(unsigned long start_time, float start_position)`
- **Purpose:** Calculate and update up/down speeds after movement completes
- **Parameters:**
  - `start_time` - When movement started (ms)
  - `start_position` - Chain length at start (meters)
- **Behavior:**
  - Compares expected vs actual time taken
  - Updates `upSpeed_` and/or `downSpeed_` with EWMA smoothing
  - Persists to NVS (non-volatile storage)
- **Smoothing Factor:** 0.2 (20% new, 80% historical)

#### `void loadSpeedsFromPrefs()`
- **Purpose:** Load saved up/down speeds from NVS on startup
- **Called By:** main.cpp initialization

#### `void saveSpeedsToPrefs()`
- **Purpose:** Save current speeds to NVS
- **Called By:** After speed calculation completes

---

## State Machine

```
IDLE
 ├─ lowerAnchor() → LOWERING
 └─ raiseAnchor() → RAISING

LOWERING
 ├─ Target reached → IDLE
 ├─ Timeout → IDLE
 └─ stop() → IDLE

RAISING
 ├─ Target reached → IDLE
 ├─ Timeout → IDLE
 └─ stop() → IDLE
```

**State Definition:**
```cpp
enum class ChainState {
    IDLE,      // No relay active
    LOWERING,  // Down relay on
    RAISING    // Up relay on
};
```

---

## Catenary Physics Model

### Overview
The ChainController accounts for **catenary sag** - the sag in the chain caused by its own weight and horizontal forces from wind/current.

### Constants (Physical Properties)
```cpp
CHAIN_WEIGHT_PER_METER_KG = 2.2      // kg/m (in water, adjusted for buoyancy)
GRAVITY = 9.81                        // m/s²
BOAT_WINDAGE_AREA_M2 = 15.0          // m² effective area
AIR_DENSITY = 1.225                   // kg/m³
DRAG_COEFFICIENT = 1.2                // typical for boat hull + rigging
```

### Horizontal Force Estimation
```cpp
float estimateHorizontalForce()
```
- Estimates total horizontal force from:
  1. **Wind:** `0.5 × air_density × wind_speed² × boat_area × drag_coeff`
  2. **Current:** Assumed ~50N baseline + 10N per m/s current speed
- Returns total force in Newtons

### Catenary Reduction Factor
```cpp
float computeCatenaryReductionFactor(
    float chainLength,      // meters
    float anchorDepth,      // meters
    float horizontalForce   // Newtons
)
```
- **Returns:** Reduction factor (typically 0.7 to 0.95)
- **Lower factor** = more sag (lighter forces, deeper water)
- **Higher factor** = less sag (strong winds, shallow water)
- **Formula:** Uses iterative solution for catenary equation
- **Output:** Multiplies straight-line distance by this factor

### Example Calculation
```
Scenario: 20m chain, 10m depth, 100N wind force
Straight-line distance: sqrt(20² - 10²) = √300 = 17.3m
Reduction factor: 0.85 (catenary sag)
Horizontal slack: 17.3 × 0.85 = 14.7m
```

---

## Common Usage Patterns

### Pattern 1: Simple Anchor Drop
```cpp
// From DeploymentManager::start()
chainController->lowerAnchor(50.0);  // Deploy 50 meters
// Control loop monitors until target reached
```

### Pattern 2: Continuous Deployment with Monitoring
```cpp
// From DeploymentManager::startContinuousDeployment()
float amount = targetLength - chainController->getChainLength();
chainController->lowerAnchor(amount);

// Every 500ms from monitorDeployment():
float current_slack = chainController->getHorizontalSlackObservable()->get();
float depth = chainController->getCurrentDepth();
if (current_slack > depth * 0.5) {
    chainController->stop();  // Safety brake
}
```

### Pattern 3: Automated Retrieval
```cpp
// From main.cpp autoRetrieve command handler
float currentRode = chainController->getChainLength();
float amountToRaise = currentRode - 2.0;  // Raise to 2.0m
if (amountToRaise > 0.1) {
    chainController->raiseAnchor(amountToRaise);
    // No external timeout - ChainController has built-in movement timeout
    // and slack-based pause/resume logic
}
```

### Pattern 4: Speed Calibration
```cpp
// From main.cpp control loop after movement completes
chainController->calcSpeed(
    deployment_start_time,
    deployment_start_position
);
chainController->saveSpeedsToPrefs();
```

---

## Integration Points

### Main.cpp
- Calls `control(current_pos)` every ~100ms with accumulator value
- Calls `calculateAndPublishHorizontalSlack()` every 500ms
- Calls `calcSpeed()` after movements complete

### DeploymentManager
- Calls `lowerAnchor(amount)` for initial drop
- Calls `lowerAnchor(amount)` for 30%, 75%, 100% stages
- Calls `stop()` on safety conditions
- Calls `isActive()` to check relay status
- Calls `getChainLength()` to track progress
- Calls `getHorizontalSlackObservable()` to check slack
- Calls `getCurrentDepth()` for depth value

### autoRetrieve (main.cpp)
- Calls `raiseAnchor(amount)` with calculated amount (rode - 2.0m)
- No external timeout - relies on ChainController's built-in movement timeout
- ChainController handles automatic slack-based pause/resume during raising
- User can manually call `stop()` if needed

### Signal K Integration (main.cpp)
- Publishes chain length to `navigation.anchor.rodeDeployed`
- Publishes depth from listener to `environment.depth.belowSurface`
- Publishes distance from listener to `navigation.anchor.distanceFromBow`
- Publishes slack to `navigation.anchor.chainSlack`
- Publishes chain direction to `navigation.anchor.chainDirection`

---

## Timeout Behavior

### Timeout Calculation
```cpp
void updateTimeout(float amount, float speed_ms_per_m) {
    unsigned long expected_ms = (unsigned long)(amount * speed_ms_per_m) + 200;
    move_timeout_ = millis() + expected_ms;
}
```

**Formula:** `timeout = now + (amount_meters × speed_ms_per_meter) + 200ms buffer`

### Timeout Actions
1. If timeout reaches: `control()` detects expired time
2. Relay deactivates (moved to IDLE)
3. Speed recalculation triggered
4. Warning logged if time significantly different from expected

### Why 200ms Buffer?
- Accounts for relay switching time
- Prevents immediate timeout on marginal moves
- Safety margin for motor ramp-up/ramp-down

---

## Safety Features

### Limits
1. **Maximum length:** Cannot exceed `max_length_`
2. **Stop-before-max:** Cannot exceed `stop_before_max_` (extra safety margin)
3. **Minimum length:** Cannot go below `min_length_`

### Timeout Protection
- Every movement has a timeout
- If relay doesn't complete in expected time, automatically stops
- Allows detection of stuck motors or jammed chain

### Safety Brake (DeploymentManager)
- Monitors slack during deployment
- If slack > 50% of depth: automatically stops deployment
- Can resume when slack drops

---

## Error Handling

### Guard Against Invalid Inputs
- `computeTargetHorizontalDistance()` checks for NaN/Inf
- `getCurrentDepth()` guards against invalid values
- `getCurrentDistance()` guards against invalid values
- Returns sensible defaults (0.0) on error

### Logging Levels
- **ERROR (ESP_LOGE):** Invalid inputs, math errors
- **WARNING (ESP_LOGW):** Limits exceeded, timeouts, suspicious values
- **INFO (ESP_LOGI):** Normal operations, commands issued, completions
- **DEBUG (ESP_LOGD):** Frequent status updates (usually suppressed)

---

## Performance Notes

### Calculation Frequency
- **Position updates:** Every ~100ms (control loop)
- **Speed calculation:** After each movement completes
- **Slack calculation:** Every 500ms
- **Speed persistence:** On each calibration completion

### Memory Usage
- **Static storage:** ~500 bytes (listeners, constants)
- **Runtime:** Minimal heap usage, all stack-based calculations

### Timing Precision
- Relay activation: Immediate (GPIO write)
- Position monitoring: ±100ms (control loop interval)
- Timeout: ±200ms (buffer allowance)

---

## How Other Systems Should Interact

### DeploymentManager
✅ **DO:**
- Call `getChainLength()` frequently for progress
- Call `getHorizontalSlackObservable()->get()` to check slack
- Call `isActive()` before issuing new movement commands
- Call `stop()` when safety conditions detected

❌ **DON'T:**
- Directly set state machine
- Directly manipulate relay pins
- Assume movements complete instantly

### autoRetrieve Command (main.cpp)
✅ **DO:**
- Call `raiseAnchor()` with calculated amount (rode - 2.0m)
- Rely on ChainController's built-in movement timeout
- Trust ChainController for slack-based pause/resume
- Allow user to manually `stop()` if needed

❌ **DON'T:**
- Issue multiple raiseAnchor() calls while one is active
- Add external timeout (ChainController has built-in protection)
- Try to manually manage slack pause/resume (ChainController handles this)

### Test Code
✅ **DO:**
- Use `control()` loop to simulate time passage
- Monitor state transitions via logging
- Verify timeout protection works
- Test with various depth/wind scenarios

❌ **DON'T:**
- Mock the accumulator without time simulation
- Skip timeout calculations
- Assume straight-line distances without catenary

---

## Firmware Build and Testing

### To Test ChainController Alone
```bash
# Build firmware
~/.platformio/penv/bin/platformio run

# Run with serial monitor to see logs
~/.platformio/penv/bin/platformio device monitor --baud 115200
```

### Expected Log Output
```
I (125) ChainController: ChainController initialized. UpRelay: 13, DownRelay: 12.
I (150) ChainController: lowerAnchor() called, start_time=150, start_pos=0.00
D (150) ChainController: Down relay activated
I (3200) ChainController: Movement complete: target reached
I (3200) ChainController: Speed calculation: expected 3050ms, actual 3050ms, new speed 1000ms/m
```

---

## Diagram: Control Flow

```
main.cpp
  ↓
  Loop every 100ms
  ├─ accumulator->get() → current_pos
  ├─ chainController->control(current_pos)
  │  ├─ Check if LOWERING/RAISING/IDLE
  │  ├─ Compare to target
  │  ├─ Check timeout
  │  └─ Update relays if needed
  │
  └─ Every 500ms
     └─ chainController->calculateAndPublishHorizontalSlack()
        ├─ Get current depth from listener
        ├─ Get current distance from listener
        ├─ Calculate catenary-adjusted distance
        ├─ Publish slack to Signal K
        └─ Notify observers

DeploymentManager (when running)
  ├─ Call lowerAnchor(amount)
  │  → ChainController sets state to LOWERING, activates relay
  │
  └─ Every cycle
     ├─ Check getChainLength() vs target
     ├─ Check getHorizontalSlackObservable() vs limits
     └─ Call stop() if needed
```

---

## Summary

**ChainController is a low-level motor control and position tracking system.**

- **Input:** Signal K depth/distance, relay pins, accumulator position
- **Processing:** State machine, timeout monitoring, catenary physics
- **Output:** Relay activation, slack calculation, speed statistics

**Higher-level systems (DeploymentManager and autoRetrieve command in main.cpp) orchestrate the strategy of what movements to make and when. ChainController executes the movement safely, tracks position accurately, and handles slack-based pause/resume automatically.**

