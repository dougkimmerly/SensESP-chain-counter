# SensESP Chain Counter System - Codebase Overview

## Purpose
An ESP32-based **intelligent anchor windlass controller** that automatically deploys and retrieves anchor chain using real-time sensor data, physics-based catenary calculations, and Signal K integration.

---

## Core Components

### 1. ChainController (src/ChainController.cpp)
Motor control & state management

**Responsibilities:**
- Controls windlass relays (up/down)
- Tracks chain position via Hall sensor + integrator
- Enforces safety limits (2-80m range)
- Self-calibrates windlass speed using exponential smoothing
- Calculates horizontal chain slack using catenary physics

**Key Methods:**
- `lowerAnchor(float amount)` - Deploy chain by specified amount
- `raiseAnchor(float amount)` - Retrieve chain by specified amount
- `control(float current_pos)` - Position feedback loop, stops at target
- `calcSpeed()` - Learns windlass speed via exponential weighted moving average
- `calculateAndPublishHorizontalSlack()` - Computes available chain slack

### 2. DeploymentManager (src/DeploymentManager.cpp)
Automated anchor deployment using multi-stage finite state machine

**Responsibilities:**
- 11-stage FSM for intelligent anchor setting
- Physics-based catenary modeling (accounts for chain weight, wind force)
- Pulse-based deployment synchronized with boat drift speed
- Deploys to 5:1 scope ratio with staged setting (40%, 75%, 100%)

**Deployment Stages:**
1. **DROP**: Initial anchor drop to depth + 4m
2. **WAIT_TIGHT**: Wait for chain to pull tight (boat drifts back)
3. **HOLD_DROP**: 2-second stabilization hold
4. **DEPLOY_30**: Deploy to 40% while tracking boat drift
5. **WAIT_30**: Wait for boat to drift to 30% target distance
6. **HOLD_30**: 30-second hold for anchor to set
7. **DEPLOY_75**: Deploy to 80%
8. **WAIT_75**: Wait for boat to drift to 75% target distance
9. **HOLD_75**: 75-second hold for further setting
10. **DEPLOY_100**: Deploy remaining chain to full scope (5:1 ratio)
11. **COMPLETE**: Deployment finished

### 3. Automated Retrieval (src/main.cpp)
Simple automated anchor retrieval using direct ChainController commands

**Responsibilities:**
- Monitors current chain position
- Raises chain directly via ChainController
- Stops when chain reaches 2.0m threshold

**Retrieval Strategy:**
- Calculates amount to raise: current_rode - 2.0m
- Single raise command with no external timeout
- Relies on ChainController's built-in movement timeout and safety mechanisms
- ChainController handles slack-based pause/resume automatically
- Directly calls `chainController->raiseAnchor()` without intermediate FSM

---

## Architecture and Data Flow

### Input Data Sources

**Physical Sensors:**
- Hall effect sensor (GPIO 27) - counts gypsy rotations
- UP button (GPIO 23) - manual raise command
- DOWN button (GPIO 25) - manual lower command
- RESET button (GPIO 26) - reset chain counter to zero

**Signal K Data Streams:**
- `environment.depth.belowSurface` - water depth
- `navigation.anchor.distanceFromBow` - GPS distance to anchor drop point
- `environment.wind.speedApparent` - apparent wind speed
- `navigation.anchor.chainSlack` - computed horizontal slack

**Signal K Commands (PUT requests):**
- `navigation.anchor.command` - windlass control commands
- `navigation.anchor.rodeDeployed` - reset trigger (PUT value=1)

### Processing Pipeline

```
Hall Sensor Pulse → DebounceInt → Counter Handler → Accumulator
                                                        ↓
                                                 Chain Length (m)
                                                        ↓
                    ┌───────────────────────────────────┴────────────────┐
                    ↓                                                     ↓
            ChainController.control()                            SKOutputFloat
            (enforces limits)                                    (publishes to SK)
                    ↓
            Updates relay states
                    ↓
         Physical windlass motor
```

### Slack Calculation

```
Chain Length + Depth + Distance → computeTargetHorizontalDistance()
                                          ↓
                        Horizontal Distance (if chain were taut)
                                          ↓
                          Taut Distance - Current Distance
                                          ↓
                              Horizontal Slack (m)
                                          ↓
                        Published to navigation.anchor.chainSlack
```

### Command Flow

```
Signal K PUT → StringSKPutRequestListener → Command Handler (main.cpp)
                                                    ↓
                        ┌───────────────────────────┼───────────────────────┐
                        ↓                           ↓                       ↓
                ChainController              DeploymentManager      autoRetrieve command
                (direct commands)            (autoDrop FSM)         (direct raiseAnchor)
                        ↓                           ↓                       ↓
                    lowerAnchor()              lowerAnchor()           raiseAnchor()
                    raiseAnchor()              (staged)                (single command)
                    stop()
```

---

## Key Design Patterns

### 1. Reactive Programming with SensESP
- Uses observer pattern extensively via `connect_to()` chains
- Lambda consumers for event handling
- `RepeatSensor` for periodic tasks (publishing, slack calculation)
- Event loop (`event_loop()->onDelay()`, `onRepeat()`) for non-blocking timing

