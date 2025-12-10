# Domain Context

> SignalK paths, marine physics, and catenary mathematics.

## SignalK Paths

### Published by This Firmware

| Path | Type | Description |
|------|------|-------------|
| `navigation.anchor.rodeDeployed` | float (m) | Current chain length out |
| `navigation.anchor.chainSlack` | float (m) | Calculated horizontal slack |
| `navigation.anchor.chainDirection` | string | "free fall" / "deployed" / "retrieving" |
| `navigation.anchor.autoStage` | string | Current deployment stage |

### Subscribed (Incoming Data)

| Path | Type | Source |
|------|------|--------|
| `environment.depth.belowSurface` | float (m) | Depth sensor |
| `navigation.anchor.distanceFromBow` | float (m) | GPS calculation |
| `environment.wind.speedTrue` | float (m/s) | Wind sensor |
| `environment.tide.heightNow` | float (m) | Tide data |
| `environment.tide.heightHigh` | float (m) | Tide prediction |

### Commands Received

| Path | Values | Action |
|------|--------|--------|
| `navigation.anchor.command` | `"autoDrop"` | Start automated deployment |
| `navigation.anchor.command` | `"autoRetrieve"` | Start automated retrieval |
| `navigation.anchor.command` | `"drop"` | Deploy to depth + 4m |
| `navigation.anchor.command` | `"raise10"` | Raise 10m |
| `navigation.anchor.command` | `"lower10"` | Lower 10m |
| `navigation.anchor.command` | `"STOP"` | Emergency stop |

## Catenary Physics

### The Problem

Chain doesn't hang straight - it forms a catenary curve. We need to calculate how much "slack" (chain lying on the seabed) exists.

### Key Formula

```
slack = chain_length - straight_line_distance_to_anchor
```

But `straight_line_distance` isn't simply `sqrt(horizontal² + vertical²)` because the chain sags.

### Catenary Reduction Factor

The chain sag depends on:
1. **Scope ratio**: chain_length / depth (higher = less sag)
2. **Horizontal force**: wind + current pulling the boat

```cpp
// Simplified catenary reduction
factor = 0.80 to 0.99

// Light forces, deep water → more sag → factor ~0.80
// Strong forces, shallow water → less sag → factor ~0.99
```

### Slack Calculation (Simplified)

```cpp
effective_depth = depth - BOW_HEIGHT;  // Subtract 2m bow height
horizontal_reach = sqrt(chain² - effective_depth²) * catenary_factor;
slack = horizontal_reach - gps_distance;
```

### What Slack Means

| Slack | Interpretation |
|-------|----------------|
| > 0 | Chain has excess length lying on seabed (good) |
| = 0 | Chain is taut from bow to anchor |
| < 0 | Boat is further than chain can reach (anchor dragging!) |

## Scope Ratio

**Scope** = chain_length / water_depth

| Scope | Holding Power | Use Case |
|-------|---------------|----------|
| 3:1 | Poor | Lunch hook, calm conditions |
| 5:1 | Good | Overnight, moderate conditions |
| 7:1 | Excellent | Storm conditions |

The deployment stages target increasing scope:
- DEPLOY_30 → ~3:1 scope
- DEPLOY_75 → ~5:1 scope
- DEPLOY_100 → Full chain (7:1+)

## Tide Compensation

For overnight anchoring, we calculate based on **high tide depth**:

```cpp
tide_adjusted_depth = current_depth + (high_tide - current_tide);
```

This ensures enough chain is deployed for when the tide rises.

## Wind Force Estimation

Wind drag on the boat creates horizontal force on the anchor chain:

```cpp
F_wind = 0.5 × air_density × drag_coefficient × windage_area × wind_speed²
```

Constants used:
- Air density: 1.225 kg/m³
- Drag coefficient: 1.2
- Windage area: 15 m²
- Baseline current force: 30N (always present)

Higher wind = straighter chain = less catenary sag.

## Marine Terminology

| Term | Meaning |
|------|---------|
| **Rode** | The anchor line (chain + rope) |
| **Scope** | Ratio of rode length to depth |
| **Catenary** | The natural curve of a hanging chain |
| **Gypsy** | The windlass wheel that grips chain |
| **Windlass** | The motor/mechanism that raises/lowers anchor |
| **Bower** | The main anchor (vs. stern or kedge) |

## SensESP Framework

### Value Publishing

```cpp
// Create observable value
auto* rodeDeployed = new ObservableValue<float>();

// Connect to SignalK output
rodeDeployed->connect_to(
    new SKOutputFloat("navigation.anchor.rodeDeployed")
);

// Update value (triggers SignalK publish)
rodeDeployed->set(newValue);
```

### Value Listening

```cpp
// Create listener for SignalK path
auto* depthListener = new SKValueListener<float>(
    "environment.depth.belowSurface"
);

// React to updates
depthListener->connect_to(new LambdaConsumer<float>(
    [](float depth) {
        // Handle new depth value
    }
));
```

### Integrator (Accumulator)

Used for chain length tracking from pulse counts:

```cpp
// Accumulates pulse counts × meters_per_pulse
auto* accumulator = new Integrator<float, float>(
    initial_value,
    meters_per_pulse
);

// Get current accumulated value
float chainLength = accumulator->get();

// Reset to specific value
accumulator->set(newValue);
```
