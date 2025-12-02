# SensESP Chain Counter - Claude Context Guide

**Quick Context Setup for AI Assistants**

This file is your starting point for understanding the codebase. Follow the links below based on what you need to accomplish.

---

## Project Overview

**What is this?**
- Marine anchor chain deployment and retrieval control system
- Runs on ESP32 microcontroller with Signal K integration
- Controls motorized windlass (chain winch) with relay control
- Calculates anchor position using catenary physics

**Main Components:**
- [ChainController](src/ChainController.cpp) - Low-level motor control and position tracking
- [DeploymentManager](src/DeploymentManager.cpp) - Automated anchor deployment sequence
- [RetrievalManager](src/RetrievalManager.cpp) - Automated anchor retrieval sequence

**Key Technology:**
- C++ on ESP32 (PlatformIO build system)
- Signal K protocol for marine instrument integration
- Catenary physics for accurate chain slack calculations
- State machine FSMs for deployment/retrieval logic

---

## Documentation Index

### 👉 Start Here
- **[CODEBASE_OVERVIEW.md](docs/CODEBASE_OVERVIEW.md)** - Architecture, file structure, class relationships
  - Read this first for understanding the big picture
  - Maps which files contain which functionality
  - Explains the overall design philosophy

### For Development Tasks

- **[ARCHITECTURE_MAP.md](docs/ARCHITECTURE_MAP.md)** - Visual system overview
  - Component interaction diagrams
  - State flow visualizations
  - Data flow during operations
  - When things go wrong (error handling)
  - How to add new features
  - Read this to understand the big picture

- **[DEPLOYMENT_REFACTOR_PLAN.md](docs/DEPLOYMENT_REFACTOR_PLAN.md)** - Current implementation status and architecture decisions
  - Deployment and retrieval system details
  - Problem analysis and solutions applied
  - Testing validation procedures
  - Reference when modifying deployment/retrieval logic

- **[MCP ChainController Documentation](docs/mcp_chaincontroller.md)** - Complete API reference
  - Public methods and their behavior
  - State machine definitions
  - Catenary physics model
  - Integration points with other systems
  - Use when implementing features that interact with ChainController

