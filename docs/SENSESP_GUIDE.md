# SensESP Framework Quick Reference for AI Assistants

**Purpose**: Help AI assistants quickly understand SensESP patterns without reading the full documentation.

---

## What is SensESP?

SensESP is a Signal K sensor framework for ESP32 microcontrollers. It provides:
- WiFi connectivity with auto-configuration
- Signal K protocol integration (publish/subscribe)
- Reactive event loop (ReactESP)
- Web UI for runtime configuration
- Sensor → Transform → Output data pipeline

---

## Key Documentation Sources

| Resource | URL | Use For |
|----------|-----|---------|
| **Official Docs** | https://signalk.org/SensESP/ | Tutorials, concepts, getting started |
| **API Reference** | https://signalk.org/SensESP/generated/docs/ | Class definitions, method signatures |
| **GitHub Repo** | https://github.com/SignalK/SensESP | Source code, examples, issues |
| **Examples** | https://github.com/SignalK/SensESP/tree/main/examples | Working code patterns |
| **Signal K Spec** | https://signalk.org/specification/1.7.0/doc/ | Path naming, units, data model |
| **ReactESP** | https://github.com/mairas/ReactESP | Event loop documentation |

---

## Core Concepts

### 1. Data Flow Pipeline

```
Sensor → Transform(s) → SKOutput → Signal K Server
                                         ↓
SKValueListener ← Signal K Server (incoming data)
```

### 2. The ReactESP Event Loop

SensESP uses ReactESP for non-blocking async operations. **Never use `delay()`** in the main loop.

```cpp
// Access the event loop
#include "sensesp.h"

// Periodic task (every 1000ms)
sensesp::event_loop()->onRepeat(1000, []() {
    // Your code here
});

// One-time delayed task
sensesp::event_loop()->onDelay(5000, []() {
    // Runs once after 5 seconds
});

// Main loop must call tick()
void loop() {
    sensesp::event_loop()->tick();
}
```

### 3. Configuration Paths

Paths starting with `/` are persisted and appear in the web UI:
```cpp
new SKOutputFloat("navigation.anchor.rodeDeployed", "/anchor/rode/sk");
//                 ↑ Signal K path                   ↑ Config path (stored)
```

---

## Common Patterns

### Publishing to Signal K (SKOutput)

Use SKOutput variants to send data to Signal K:

```cpp
#include "sensesp/signalk/signalk_output.h"

// Float values
new SKOutputFloat("path.to.value", "/config/path");

// String values
new SKOutputString("path.to.string", "/config/path");

// Boolean values
new SKOutputBool("path.to.bool", "/config/path");

// Integer values
new SKOutputInt("path.to.int", "/config/path");
```

**With metadata (units, description):**
```cpp
auto metadata = new SKMetadata("K", "Engine temperature");  // units, description
new SKOutputFloat("propulsion.engine.temperature", "/engine/temp/sk", metadata);
```

### Listening to Signal K (SKValueListener)

Use SKValueListener to receive data from Signal K:

```cpp
#include "sensesp/signalk/signalk_value_listener.h"

// Listen to a float value (2000ms timeout, config path)
auto* depthListener = new sensesp::SKValueListener<float>(
    "environment.depth.belowSurface",  // Signal K path to listen to
    2000,                               // Timeout in ms
    "/depth/sk"                         // Config path
);

// Get the current value
float depth = depthListener->get();
```

### Observable Values (Internal State)

Use ObservableValue to create reactive state that can be connected to outputs:

```cpp
#include "sensesp/system/observable.h"

// Create an observable
auto* myValue = new sensesp::ObservableValue<float>(0.0);

// Update it (triggers connected outputs)
myValue->set(42.5);

// Connect to Signal K output
myValue->connect_to(new SKOutputFloat("my.signal.path", "/my/config"));

// Get current value
float current = myValue->get();
```

### Transform Chains

Connect sensors through transforms to outputs:

```cpp
// Basic chain: sensor → transform → output
auto* sensor = new AnalogInput(36, 1000, "/sensor/config");
sensor->connect_to(new Linear(1.0, 0.0, "/calibration"))
      ->connect_to(new SKOutputFloat("my.sensor.path", "/output/config"));

// Lambda transform for custom logic
auto* transform = new LambdaTransform<float, bool>([](float input) -> bool {
    return input > 100.0;
});
```

---

## Signal K Path Conventions

