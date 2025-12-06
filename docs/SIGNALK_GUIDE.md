# Signal K Quick Reference for AI Assistants

**Purpose**: Help AI assistants quickly understand Signal K concepts and paths without reading the full specification.

---

## Tools Available

### SignalK MCP Server

You have access to the SignalK MCP server which provides real-time data from the Signal K server. Use these tools to:

- **Check current state**: See live values when debugging
- **Verify plugin output**: Confirm your plugin is publishing correctly
- **Test subscriptions**: See what data is available before subscribing

**Available MCP tools:**
- `mcp__signalk__get_navigation_data` - Get position, heading, speed
- `mcp__signalk__get_signalk_overview` - Get server info and available paths
- `mcp__signalk__get_ais_targets` - Get nearby vessels
- `mcp__signalk__get_system_alarms` - Get active alarms

**When to use:**
- When debugging: Check if values are being published correctly
- Before subscribing: See what paths/data are available
- During development: Verify your plugin's output in real-time

### Research Command

When you need to research Signal K plugin development topics that aren't covered in this guide, use the `/signalk-research` command to spawn a background research agent:

```
/signalk-research how do I register a PUT handler for custom commands?
```

This spawns an independent agent that will:
- Search local docs and source code
- Search project plugin code for patterns
- Fetch online documentation if needed
- Return detailed findings without consuming your session context

Use this for any non-trivial Signal K questions before attempting implementation.

---

## What is Signal K?

Signal K is an open marine data standard that provides:
- A universal data format for marine instruments
- JSON-based data model with hierarchical paths
- WebSocket and REST APIs for real-time data
- Standardized units (SI) across all measurements

---

## Key Documentation Sources

| Resource | URL | Use For |
|----------|-----|---------|
| **Specification** | https://signalk.org/specification/latest/ | Full specification |
| **Path Reference** | https://signalk.org/specification/1.5.0/doc/vesselsBranch.html | All vessel paths |
| **Data Model** | https://signalk.org/specification/1.7.0/doc/data_model.html | Full vs delta format |
| **GitHub Schemas** | https://github.com/SignalK/specification/tree/master/schemas/groups | JSON schemas |
| **Server Docs** | https://demo.signalk.org/documentation/ | Server implementation |

---

## Core Concepts

### 1. Data Models

**Delta Model** (most common):
```json
{
  "context": "vessels.self",
  "updates": [{
    "source": {"label": "sensor1"},
    "timestamp": "2025-12-02T10:30:00Z",
    "values": [
      {"path": "navigation.speedOverGround", "value": 3.5}
    ]
  }]
}
```

**Full Model** (complete state snapshot):
```json
{
  "version": "1.7.0",
  "self": "vessels.urn:mrn:imo:mmsi:123456789",
  "vessels": {
    "urn:mrn:imo:mmsi:123456789": {
      "navigation": {
        "speedOverGround": {
          "value": 3.5,
          "timestamp": "2025-12-02T10:30:00Z",
          "$source": "sensor1"
        }
      }
    }
  }
}
```

### 2. Path Structure

Paths use dot-notation hierarchy:
```
category.subcategory.property
```

Examples:
- `navigation.speedOverGround`
- `environment.depth.belowSurface`
- `electrical.batteries.house.voltage`
- `propulsion.port.revolutions`

### 3. Units (Always SI)

| Measurement | Unit | Notes |
|-------------|------|-------|
| Speed | m/s | NOT knots |
| Distance | m | meters |
| Temperature | K | Kelvin (add 273.15 to Celsius) |
| Pressure | Pa | Pascals |
| Angles | rad | radians (NOT degrees) |
| Voltage | V | Volts |
| Current | A | Amperes |
| Frequency/RPM | Hz | multiply by 60 for RPM |
| Volume | m³ | cubic meters |
| Ratios | 0-1 | percentage as decimal |

---

## Common Signal K Paths

### Navigation