### 2. State Machine Pattern
DeploymentManager uses explicit finite state machine:
- Clear state transitions with logging
- Stage-based progression prevents race conditions
- Single responsibility per state
- Retrieval is simplified to direct ChainController command (no FSM needed)

### 3. Self-Calibrating Speed Learning
ChainController learns windlass speeds over time:
- Exponential smoothing factor (0.2) balances responsiveness vs. noise
- Separate speeds for up/down (they often differ)
- Persisted to Preferences for survival across reboots
- Used for accurate timeout calculations

### 4. Physics-Based Catenary Modeling
DeploymentManager uses real physics for chain behavior:
```cpp
// Catenary reduction factor based on horizontal force
float w = CHAIN_WEIGHT_PER_METER_KG * GRAVITY;  // Chain weight (N/m)
float a = horizontalForce / w;                   // Catenary parameter
float catenarySagReduction = (w * chainLength²) / (8 * horizontalForce);
```
- Accounts for chain weight (2.2 kg/m in water)
- Adjusts for wind/current forces
- More accurate deployment distances than simple geometry

### 5. Pulse-Based Deployment
DeploymentManager deploys chain in intelligent pulses:
- Accumulates demand based on boat drift speed
- Only deploys when accumulated demand exceeds threshold (1m)
- Pauses if slack becomes excessive
- Prevents over-deployment and chain pile-up

### 6. Robust Error Handling
Extensive validation throughout:
```cpp
if (isnan(value) || isinf(value)) {
    ESP_LOGW(__FILE__, "Invalid value detected, using default");
    return safe_default;
}
```
- Guards against NaN/Inf propagation
- Validates sensor data before use
- Safe defaults when sensors unavailable

---

## Safety Features

Multiple layers of protection:
1. **Position limits**: min_length (2m), max_length (80m), stop_before_max (75m)
2. **Timeout protection**: Movement automatically stops after calibrated time + buffer
3. **Negative slack detection**: Stops retrieval if pulling boat toward anchor
4. **Input debouncing**: Prevents switch bounce from causing false counts
5. **Startup delay**: 2-second ignore period to prevent spurious inputs during boot
6. **NaN/Inf validation**: All sensor data validated before use
7. **Persistent state**: Chain position survives power cycles via Preferences

---

## Notable Intelligence

The system goes beyond simple "lower X meters" by:

- **Adapting to conditions**: Wind force affects deployment distances
- **Learning over time**: Windlass speeds auto-calibrate
- **Working with the boat**: Waits for natural drift instead of fighting it
- **Physics-aware**: Understands actual chain behavior (catenary sag)

---

## Class Relationships

```
main.cpp (setup & command handling)
    ├── ChainController (motor control, position tracking)
    │   ├── accumulator (Integrator<float,float>) - tracks chain position
    │   ├── depthListener (SKValueListener) - monitors water depth
    │   ├── distanceListener (SKValueListener) - monitors distance from anchor
    │   └── horizontalSlack (ObservableValue) - computed slack available
    │
    └── DeploymentManager (automated deployment FSM)
        ├── Uses ChainController for motor commands
        ├── windSpeedListener (SKValueListener) - for force calculations
        └── Implements multi-stage deployment logic

Note: Automated retrieval is handled directly in main.cpp via chainController->raiseAnchor()

SensESP Framework Components:
    ├── DigitalInputChange - debounced GPIO inputs (buttons, hall sensor)
    ├── Integrator - accumulates hall sensor pulses to track chain length
    ├── SKOutputFloat/String - publishes to Signal K server
    ├── SKValueListener - subscribes to Signal K data
    └── SKPutRequestListener - receives commands from Signal K
```

---

## Configuration

All critical parameters exposed through SensESP UI:
- GPIO pin assignments
- Debounce times
- Gypsy circumference
- Free fall delays
- Max chain length

---

## Data Flow Example: Automated Deployment

```
User sends: PUT navigation.anchor.command = "autoDrop"
    ↓
Command handler in main.cpp calls deploymentManager->start()
    ↓
DeploymentManager calculates:
    - targetDropDepth = depth + 4m
    - totalChainLength = 5 × (depth + 2m)
    - Catenary-adjusted distances for 30%, 75%, 100%
    ↓
Stage 1 (DROP): ChainController->lowerAnchor(targetDropDepth)
    ↓
Relay activates → Motor lowers chain → Hall sensor pulses
    ↓
Each pulse → Accumulator increments by gypsy_circum (0.25m)
    ↓
When accumulator >= targetDropDepth → ChainController stops
    ↓
Stage 2 (WAIT_TIGHT): Monitor distance until boat drifts back
    ↓
Stage 3 (HOLD_DROP): 2-second stabilization
    ↓
Stage 4 (DEPLOY_30): Pulse-based deployment
    - Measure boat drift speed (EWMA)
    - Calculate demand = speed × time × slack_factor
    - Accumulate demand
    - When demand >= 1m AND slack < limit:
        → ChainController->lowerAnchor(demand)
    - Repeat until chain30 reached
    ↓
... (continues through remaining stages) ...
    ↓
Stage 11 (COMPLETE): Stop all operations
```

---

## Summary

This is a **state-machine-based marine automation system** that uses **sensor fusion** and **physics modeling** to safely automate anchor handling. It combines real-time sensor data, catenary physics, adaptive control algorithms, and Signal K integration to create an intelligent windlass controller.
