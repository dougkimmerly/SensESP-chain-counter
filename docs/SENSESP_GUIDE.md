# SensESP Quick Reference for AI Assistants

**Purpose**: Help AI assistants quickly understand SensESP concepts for marine sensor development.

---

## Tools Available

### SignalK MCP Server

You have access to the SignalK MCP server which provides real-time data from the Signal K server. Use these tools to:

- **Check existing paths**: See what data is already available before creating new sensors
- **Verify sensor output**: Confirm your SensESP device is publishing correctly
- **Debug integration**: Check if values are reaching the server

**Available MCP tools:**
- `mcp__signalk__get_navigation_data` - Get position, heading, speed
- `mcp__signalk__get_signalk_overview` - Get server info and available paths
- `mcp__signalk__get_ais_targets` - Get nearby vessels
- `mcp__signalk__get_system_alarms` - Get active alarms

**When to use:**
- Before implementing a sensor: Check if the path already exists
- After deploying: Verify the sensor is publishing to the correct path
- When debugging: See what values are actually reaching the server

### Research Command

When you need to research SensESP or Signal K topics that aren't covered in this guide, use the `/sensesp-research` command to spawn a background research agent:

```
/sensesp-research how do I subscribe to a Signal K path and react to changes?
```

This spawns an independent agent that will:
- Search local docs and source code
- Search SensESP library in `.pio/libdeps/`
- Fetch online documentation if needed
- Return detailed findings without consuming your session context

Use this for any non-trivial SensESP questions before attempting implementation.

---

## What is SensESP?

SensESP is an ESP32/ESP8266 framework for building Signal K sensors:
- Arduino-based development using PlatformIO
- Connects sensors directly to Signal K server via WiFi
- Built-in web UI for configuration
- Supports a wide variety of sensors (temperature, tank levels, engine data, etc.)

---

## Key Documentation Sources

| Resource | URL | Use For |
|----------|-----|---------|
| **Main Docs** | https://signalk.org/SensESP/ | Getting started |
| **GitHub** | https://github.com/SignalK/SensESP | Source code, examples |
| **Class Reference** | https://signalk.org/SensESP/docs/generated/docs/index.html | API reference |
| **Examples** | https://github.com/SignalK/SensESP/tree/main/examples | Code examples |

---

## Project Structure

```
my-sensesp-project/
├── platformio.ini          # PlatformIO configuration
├── src/
│   └── main.cpp            # Main application code
├── data/                   # SPIFFS files (web UI, config)
└── include/                # Header files (optional)
```

---

## platformio.ini Template

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    SignalK/SensESP @ ^3.0.0
monitor_speed = 115200
upload_speed = 921600

; For mDNS/WiFi config
board_build.partitions = min_spiffs.csv
```

---

## Basic Application Structure

```cpp
#include <Arduino.h>
#include "sensesp_app_builder.h"
#include "sensesp/sensors/analog_input.h"
#include "sensesp/transforms/linear.h"

using namespace sensesp;

SensESPApp* sensesp_app;

void setup() {
  // Create the SensESP application
  SensESPAppBuilder builder;
  sensesp_app = builder
    .set_hostname("my-sensor")
    .set_sk_server("192.168.1.100", 3000)
    .get_app();

  // Define sensor pipeline
  auto* analog_input = new AnalogInput(36, 1000);  // GPIO36, 1s interval

  analog_input
    ->connect_to(new Linear(1.0, 0.0, "/transforms/calibration"))
    ->connect_to(new SKOutputFloat("environment.outside.temperature"));

  sensesp_app->start();
}

void loop() {
  sensesp_app->tick();
}
```

---

## Core Concepts

### 1. Producers
Generate data from sensors:

```cpp
// Analog input (ADC)
new AnalogInput(gpio_pin, read_interval_ms)

// Digital input
new DigitalInputState(gpio_pin, INPUT_PULLUP, read_interval_ms)

// 1-Wire temperature (DS18B20)
new DallasTemperatureSensors(one_wire_pin)

// I2C sensors
new BME280(i2c_addr)  // Temperature, humidity, pressure
```

### 2. Transforms
Process and convert data:

```cpp
// Linear scaling: output = input * multiplier + offset
new Linear(multiplier, offset, config_path)

// Moving average
new MovingAverage(samples, config_path)

// Median filter
new Median(samples, config_path)

// Voltage divider (for battery monitoring)
new VoltageDividerR2(R1_ohms, R2_ohms, config_path)

// Change filter (only output when value changes)
new ChangeFilter(min_delta, max_delta, max_skips, config_path)

// Lambda (custom transform)
new LambdaTransform<float, float>([](float input) {
  return input * 2.0;
}, config_path)
```

### 3. Consumers
Output data to Signal K or other destinations:

```cpp
// Signal K output
new SKOutputFloat("path.to.value", config_path)
new SKOutputInt("path.to.value", config_path)
new SKOutputBool("path.to.value", config_path)
new SKOutputString("path.to.value", config_path)

