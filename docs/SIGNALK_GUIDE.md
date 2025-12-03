# Signal K Quick Reference for AI Assistants

**Purpose**: Help AI assistants quickly understand Signal K concepts and paths without reading the full specification.

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