- **[QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** - Code locations and constants
  - File locations by function
  - Constants reference
  - Debugging tips
  - Signal K paths
  - Build commands

### README Files
- **[README.md](README.md)** - Setup, building, and running the firmware
  - Development environment setup
  - How to compile and upload firmware
  - Hardware connections and pin assignments

---

## Quick Reference by Task

### "I need to fix a bug in deployment"
1. [ARCHITECTURE_MAP.md](docs/ARCHITECTURE_MAP.md) - Section: "Deployment State Flow" & "When Things Go Wrong"
2. [DEPLOYMENT_REFACTOR_PLAN.md](docs/DEPLOYMENT_REFACTOR_PLAN.md) for current implementation details
3. [QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md) to find the exact code location

### "I need to understand how slack calculation works"
1. [ARCHITECTURE_MAP.md](docs/ARCHITECTURE_MAP.md) - Section: "Slack Calculation Flow"
2. [mcp_chaincontroller.md](docs/mcp_chaincontroller.md) - "Slack Calculation" & "Catenary Physics Model"
3. Code: `calculateAndPublishHorizontalSlack()` in [src/ChainController.cpp](src/ChainController.cpp):305

### "I need to add a new feature to the deployment process"
1. [ARCHITECTURE_MAP.md](docs/ARCHITECTURE_MAP.md) - Understand state flows and data flow
2. [DEPLOYMENT_REFACTOR_PLAN.md](docs/DEPLOYMENT_REFACTOR_PLAN.md) - Current implementation
3. [QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md) - Find code locations

### "I need to understand the architecture"
1. [CODEBASE_OVERVIEW.md](docs/CODEBASE_OVERVIEW.md) - Architecture & design
2. [ARCHITECTURE_MAP.md](docs/ARCHITECTURE_MAP.md) - Visual diagrams & flows
3. [mcp_chaincontroller.md](docs/mcp_chaincontroller.md) - Core controller details

### "I need to find a specific piece of code"
1. [QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md) - "File Locations by Function"
2. Use Ctrl+F to search for file:line references

### "I need to work with the build system"
1. [README.md](README.md) - Build and upload instructions
2. `platformio.ini` - PlatformIO configuration
3. `.vscode/` - VSCode settings and debugging configuration

---

## Current Status

### ✅ Completed Work
- [x] Centralized catenary physics in ChainController
- [x] Implemented continuous deployment monitoring (replaced pulsed system)
- [x] Fixed retrieval relay wear (added cooldown and hysteresis)
- [x] Corrected slack calculation for 2m bow height offset
- [x] Removed debug logging from slack calculations
- [x] Added tide-adjusted depth calculation for deployment
- [x] Added configurable scope ratio (autoDrop3, autoDrop7, etc.)

### 🔧 Key Features
- **Tide-Adjusted Deployment**: Calculates chain for high tide conditions using `environment.tide.heightNow` and `environment.tide.heightHigh`
- **Configurable Scope**: `autoDrop` accepts scope ratio suffix (e.g., `autoDrop7` for 7:1, range 3.0-10.0)
- **Slack Calculation**: Accounts for bow height (2m above water) when computing effective depth
- **Deployment**: Continuous deployment with monitoring every 500ms
- **Retrieval**: 3-second cooldown and 1.0m minimum raise threshold to prevent relay wear

---

## File Structure Reference

```
SensESP-chain-counter/
├── claude.md                          (← You are here)
├── README.md                          (Setup & building)
├── src/
│   ├── ChainController.cpp/h          (Core motor control)
│   ├── DeploymentManager.cpp/h        (Deployment FSM)
│   ├── RetrievalManager.cpp/h         (Retrieval FSM)
│   ├── main.cpp                       (Firmware entry point)
│   └── events.h                       (Custom events)
├── docs/
│   ├── CODEBASE_OVERVIEW.md          (Architecture guide)
│   ├── DEPLOYMENT_REFACTOR_PLAN.md   (Implementation details)
│   └── mcp_chaincontroller.md        (API reference)
├── platformio.ini                     (Build configuration)
└── .vscode/                           (Editor settings)
```

---

## Key Concepts to Remember

### State Machines
Both deployment and retrieval use finite state machines:
- **Deployment**: DROP → WAIT_TIGHT → HOLD_DROP → DEPLOY_30 → WAIT_30 → HOLD_30 → DEPLOY_75 → WAIT_75 → HOLD_75 → DEPLOY_100 → COMPLETE
- **Retrieval**: CHECKING_SLACK → RAISING → WAITING_FOR_SLACK → COMPLETE

### Catenary Physics
The chain doesn't hang straight - it sags due to:
- Chain weight: 2.2 kg/m in water
- Horizontal forces: Wind drag + current resistance
- Result: Reduction factor (0.80-0.99) applied to theoretical straight-line distance

### Safety Limits
- Max slack: 85% of effective depth (prevents chain from losing grip on seabed)
- Min raise amount: 1.0m (prevents rapid relay cycling during retrieval)
- Cooldown period: 3 seconds between raises (relay protection)

---

## When to Update Documentation

- **CODEBASE_OVERVIEW.md** - When class structure changes or new major components added
- **DEPLOYMENT_REFACTOR_PLAN.md** - When deployment/retrieval logic changes
- **mcp_chaincontroller.md** - When ChainController public API changes
- **claude.md** (this file) - When doc locations or quick reference tasks change

---

## Tips for Efficient Context Usage

1. **Use relative links** when referencing files - start with `docs/` or `src/`
2. **Check the index** at the top of each docs file for section locations
3. **Reference code by file:line** when discussing specific implementations
4. **Update the Status section** above when completing work
5. **Keep docs in sync** with code changes to avoid confusion

---

Last Updated: 2025-12-02