### Common Path Prefixes

| Prefix | Use For |
|--------|---------|
| `navigation.*` | Position, speed, course, anchor |
| `environment.*` | Depth, wind, temperature, tide |
| `electrical.*` | Batteries, alternators, solar |
| `propulsion.*` | Engine data |
| `tanks.*` | Fuel, water, waste levels |

### Units (Always SI)

| Measurement | Unit |
|-------------|------|
| Temperature | Kelvin (K) |
| Distance | Meters (m) |
| Speed | m/s |
| Pressure | Pascals (Pa) |
| Voltage | Volts (V) |
| Frequency | Hertz (Hz) |

---

## Project Structure

Typical SensESP project:
```
project/
├── platformio.ini          # PlatformIO config
├── src/
│   └── main.cpp           # Entry point, setup sensors
└── include/               # Custom headers
```

### platformio.ini essentials

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    SignalK/SensESP @ ^3.0.0
monitor_speed = 115200
```

---

## Application Builder Pattern

```cpp
#include "sensesp_app_builder.h"

void setup() {
    SensESPAppBuilder builder;

    sensesp_app = builder
        .set_hostname("my-device")
        .set_wifi("SSID", "password")           // Optional: hard-code WiFi
        .set_sk_server("192.168.1.5", 3000)     // Optional: hard-code server
        .get_app();

    // Add your sensors and outputs here...
}

void loop() {
    sensesp::event_loop()->tick();
}
```

---

## Debugging Tips

### Check Signal K Connection
- Device creates AP "Configure SensESP" (password: "thisisfine") on first boot
- Access web UI at 192.168.4.1 when connected to device AP
- Check Serial monitor at 115200 baud for connection status

### Common Issues

| Problem | Solution |
|---------|----------|
| Won't connect to SK server | Disable SSL on Signal K server |
| Values not appearing | Check Signal K path format (dots not slashes) |
| Config not saving | Ensure config path starts with `/` |
| Event loop blocking | Remove any `delay()` calls |

---

## Migration Notes (v2 → v3)

Key changes in SensESP 3.x:
- ReactESP is no longer a singleton - use `sensesp::event_loop()`
- `ReactESP::app` removed - use `event_loop()`
- ReactESP class renamed to `reactesp::EventLoop`
- WSClient renamed to SKWSClient

---

## Quick Examples from This Project

### Publishing a String to Signal K
```cpp
// From DeploymentManager - publishing stage name
autoStageObservable_ = new sensesp::ObservableValue<String>("Idle");
autoStageObservable_->connect_to(
    new sensesp::SKOutputString("navigation.anchor.autoStage", "/anchor/autoStage")
);

// Update the value (publishes automatically)
autoStageObservable_->set("Deploy 40");
```

### Listening to Multiple Signal K Values
```cpp
// From ChainController - listening to depth, distance, wind, tide
depthListener_ = new sensesp::SKValueListener<float>("environment.depth.belowSurface", 2000, "/depth/sk");
distanceListener_ = new sensesp::SKValueListener<float>("navigation.anchor.distanceFromBow", 2000, "/distance/sk");
windSpeedListener_ = new sensesp::SKValueListener<float>("environment.wind.speedTrue", 2000, "/wind/sk");
tideHeightNowListener_ = new sensesp::SKValueListener<float>("environment.tide.heightNow", 2000, "/tide/heightNow/sk");

// Get values
float depth = depthListener_->get();
float wind = windSpeedListener_->get();
```

### Scheduled Repeating Task
```cpp
// From DeploymentManager - update loop every 1ms
updateEvent = sensesp::event_loop()->onRepeat(1, std::bind(&DeploymentManager::updateDeployment, this));

// Clean up when done
if (updateEvent != nullptr) {
    sensesp::event_loop()->remove(updateEvent);
    updateEvent = nullptr;
}
```

---

## Where to Look for More

1. **Concepts & Architecture**: https://signalk.org/SensESP/pages/concepts/
2. **Tutorials**: https://signalk.org/SensESP/pages/tutorials/
3. **Lambda Transforms**: https://signalk.org/SensESP/pages/tutorials/lambda_transform/
4. **Arduino Integration**: https://signalk.org/SensESP/pages/tutorials/arduino_style/
5. **Signal K Paths Reference**: https://signalk.org/specification/1.7.0/doc/vesselsBranch.html

---

Last Updated: 2025-12-02