| Path | Description | Unit |
|------|-------------|------|
| `navigation.position.latitude` | Latitude | degrees |
| `navigation.position.longitude` | Longitude | degrees |
| `navigation.speedOverGround` | Speed over ground | m/s |
| `navigation.courseOverGroundTrue` | Course (true) | rad |
| `navigation.headingTrue` | Heading (true) | rad |
| `navigation.headingMagnetic` | Heading (magnetic) | rad |
| `navigation.rateOfTurn` | Turn rate | rad/s |
| `navigation.anchor.position` | Anchor position | object |
| `navigation.anchor.maxRadius` | Anchor alarm radius | m |
| `navigation.anchor.currentRadius` | Distance from anchor | m |
| `navigation.anchor.rodeDeployed` | Chain/rode out | m |

### Environment

| Path | Description | Unit |
|------|-------------|------|
| `environment.depth.belowSurface` | Water depth | m |
| `environment.depth.belowTransducer` | Depth at transducer | m |
| `environment.wind.speedTrue` | True wind speed | m/s |
| `environment.wind.speedApparent` | Apparent wind speed | m/s |
| `environment.wind.directionTrue` | True wind direction | rad |
| `environment.wind.directionApparent` | Apparent wind direction | rad |
| `environment.outside.temperature` | Air temperature | K |
| `environment.outside.pressure` | Barometric pressure | Pa |
| `environment.outside.humidity` | Humidity | ratio 0-1 |
| `environment.water.temperature` | Water temperature | K |
| `environment.tide.heightNow` | Current tide height | m |
| `environment.tide.heightHigh` | High tide height | m |
| `environment.tide.heightLow` | Low tide height | m |
| `environment.tide.timeLow` | Next low tide time | ISO 8601 |
| `environment.tide.timeHigh` | Next high tide time | ISO 8601 |

### Electrical

| Path | Description | Unit |
|------|-------------|------|
| `electrical.batteries.{id}.voltage` | Battery voltage | V |
| `electrical.batteries.{id}.current` | Battery current | A |
| `electrical.batteries.{id}.stateOfCharge` | Charge level | ratio 0-1 |
| `electrical.batteries.{id}.temperature` | Battery temp | K |
| `electrical.alternators.{id}.voltage` | Alternator voltage | V |
| `electrical.alternators.{id}.current` | Alternator current | A |
| `electrical.alternators.{id}.revolutions` | Alternator speed | Hz |
| `electrical.solar.{id}.voltage` | Solar voltage | V |
| `electrical.solar.{id}.current` | Solar current | A |

### Propulsion

| Path | Description | Unit |
|------|-------------|------|
| `propulsion.{id}.revolutions` | Engine RPM | Hz (×60 for RPM) |
| `propulsion.{id}.state` | Engine state | stopped/started |
| `propulsion.{id}.temperature` | Engine temp | K |
| `propulsion.{id}.oilPressure` | Oil pressure | Pa |
| `propulsion.{id}.oilTemperature` | Oil temp | K |
| `propulsion.{id}.coolantTemperature` | Coolant temp | K |
| `propulsion.{id}.fuelRate` | Fuel consumption | m³/s |
| `propulsion.{id}.transmission.gear` | Gear | Forward/Neutral/Reverse |

### Tanks

| Path | Description | Unit |
|------|-------------|------|
| `tanks.freshWater.{id}.currentLevel` | Water level | ratio 0-1 |
| `tanks.freshWater.{id}.capacity` | Tank capacity | m³ |
| `tanks.fuel.{id}.currentLevel` | Fuel level | ratio 0-1 |
| `tanks.fuel.{id}.capacity` | Tank capacity | m³ |
| `tanks.blackWater.{id}.currentLevel` | Waste level | ratio 0-1 |

---

## WebSocket API

### Connection

```
ws://hostname:3000/signalk/v1/stream?subscribe=self
```

Query parameters:
- `subscribe=self` - vessel data only (default)
- `subscribe=all` - all vessels
- `subscribe=none` - no auto-subscription

### Subscribe Message

```json
{
  "context": "vessels.self",
  "subscribe": [{
    "path": "navigation.speedOverGround",
    "period": 1000,
    "policy": "ideal"
  }]
}
```

**Parameters:**
- `path`: Path to subscribe (supports `*` wildcard)
- `period`: Update interval in ms (default 1000)
- `policy`: `instant`, `ideal`, or `fixed`
- `minPeriod`: Minimum update interval

### Unsubscribe

```json
{
  "context": "*",
  "unsubscribe": [{"path": "*"}]
}
```

