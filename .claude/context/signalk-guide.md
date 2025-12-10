# Signal K Quick Reference

> API reference for Signal K plugin development

---

## Core Concepts

### Data Models

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
          "timestamp": "2025-12-02T10:30:00Z"
        }
      }
    }
  }
}
```

### Path Structure
Paths use dot-notation: `category.subcategory.property`
- `navigation.speedOverGround`
- `environment.depth.belowSurface`
- `electrical.batteries.house.voltage`

---

## Units (Always SI)

| Measurement | Unit | Notes |
|-------------|------|-------|
| Speed | m/s | NOT knots |
| Distance | m | meters |
| Temperature | K | Kelvin (add 273.15 to °C) |
| Pressure | Pa | Pascals |
| Angles | rad | radians (NOT degrees) |
| Voltage | V | Volts |
| Current | A | Amperes |
| Frequency/RPM | Hz | multiply by 60 for RPM |
| Volume | m³ | cubic meters |
| Ratios | 0-1 | percentage as decimal |

### Unit Conversions
```
Knots ↔ m/s:     knots × 0.514444 | m/s × 1.94384
°C ↔ K:          °C + 273.15 | K - 273.15
Degrees ↔ rad:   deg × (π/180) | rad × (180/π)
PSI → Pa:        PSI × 6894.76
Bar → Pa:        bar × 100000
Hz ↔ RPM:        Hz × 60 | RPM / 60
```

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
| `navigation.anchor.position` | Anchor position | object |
| `navigation.anchor.maxRadius` | Anchor alarm radius | m |
| `navigation.anchor.currentRadius` | Distance from anchor | m |
| `navigation.anchor.rodeDeployed` | Chain/rode out | m |

### Environment
| Path | Description | Unit |
|------|-------------|------|
| `environment.depth.belowSurface` | Water depth | m |
| `environment.wind.speedTrue` | True wind speed | m/s |
| `environment.wind.angleApparent` | Apparent wind angle | rad |
| `environment.outside.temperature` | Air temperature | K |
| `environment.water.temperature` | Water temperature | K |
| `environment.tide.heightNow` | Current tide height | m |

### Electrical
| Path | Description | Unit |
|------|-------------|------|
| `electrical.batteries.{id}.voltage` | Battery voltage | V |
| `electrical.batteries.{id}.current` | Battery current | A |
| `electrical.batteries.{id}.stateOfCharge` | Charge level | 0-1 |

### Propulsion
| Path | Description | Unit |
|------|-------------|------|
| `propulsion.{id}.revolutions` | Engine RPM | Hz |
| `propulsion.{id}.temperature` | Engine temp | K |
| `propulsion.{id}.oilPressure` | Oil pressure | Pa |

### Tanks
| Path | Description | Unit |
|------|-------------|------|
| `tanks.fuel.{id}.currentLevel` | Fuel level | 0-1 |
| `tanks.freshWater.{id}.currentLevel` | Water level | 0-1 |

---

## WebSocket API

### Connection
```
ws://hostname:3000/signalk/v1/stream?subscribe=self
```

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

### Unsubscribe
```json
{
  "context": "*",
  "unsubscribe": [{"path": "*"}]
}
```

---

## REST API

| Endpoint | Purpose |
|----------|---------|
| `GET /signalk` | Discovery |
| `GET /signalk/v1/api/` | Full data |
| `GET /signalk/v1/api/vessels/self/navigation/speedOverGround` | Specific path |
| `PUT /signalk/v1/api/vessels/self/path` | Control command |

---

## Plugin PUT Handlers

Register handlers to receive commands:

```javascript
app.registerPutHandler('navigation.anchor.setAnchor', 
  (context, path, value, callback) => {
    if (!value.latitude || !value.longitude) {
      return callback({
        state: 'COMPLETED',
        statusCode: 400,
        message: 'Missing required fields'
      });
    }
    
    // Do something with value
    processAnchorSet(value);
    
    return callback({
      state: 'COMPLETED',
      statusCode: 200,
      message: 'Anchor set successfully'
    });
  }
);
```

### Calling via REST
```bash
curl -X PUT http://localhost:3000/signalk/v1/api/vessels/self/navigation/anchor/setAnchor \
  -H "Content-Type: application/json" \
  -d '{"value": {"latitude": 43.65, "longitude": 7.02}}'
```

---

## Plugin Lifecycle

```javascript
module.exports = function(app) {
  const plugin = {
    id: 'my-plugin',
    name: 'My Plugin',
    
    start: function(options) {
      // Initialize plugin
    },
    
    stop: function() {
      // Cleanup
    },
    
    schema: {
      // Configuration schema
    }
  };
  
  return plugin;
};
```

### Publishing Data
```javascript
app.handleMessage(plugin.id, {
  updates: [{
    values: [{
      path: 'navigation.speedOverGround',
      value: 3.5
    }]
  }]
});
```

### Subscribing to Data
```javascript
app.subscriptionmanager.subscribe(
  {
    context: 'vessels.self',
    subscribe: [{path: 'navigation.position'}]
  },
  unsubscribes,
  (err) => { /* error handler */ },
  (delta) => { /* data handler */ }
);
```

---

## Documentation Links

- **Specification**: https://signalk.org/specification/latest/
- **Path Reference**: https://signalk.org/specification/1.5.0/doc/vesselsBranch.html
- **Server Docs**: https://demo.signalk.org/documentation/
- **Plugin Dev**: https://demo.signalk.org/documentation/Developing/Plugins.html
