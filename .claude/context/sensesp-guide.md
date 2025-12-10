# SensESP Quick Reference

> ESP32 sensor development with Signal K integration

---

## Project Structure

```
my-sensesp-project/
├── platformio.ini          # PlatformIO configuration
├── src/
│   └── main.cpp            # Main application code
├── include/                # Header files
└── data/                   # SPIFFS files (web UI)
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
  SensESPAppBuilder builder;
  sensesp_app = builder
    .set_hostname("my-sensor")
    .set_sk_server("192.168.1.100", 3000)
    .get_app();

  // Define sensor pipeline
  auto* input = new AnalogInput(36, 1000);  // GPIO36, 1s interval

  input
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
new OneWireTemperature(dallas, config_path)

// Pulse counter (RPM, flow)
new DigitalInputCounter(gpio_pin, INPUT_PULLUP, RISING, read_interval_ms)
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

// Change filter (only output when value changes)
new ChangeFilter(min_delta, max_delta, max_skips, config_path)

// Lambda (custom transform)
new LambdaTransform<float, float>([](float input) {
  return input * 2.0;
}, config_path)

// Frequency (pulses to Hz)
new Frequency(1.0, config_path)
```

### 3. Consumers
Output data to Signal K:

```cpp
new SKOutputFloat("path.to.value", config_path)
new SKOutputInt("path.to.value", config_path)
new SKOutputBool("path.to.value", config_path)
new SKOutputString("path.to.value", config_path)
```

---

## Common Sensor Patterns

### Temperature Sensor (DS18B20)
```cpp
#include "sensesp/sensors/onewire_temperature.h"

auto* dallas = new DallasTemperatureSensors(ONE_WIRE_PIN);
auto* temp = new OneWireTemperature(dallas, "/sensors/engine_temp");

temp
  ->connect_to(new Linear(1.0, 273.15, "/transforms/to_kelvin"))  // °C to K
  ->connect_to(new SKOutputFloat("propulsion.main.temperature"));
```

### Tank Level (Analog)
```cpp
auto* tank = new AnalogInput(TANK_PIN, 5000);

tank
  ->connect_to(new Linear(1.0/4095.0, 0.0, "/transforms/normalize"))  // 0-1
  ->connect_to(new MovingAverage(10, "/transforms/smooth"))
  ->connect_to(new SKOutputFloat("tanks.fuel.main.currentLevel"));
```

### Battery Voltage
```cpp
auto* voltage = new AnalogInput(BATT_PIN, 1000);

voltage
  ->connect_to(new VoltageDividerR2(100000, 27000, "/transforms/divider"))
  ->connect_to(new SKOutputFloat("electrical.batteries.house.voltage"));
```

### RPM from Pulses
```cpp
auto* rpm = new DigitalInputCounter(RPM_PIN, INPUT_PULLUP, RISING, 1000);

rpm
  ->connect_to(new Frequency(1.0, "/transforms/to_hz"))
  ->connect_to(new SKOutputFloat("propulsion.main.revolutions"));
```

---

## WiFi Configuration

On first boot:
1. ESP creates AP: "Configure [hostname]"
2. Connect to AP, open 192.168.4.1
3. Enter WiFi credentials and Signal K server
4. ESP reboots and connects

Or hardcode:
```cpp
builder
  .set_wifi("SSID", "password")
  .set_sk_server("192.168.1.100", 3000)
```

---

## Debugging

```cpp
// Enable debug output
debugD("Debug: %f", value);
debugI("Info message");
debugW("Warning");
debugE("Error");
```

Monitor: `pio run -t monitor`

---

## ESP32 GPIO Reference

| GPIO | Notes |
|------|-------|
| 36, 39, 34, 35 | Input only, no pull-up |
| 32, 33 | ADC + touch |
| 25, 26, 27 | ADC + DAC |
| 21, 22 | Default I2C (SDA, SCL) |
| 16, 17 | Default UART2 |
| 4 | Common for 1-Wire |

**Avoid**: GPIO 0, 2, 15 (boot pins), GPIO 6-11 (flash)

---

## Configuration Paths

Every configurable component has a path:
- `/sensors/engine_temp`
- `/transforms/calibration`
- `/sk/output`

Access config UI at: `http://[hostname].local/`

---

## Subscribing to Signal K Data

```cpp
#include "sensesp/signalk/signalk_listener.h"

auto* listener = new FloatSKListener("environment.depth.belowSurface");
listener->connect_to(new LambdaConsumer<float>([](float depth) {
  // React to depth changes
}));
```

---

## Common Issues

| Issue | Solution |
|-------|----------|
| WiFi won't connect | Check 2.4GHz (no 5GHz support) |
| SK connection failed | Verify IP/port, check auth |
| Sensor not reading | Check wiring, pull-up resistors |
| Values not appearing | Verify SK path format |

---

## Documentation Links

- **Main Docs**: https://signalk.org/SensESP/
- **GitHub**: https://github.com/SignalK/SensESP
- **Examples**: https://github.com/SignalK/SensESP/tree/main/examples
- **Class Reference**: https://signalk.org/SensESP/docs/generated/docs/
