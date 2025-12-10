# /implement - Feature Development

> Use when adding features, modifying control logic, or extending functionality.

## Workflow

### Phase 1: Understand

1. **Read context files as needed:**
   - `.claude/context/architecture.md` - State machines, data flow
   - `.claude/context/domain.md` - SignalK paths, physics
   - `.claude/context/patterns.md` - C++/SensESP patterns

2. **Identify affected files:**
   - `src/ChainController.cpp` - Motor control, physics
   - `src/DeploymentManager.cpp` - Automation logic
   - `src/main.cpp` - Commands, main loop

3. **Check existing constants** in header files before adding new ones.

### Phase 2: Plan

1. List specific changes needed
2. Identify any new SignalK paths
3. Consider safety implications (see `.claude/context/safety.md`)
4. Plan testing approach

### Phase 3: Implement

1. Make focused, minimal changes
2. Follow coding standards:
   - 4-space indent
   - Trailing underscore for member variables
   - `constexpr` for constants

3. Use established patterns:
   - Relay control via ChainController methods
   - SignalK via ObservableValue
   - Timing via ReactESP

4. Add logging for debugging:
   ```cpp
   ESP_LOGI("TAG", "Description: %.2f", value);
   ```

### Phase 4: Build & Test

1. **Build:**
   ```bash
   pio run
   ```

2. **Check for errors/warnings** in build output

3. **Upload:**
   ```bash
   pio run -t upload
   ```

4. **Monitor serial:**
   ```bash
   pio device monitor --baud 115200
   ```

5. **Test the specific feature** via SignalK commands or physical buttons

### Phase 5: Report

Summarize:
- Files changed
- What was added/modified
- How it was tested
- Any follow-up items

---

## Quick Reference

### Add a New SignalK Output

```cpp
// In ChainController.h - add member
ObservableValue<float>* newValue_;

// In ChainController.cpp constructor
newValue_ = new ObservableValue<float>(0.0);

// In main.cpp setup - connect to SignalK
chainController->getNewValueObservable()->connect_to(
    new SKOutputFloat("navigation.anchor.newPath", "/sensors/anchor/new")
);

// Update the value
newValue_->set(calculatedValue);
```

### Add a New SignalK Listener

```cpp
// In ChainController.h - add member
SKValueListener<float>* newListener_;

// In ChainController.cpp constructor
newListener_ = new SKValueListener<float>("some.signalk.path");

// In main.cpp setup - connect callback
chainController->getNewListener()->connect_to(
    new LambdaConsumer<float>([chainController](float value) {
        chainController->setNewValue(value);
    })
);
```

### Add a New Constant

```cpp
// In header file, inside class
static constexpr float NEW_THRESHOLD_M = 1.5;

// Usage in .cpp file
if (value > NEW_THRESHOLD_M) { ... }
```

### Add a New Command

```cpp
// In main.cpp, in the command handler section
if (command == "newCommand") {
    // Validate state
    if (!chainController->isActive()) {
        // Execute action
        chainController->doSomething();
    }
}
```

---

## Task

$ARGUMENTS
