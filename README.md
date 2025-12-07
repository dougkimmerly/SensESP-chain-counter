# SensESP Chain Counter

**Marine anchor chain deployment and retrieval control system** for ESP32 with Signal K integration.

## System Overview

This project controls a motorized windlass (chain winch) with automated anchor deployment and retrieval sequences. It uses catenary physics calculations to monitor anchor holding power and applies safety limits to prevent excessive slack or relay wear.

### Key Features

- **Automated Deployment**: Multi-stage anchor drop with continuous monitoring and safety brake
- **Automated Retrieval**: Slack-based chain raising with relay protection and cooldown
- **Physics-Based Slack Calculation**: Accounts for catenary sag, bow height offset (2m), and horizontal forces
- **Safety Limits**: Prevents slack from exceeding 85% of effective water depth
- **Signal K Integration**: Full integration with marine instrument network for depth, wind, and distance data
- **Non-Volatile Memory**: Saves chain position across power cycles
- **Real-Time Monitoring**: Publishes chain length, slack, and deployment status to Signal K

## How It Works

### Core System Architecture

```
Signal K Network (depth, wind, distance)
          ↓
    ChainController (physics & motor control)
          ↓
DeploymentManager / RetrievalManager (automation FSM)
          ↓
    Relay Hardware (motor control)
```

The system has three main components:

1. **ChainController** ([src/ChainController.cpp](src/ChainController.cpp))
   - Motor relay control (lower/raise)
   - Chain position tracking (accumulator-based)
   - Slack calculation with catenary physics
   - Publishes to Signal K

2. **DeploymentManager** ([src/DeploymentManager.cpp](src/DeploymentManager.cpp))
   - Multi-stage deployment FSM
   - Continuous deployment monitoring
   - Safety brake (pauses if slack > 85% of depth)

3. **Automated Retrieval** ([src/main.cpp](src/main.cpp))
   - Simple direct retrieval via ChainController
   - Single raiseAnchor() command with 4x timeout multiplier
   - Slack-based pause/resume handled by ChainController

### Slack Calculation

The system calculates horizontal slack (chain sag on seabed) using catenary physics:

```
Effective depth = water_depth - 2.0m (bow height adjustment)

Horizontal forces = wind drag + current baseline
Reduction factor = catenary sag model (0.80 - 0.99)

Horizontal distance = √(chain² - depth²) × reduction_factor
Slack = horizontal_distance - GPS_distance_to_anchor
```

When GPS distance is unavailable (boat directly over anchor), slack equals the full horizontal reach.

**Example**: 7m chain, 3m water depth
- Effective depth: 3 - 2 = 1m
- Horizontal reach: ~4.17m
- If GPS distance = 0: Slack = 4.17m

### Tide-Adjusted Deployment

The system accounts for tidal changes when calculating chain length. If tide data is available from Signal K, it calculates the depth at high tide to ensure adequate chain:

```
Tide-adjusted depth = current_depth - tide_height_now + tide_height_high
```

**Example**: Anchoring at low tide
- Current depth: 8m
- Current tide height: 1.5m
- High tide height: 3.0m
- Tide-adjusted depth = 8 - 1.5 + 3.0 = 9.5m

This ensures the chain deployed is sufficient for high tide conditions (deepest water).

If tide data is unavailable, the system uses current depth without adjustment.

### Configurable Scope Ratio

The scope ratio (chain length to depth) can be specified with the `autoDrop` command:

| Command | Scope Ratio | Description |
|---------|-------------|-------------|
| `autoDrop` | 5:1 | Default - good for most conditions |
| `autoDrop3` | 3:1 | Minimum safe - calm conditions only |
| `autoDrop4` | 4:1 | Light conditions |
| `autoDrop7` | 7:1 | Heavy weather |
| `autoDrop6.5` | 6.5:1 | Any value between 3.0-10.0 accepted |

Values outside 3.0-10.0 are clamped with a warning log.

### Deployment Sequence

The deployment uses a multi-stage FSM to gradually deploy the anchor:

```
DROP → WAIT_TIGHT → HOLD_DROP
  → DEPLOY_30 → WAIT_30 → HOLD_30
  → DEPLOY_75 → WAIT_75 → HOLD_75
  → DEPLOY_100 → COMPLETE
```

**Key features:**
- **Tide-Adjusted Calculations**: Uses high tide depth for chain length calculation
- **Configurable Scope**: Scope ratio can be set via `autoDrop` command
- **Continuous Monitoring**: Every 500ms, checks if slack exceeds safety limit (85% of depth)
- **Safety Brake**: Automatically pauses deployment if slack becomes excessive, resumes when slack drops
- **Distance-Based Stages**: Waits for GPS distance to reach target before moving to next stage
- **Hold Periods**: Brief stabilization delays between stages

### Retrieval Sequence

Retrieval uses a simple direct command to ChainController:

```
autoRetrieve → Calculate amount → raiseAnchor(amount) → COMPLETE
```

**Key features:**
- **Single Command**: One raiseAnchor() call with calculated amount
- **No External Timeout**: Relies on ChainController's built-in movement timeout and safety mechanisms
- **Automatic Pause/Resume**: ChainController handles slack monitoring and pause/resume during raising
- **Final Pull**: When rode < depth + bow + 3m, skips slack checks for final pull

### Safety Limits

| Parameter | Value | Purpose |
|-----------|-------|---------|
| MAX_SLACK_RATIO | 0.85 | Deployment pauses if slack > 85% of effective depth |
| PAUSE_SLACK_M | 0.2m | Pause raising when slack drops below this |
| RESUME_SLACK_M | 1.0m | Resume raising when slack builds to this |
| SLACK_COOLDOWN_MS | 3s | Cooldown between pause/resume actions |
| FINAL_PULL_THRESHOLD_M | 3.0m | Skip slack checks when rode < depth+bow+3m |
| BOW_HEIGHT_OFFSET | 2.0m | Bow height above waterline for slack calculations |

