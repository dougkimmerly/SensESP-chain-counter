# SensESP Chain Counter

## Description

This project implements a bi-directional chain counter based on SensESP. It includes the following features:

- **Dynamic Configuration** via the SensESP user interface (UI) for:
  - Gypsy circumference
  - Free fall return delay
  - GPIO and debounce time for 4 digital inputs (UP, DOWN, Hall sensor, RESET)
- **Saving/Restoring** the chain length in non-volatile memory (via `Preferences`)
- **Digital Input Management** with software debounce (`DebounceInt`)
- **Chain Counting** with an accumulator (`Integrator`)
- **Data Publishing** to a Signal K server:
  - Deployed chain length (`navigation.anchor.rodeDeployed`)
  - Movement direction (`navigation.anchor.chainDirection`)
- **Event Reactions**:
  - UP/DOWN change the direction
  - Hall sensor increments/decrements based on the direction
  - RESET resets the length to zero

## Features

- Dynamic configuration via SensESP UI
- Non-volatile memory storage for chain length
- Software debounce for digital inputs
- Accumulator-based chain counting
- Data publishing to Signal K server
- Event-based reactions for UP, DOWN, Hall sensor, and RESET

## Dependencies

- SensESP library
- Preferences library
- Signal K server