---

## REST API

### Discovery

```
GET /signalk
```
Returns available endpoints and versions.

### Get Full Data

```
GET /signalk/v1/api/
```
Returns complete Signal K document.

### Get Specific Path

```
GET /signalk/v1/api/vessels/self/navigation/speedOverGround
```

### PUT Request (Control)

```
PUT /signalk/v1/api/vessels/self/electrical/switches/anchorLight/state
Content-Type: application/json

{"value": 1}
```

---

## Plugin PUT Handlers (Command Endpoints)

Signal K plugins can register PUT handlers to receive commands via standard Signal K paths. This pattern is commonly used with the SKipper app to control vessel systems.

### How It Works

1. Plugin registers a handler for a Signal K path using `app.registerPutHandler()`
2. Client (e.g., SKipper app) sends a PUT request to that path with a command value
3. Plugin receives the request and can execute any logic (start/stop motor, deploy anchor, etc.)
4. Plugin responds with status and message

### Example: Anchor Control Handler

```javascript
// Register a PUT handler for anchor commands
app.registerPutHandler('navigation.anchor.setAnchor', (context, path, value, callback) => {
  // Validate required fields
  if (!value || !value.latitude || !value.longitude || value.rodeDeployed === undefined) {
    return callback({
      state: 'COMPLETED',
      statusCode: 400,
      message: 'Missing required fields: latitude, longitude, rodeDeployed'
    });
  }

  try {
    // Store the anchor position and rope deployed length
    anchorPosition = {
      latitude: value.latitude,
      longitude: value.longitude,
      altitude: value.altitude || 0
    };
    rodeDeployed = value.rodeDeployed;

    // Perform any necessary logic (validation, API calls, etc.)
    validateAnchorPosition(anchorPosition);

    return callback({
      state: 'COMPLETED',
      statusCode: 200,
      message: `Anchor set at (${value.latitude.toFixed(4)}, ${value.longitude.toFixed(4)}), rode ${value.rodeDeployed}m`
    });
  } catch (error) {
    return callback({
      state: 'COMPLETED',
      statusCode: 500,
      message: `Error: ${error.message}`
    });
  }
});
```

### Calling the Handler via REST API

```bash
curl -X PUT http://localhost:80/signalk/v1/api/vessels/self/navigation/anchor/setAnchor \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <token>" \
  -d '{
    "value": {
      "latitude": 43.6539,
      "longitude": 7.0234,
      "rodeDeployed": 45.5
    }
  }'
```

Response:
```json
{
  "state": "COMPLETED",
  "statusCode": 200,
  "message": "Anchor set at (43.6539, 7.0234), rode 45.5m"
}
```

### SKipper App Integration

SKipper displays Signal K paths and can send PUT commands to any registered PUT handler. To make a handler available in SKipper:

1. **Register the PUT handler** in your plugin (as shown above)
2. **Use a standard or custom Signal K path** (e.g., `navigation.anchor.setAnchor`)
3. **SKipper will automatically discover** the handler via the `/signalk/v1/api` endpoint
4. **Users can trigger the command** from SKipper's UI

### Best Practices

#### 1. Use Standard Paths When Possible

Prefer standard Signal K paths (`navigation.anchor.distanceFromBow`, `electrical.switches.anchorLight.state`) over custom paths to maintain compatibility.

#### 2. Make Handlers Idempotent

If the same command is sent twice, it should be safe:

```javascript
// ✓ Good: Setting anchor position twice produces same result
setAnchor(pos) → stored in database

// ✗ Bad: Incrementing counter twice produces different result
incrementCounter() → 1 becomes 2 becomes 3
```

#### 3. Validate Input

Always validate required fields and types:

```javascript
if (!value.latitude || isNaN(value.latitude)) {
  return callback({
    state: 'COMPLETED',
    statusCode: 400,
    message: 'Invalid latitude'
  });
}
```

#### 4. Return Appropriate Status Codes

- `200` - Success
- `400` - Invalid input
- `500` - Server error
- `503` - Service unavailable

#### 5. Keep Responses Lightweight

Use concise messages that fit on mobile displays:

```javascript
// ✓ Good
message: "Anchor set, rode 45m"

// ✗ Bad
message: "The anchor position has been successfully configured with a rope length of 45 meters deployed on the seafloor"
```