## Hardware Configuration

- **ESP32 Microcontroller**
- **Relay Pin 12**: Lower chain (to GND to activate)
- **Relay Pin 13**: Raise chain (to GND to activate)
- **Hall Sensor**: Chain position feedback
- **Depth Sensor**: Water depth (via Signal K)
- **GPS/Distance**: Distance to anchor location (via Signal K)
- **Wind Sensor**: Wind speed for catenary calculations (via Signal K)

## Signal K Integration

### Published Values

The system publishes the following values to Signal K:

| Path | Description | Type |
|------|-------------|------|
| `navigation.anchor.rodeDeployed` | Current chain length deployed | Float (meters) |
| `navigation.anchor.chainSlack` | Calculated horizontal slack on seabed | Float (meters) |
| `navigation.anchor.chainDirection` | Chain movement direction | String: "free fall" / "deployed" / "retrieving" |

### Listened Values

The system receives the following values from Signal K:

| Path | Description | Used For |
|------|-------------|----------|
| `environment.depth.belowSurface` | Water depth below surface | Slack calculation, deployment targets |
| `navigation.anchor.distanceFromBow` | GPS distance to anchor | Slack calculation (horizontal reach) |
| `environment.wind.speedTrue` | True wind speed | Catenary force estimation |
| `environment.tide.heightNow` | Current tide height | Tide-adjusted depth calculation |
| `environment.tide.heightHigh` | High tide height | Tide-adjusted depth calculation |

## Build & Setup

### Prerequisites

- **PlatformIO** (installed via VSCode or CLI)
- **ESP32 Development Board**
- **Signal K Server** running on your network

### Development Environment Setup

1. **Install PlatformIO**:
   ```bash
   # Via VSCode extension, or
   pip install platformio
   ```

2. **Clone and open the project**:
   ```bash
   git clone <repository>
   cd SensESP-chain-counter
   ```

3. **Configure your WiFi and Signal K server** in `src/main.cpp`:
   ```cpp
   const char* ssid = "Your-WiFi-SSID";
   const char* password = "Your-WiFi-Password";
   const char* signalk_server = "192.168.1.100:3000";  // Your Signal K server
   ```

### Building and Uploading

```bash
# Full clean build
~/.platformio/penv/bin/platformio run -t clean && ~/.platformio/penv/bin/platformio run

# Quick rebuild (no clean)
~/.platformio/penv/bin/platformio run

# Build and upload to device
~/.platformio/penv/bin/platformio run --target upload

# Monitor serial output (115200 baud)
~/.platformio/penv/bin/platformio device monitor --baud 115200
```

### PlatformIO Configuration

See [platformio.ini](platformio.ini) for:
- Board type (ESP32-WROOM-32)
- Serial port settings
- Build flags and library dependencies
- Monitor settings

## Documentation

For detailed information, see:

- **[claude.md](claude.md)** - Entry point with navigation guide
- **[docs/ARCHITECTURE_MAP.md](docs/ARCHITECTURE_MAP.md)** - Visual system flows and state machines
- **[docs/DEPLOYMENT_REFACTOR_PLAN.md](docs/DEPLOYMENT_REFACTOR_PLAN.md)** - Implementation details
- **[docs/QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** - Code locations and constants

## Troubleshooting

### Slack Reading Shows 0m

**Cause**: GPS distance is unavailable (0 value), or depth data not updating.

**Check**:
- Verify depth sensor is publishing to Signal K (`environment.depth.belowSurface`)
- Verify GPS distance is publishing (`navigation.anchor.distanceFromBow`)
- Watch serial output for data updates

### Deployment Not Pausing at Safety Limit

**Cause**: Slack calculation may not be accounting for wind/current forces correctly.

**Check**:
- Verify wind speed data in Signal K
- Monitor slack value vs. 85% of effective depth threshold
- Review actual catenary reduction factor being used

### Relay Not Activating

**Cause**: Relay pins may not be configured correctly, or relay circuit issue.

**Check**:
- Verify relay pins (12 for lower, 13 for raise) in ChainController
- Check relay circuit continuity with multimeter
- Monitor `isActive()` state in ChainController

## Updates (December 2, 2025)

### Recent Changes

- **Tide-Adjusted Deployment**: Chain calculations now account for tide changes via `environment.tide.heightNow` and `environment.tide.heightHigh` Signal K paths
- **Configurable Scope Ratio**: `autoDrop` command now accepts scope ratio suffix (e.g., `autoDrop7` for 7:1 scope, range 3.0-10.0)
- **Centralized Catenary Physics**: All slack calculations now use consistent physics model
- **Continuous Deployment**: Replaced pulsed deployment (0.5m/1.0m increments) with single large deployment monitored every 500ms
- **Safety Brake**: Deployment automatically pauses and resumes based on slack levels
- **Simplified Retrieval**: Removed RetrievalManager FSM in favor of direct ChainController command with extended timeout
- **Slack-Based Pause/Resume**: ChainController now handles automatic pause/resume during raising based on slack levels
- **Final Pull Logic**: Skips slack checks when chain near vertical (rode < depth+bow+3m)
- **Bow Height Offset**: Slack calculations now correctly account for 2m bow height above waterline
- **Documentation Reorganization**: Added comprehensive architecture and reference documentation

### Known Limitations

- Catenary physics assumes relatively straight chain (scope ratio > 2:1)
- Wind and current estimation is approximate; actual forces vary by boat shape
- GPS distance data must be available for accurate slack calculation (0 distance is handled but less accurate)
- Tide data must be available in Signal K for tide adjustment to work; falls back to current depth if unavailable
