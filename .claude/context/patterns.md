# Patterns Context

> C++/SensESP coding patterns and standards for this project.

## Code Style

From `.clang-format`:
- Indentation: 4 spaces
- Braces: Same line for functions
- Pointer alignment: Left (`float* ptr`)

## C++ Patterns Used

### Class Structure

```cpp
// Header file (.h)
class ChainController {
public:
    // Constructor
    ChainController(float min, float max, ...);
    
    // Public methods
    void lowerAnchor(float amount);
    void stop();
    float getChainLength() const;  // const for getters
    
    // Public constants
    static constexpr float BOW_HEIGHT_M = 2.0;
    
private:
    // Member variables (trailing underscore)
    float min_length_;
    float target_;
    ChainState state_;
    
    // Private helpers
    void updateTimeout(float amount, float speed);
};
```

### Enum Classes

```cpp
// Prefer enum class over plain enum (type safety)
enum class ChainState {
    IDLE,
    LOWERING,
    RAISING
};

// Usage
if (state_ == ChainState::IDLE) { ... }
```

### Constants

```cpp
// Use constexpr for compile-time constants
static constexpr float BOW_HEIGHT_M = 2.0;
static constexpr unsigned long COOLDOWN_MS = 3000;

// In header, inside class
class ChainController {
    static constexpr float PAUSE_SLACK_M = 0.2;
};
```

### Null Checks

```cpp
// Check pointers before use
if (depthListener_ != nullptr) {
    float depth = depthListener_->get();
}

// Or use early return
if (accumulator_ == nullptr) return;
```

### Safe Math

```cpp
// Avoid division by zero
if (depth > 0.01) {
    float scope = chain / depth;
}

// Use fmax/fmin for bounds
float effective_depth = fmax(0.0, depth - BOW_HEIGHT);

// Check for valid numbers
if (!isnan(value) && !isinf(value)) {
    // Use value
}
```

## SensESP Patterns

### Creating Observable Values

```cpp
// In constructor or setup
horizontalSlack_ = new ObservableValue<float>(0.0);

// Connect to SignalK
horizontalSlack_->connect_to(
    new SKOutputFloat("navigation.anchor.chainSlack", "/sensors/anchor/slack")
);
```

### Creating Listeners

```cpp
// In constructor
depthListener_ = new SKValueListener<float>("environment.depth.belowSurface");

// In main.cpp, connect callback
chainController->getDepthListener()->connect_to(
    new LambdaConsumer<float>([chainController](float depth) {
        chainController->setDepthBelowSurface(depth);
    })
);
```

### Lambda Consumers

```cpp
// For simple callbacks
new LambdaConsumer<float>([](float value) {
    Serial.printf("Received: %.2f\n", value);
});

// Capturing external variables
new LambdaConsumer<float>([controller](float value) {
    controller->setValue(value);
});
```

### ReactESP Timing

```cpp
// One-shot delayed action
reactESP->onDelay(5000, []() {
    Serial.println("5 seconds elapsed");
});

// Repeating action
reactESP->onRepeat(500, []() {
    // Called every 500ms
    calculateSlack();
});
```

## Hardware Patterns

### Relay Control

```cpp
// Initialize pins
pinMode(downRelayPin_, OUTPUT);
pinMode(upRelayPin_, OUTPUT);

// Activate relay (active LOW typically)
digitalWrite(downRelayPin_, LOW);   // ON
digitalWrite(downRelayPin_, HIGH);  // OFF

// Always stop before changing direction
void changeDirection() {
    digitalWrite(downRelayPin_, HIGH);  // Stop down
    digitalWrite(upRelayPin_, HIGH);    // Stop up
    delay(100);                          // Let relays settle
    // Now safe to activate new direction
}
```

### Reading Accumulated Position

```cpp
// Get current chain length from integrator
float current = accumulator_->get();

// Reset position (e.g., when chain fully up)
accumulator_->set(0.0);
```

## Logging

### Serial Output

```cpp
// Use ESP_LOG macros for level-controlled logging
ESP_LOGI("ChainCtrl", "Lowering %.2fm", amount);
ESP_LOGW("ChainCtrl", "Timeout exceeded!");
ESP_LOGE("ChainCtrl", "Invalid state: %d", state);

// Or Serial.printf for quick debugging
Serial.printf("[SLACK] chain=%.2f depth=%.2f slack=%.2f\n", 
              chain, depth, slack);
```

### Log Levels

```
ARDUHAL_LOG_LEVEL_NONE      (0)
ARDUHAL_LOG_LEVEL_ERROR     (1)
ARDUHAL_LOG_LEVEL_WARN      (2)
ARDUHAL_LOG_LEVEL_INFO      (3)
ARDUHAL_LOG_LEVEL_DEBUG     (4)
ARDUHAL_LOG_LEVEL_VERBOSE   (5)
```

Set in `platformio.ini`:
```ini
build_flags = -D CORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_VERBOSE
```

## File Organization

```
src/
├── main.cpp              # Entry point, setup, loop, commands
├── ChainController.cpp   # Motor control, physics
├── ChainController.h     # Class definition, constants
├── DeploymentManager.cpp # Automation FSM
└── DeploymentManager.h   # Stage definitions
```

## Build Commands

```bash
# Full path to pio (if not in PATH)
~/.platformio/penv/bin/platformio run

# Or if pio is in PATH
pio run                    # Build
pio run -t clean           # Clean
pio run -t upload          # Upload to ESP32
pio device monitor         # Serial monitor
```

## Common Gotchas

1. **Forgetting `const`** on getter methods
2. **Integer division** - use `float` or cast: `float(a) / b`
3. **Pointer lifetime** - SensESP objects must persist (use `new`)
4. **Relay active state** - Check if active HIGH or LOW
5. **Blocking delays** - Avoid `delay()` in loop, use ReactESP