### Common Patterns

#### Motor Control
```javascript
app.registerPutHandler('electrical.motors.main.control', (context, path, value, callback) => {
  if (value === 'start') {
    startMotor();
  } else if (value === 'stop') {
    stopMotor();
  }
  callback({ state: 'COMPLETED', statusCode: 200, message: `Motor ${value}` });
});
```

#### Light Switch
```javascript
app.registerPutHandler('electrical.switches.anchorLight.state', (context, path, value, callback) => {
  setLightState(value === 1);
  callback({ state: 'COMPLETED', statusCode: 200, message: value === 1 ? 'On' : 'Off' });
});
```

#### Configuration Setting
```javascript
app.registerPutHandler('plugins.myapp.config.threshold', (context, path, value, callback) => {
  if (value < 0 || value > 100) {
    return callback({ state: 'COMPLETED', statusCode: 400, message: 'Must be 0-100' });
  }
  config.threshold = value;
  callback({ state: 'COMPLETED', statusCode: 200, message: `Threshold set to ${value}` });
});
```

---

## Metadata

Every path can have metadata:

```json
{
  "meta": {
    "displayName": "Speed Over Ground",
    "units": "m/s",
    "description": "Vessel speed relative to ground",
    "zones": [
      {"upper": 5, "state": "normal"},
      {"lower": 5, "upper": 10, "state": "alert"},
      {"lower": 10, "state": "alarm"}
    ]
  }
}
```

**Zone States:** `nominal`, `normal`, `alert`, `warn`, `alarm`, `emergency`

---

## Source Tracking

Every value includes source information:

```json
{
  "path": "navigation.speedOverGround",
  "value": 3.5,
  "timestamp": "2025-12-02T10:30:00Z",
  "$source": "sources.nmea0183.GP"
}
```

Sources are organized by:
1. Physical connection (USB, network)
2. Protocol (NMEA0183, NMEA2000, SignalK)
3. Device identifier

---

## Paths Used in This Project

### Published (Output)

| Path | Description |
|------|-------------|
| `navigation.anchor.rodeDeployed` | Chain length deployed |
| `navigation.anchor.chainSlack` | Calculated horizontal slack |
| `navigation.anchor.chainDirection` | free fall / deployed / retrieving |
| `navigation.anchor.autoStage` | Current autoDrop stage |

### Subscribed (Input)

| Path | Description |
|------|-------------|
| `environment.depth.belowSurface` | Water depth |
| `navigation.anchor.distanceFromBow` | GPS distance to anchor |
| `environment.wind.speedTrue` | Wind speed |
| `environment.tide.heightNow` | Current tide height |
| `environment.tide.heightHigh` | High tide height |

---

## Common Patterns

### Custom Paths

For non-standard data, use vessel-specific paths:
```
vessels.self.myapp.customValue
```

### Multiple Instances

Use identifiers for multiple devices:
```
electrical.batteries.house.voltage
electrical.batteries.starter.voltage
propulsion.port.revolutions
propulsion.starboard.revolutions
```

### Notifications/Alarms

```
notifications.{category}.{item}
```

States: `normal`, `alert`, `warn`, `alarm`, `emergency`

---

## Unit Conversions

Common conversions you'll need:

```
Knots to m/s:     knots × 0.514444
m/s to Knots:     m/s × 1.94384

Celsius to K:     °C + 273.15
Kelvin to °C:     K - 273.15

Degrees to rad:   degrees × (π / 180)
rad to Degrees:   rad × (180 / π)

PSI to Pa:        PSI × 6894.76
Bar to Pa:        bar × 100000

Hz to RPM:        Hz × 60
RPM to Hz:        RPM / 60
```

---

## Authentication

Signal K supports token-based auth:

```
Authorization: Bearer <token>
```

Or via cookies on WebSocket connections.

---

## Quick Reference Links

- **Full specification**: https://signalk.org/specification/latest/
- **Path schemas**: https://github.com/SignalK/specification/tree/master/schemas/groups
- **Server API**: https://demo.signalk.org/documentation/
- **Plugin development**: https://demo.signalk.org/documentation/Developing/Plugins.html

---

Last Updated: 2025-12-02