// Position output (lat/lon)
new SKOutputPosition("navigation.position", config_path)
```

---

## Common Sensor Patterns

### Temperature Sensor (DS18B20)

```cpp
#include "sensesp/sensors/onewire_temperature.h"

auto* dallas = new DallasTemperatureSensors(ONE_WIRE_PIN);
auto* engine_temp = new OneWireTemperature(dallas, "/sensors/engine_temp");

engine_temp
  ->connect_to(new Linear(1.0, 273.15, "/transforms/to_kelvin"))  // C to K
  ->connect_to(new SKOutputFloat("propulsion.main.temperature"));
```

### Tank Level (Analog)

```cpp
auto* tank_input = new AnalogInput(TANK_PIN, 5000);

tank_input
  ->connect_to(new Linear(1.0/4095.0, 0.0, "/transforms/normalize"))  // 0-1
  ->connect_to(new MovingAverage(10, "/transforms/smooth"))
  ->connect_to(new SKOutputFloat("tanks.fuel.main.currentLevel"));
```

### Battery Voltage

```cpp
auto* voltage_input = new AnalogInput(BATT_PIN, 1000);

voltage_input
  ->connect_to(new VoltageDividerR2(100000, 27000, "/transforms/divider"))
  ->connect_to(new SKOutputFloat("electrical.batteries.house.voltage"));
```

### RPM from Pulse Counter

```cpp
#include "sensesp/sensors/digital_input.h"

auto* rpm_input = new DigitalInputCounter(RPM_PIN, INPUT_PULLUP, RISING, 1000);

rpm_input
  ->connect_to(new Frequency(1.0, "/transforms/to_hz"))  // pulses/sec
  ->connect_to(new SKOutputFloat("propulsion.main.revolutions"));
```

---

## WiFi Configuration

On first boot:
1. ESP creates AP: "Configure <hostname>"
2. Connect to AP, open 192.168.4.1
3. Enter WiFi credentials and Signal K server address
4. ESP reboots and connects

Or set in code:
```cpp
builder
  .set_wifi("SSID", "password")
  .set_sk_server("192.168.1.100", 3000)
```

---

## Configuration Paths

Every configurable component has a path like:
- `/sensors/engine_temp`
- `/transforms/calibration`
- `/sk/engine_temp`

Access config UI at: `http://<hostname>.local/`

---

## Signal K Path Conventions

Use standard Signal K paths:

| Measurement | Signal K Path |
|-------------|---------------|
| Engine temp | `propulsion.{id}.temperature` |
| Oil pressure | `propulsion.{id}.oilPressure` |
| Coolant temp | `propulsion.{id}.coolantTemperature` |
| RPM | `propulsion.{id}.revolutions` |
| Battery voltage | `electrical.batteries.{id}.voltage` |
| Tank level | `tanks.{type}.{id}.currentLevel` |
| Bilge state | `notifications.bilge.{id}` |

---

## Debugging

```cpp
// Enable debug output
#define DEBUG_ESP_PORT Serial

// In code
debugD("Debug message: %f", value);
debugI("Info message");
debugW("Warning message");
debugE("Error message");
```

Monitor: `pio run -t monitor`

---

## Common Issues

### WiFi Won't Connect
- Check credentials
- Ensure 2.4GHz network (ESP doesn't support 5GHz)
- Try static IP

### Signal K Connection Failed
- Verify server IP and port
- Check authentication settings
- Ensure mDNS works or use IP address

### Sensor Not Reading
- Check wiring and GPIO pin
- Verify pull-up/pull-down resistors
- Check power supply (some sensors need 5V)

---

## Useful Libraries

```ini
lib_deps =
    SignalK/SensESP @ ^3.0.0
    paulstoffregen/OneWire
    milesburton/DallasTemperature
    adafruit/Adafruit BME280 Library
```

---

## ESP32 GPIO Reference

| GPIO | Notes |
|------|-------|
| 36, 39, 34, 35 | Input only, no pull-up |
| 32, 33 | ADC + touch |
| 25, 26, 27 | ADC + DAC |
| 21, 22 | Default I2C (SDA, SCL) |
| 16, 17 | Default UART2 |
| 4 | Default 1-Wire |

Avoid: GPIO 0, 2, 15 (boot pins), GPIO 6-11 (flash)

---

## Quick Reference Links

- **Getting Started**: https://signalk.org/SensESP/pages/getting_started/
- **API Reference**: https://signalk.org/SensESP/docs/generated/docs/
- **Examples**: https://github.com/SignalK/SensESP/tree/main/examples
- **Signal K Paths**: https://signalk.org/specification/latest/

---

Last Updated: 2025-12-04
