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



## Updates  October 23, 2025

### Reset Chain Count via Signal K
- **PUT to `navigation.anchor.rodeDeployed` with a value of `1`** will now reset the chain counter.  
  This allows remote resetting of the counter through Signal K updates using a standard HTTP PUT request.

#### How to send the PUT request:
You can use SKipper, WilhelmSK, Your own SKPlugin or `curl`, Postman, as well as any HTTP client to send this request:

    curl -X PUT \
         -H "Content-Type: application/json" \
         -d '{"value":1}' \
         http://<your_signalk_server>/signalk/v1/api/vessels/self/navigation/anchor/rodeDeployed

Replace `<your_signalk_server>` with your Signal K server URL.

---

### Windlass Control Logic
- **Added windlass control logic** to manage deploying and retrieving the anchor via relay pins, with safety features such as stopping from both sensor feedback and a timed drop.  It learns the speed your windlass takes to go up or down and adds a stop timeout in case there are sensor issues.  If a new command is received while one is already active, it will instantly stop windlass movement and then continue with the new command.
- Commands can be sent via Signal K as updates to `navigation.anchor.command`.

#### Commands supported:
- `"drop"`: Initiates the initial drop of the anchor, lowering it until the depth plus 4 meters.  
  > **Note:** This "drop" is intended for the initial anchor deployment. The 4-meter additional slack accounts for the bow height (approximately 2 meters) and an initial chain on the seabed (another 2 meters), ensuring the anchor is ready to set.
- `"raiseXX"`: Raises the anchor by XX meters (e.g., `"raise10"`).
- `"lowerXX"`: Lowers the anchor by XX meters (e.g., `"lower10"`).
- `"STOP"`: Stops any ongoing movement.

#### How to send commands:
Use a HTTP PUT request to update `navigation.anchor.command`:

    curl -X PUT \
         -H "Content-Type: application/json" \
         -d '"drop"' \
         http://<your_signalk_server>/signalk/v1/api/vessels/self/navigation/anchor/command

Replace `"drop"` with other commands as needed (`"raise10"`, `"lower5"`, `"STOP"`).

---

### Signal K Path Updates
- **`navigation.anchor.rodeDeployed`**:  
  When set to `1`, it triggers a reset of the chain counter.
- **`navigation.anchor.command`**:  
  Receives commands like `"drop"`, `"raiseXX"`, `"lowerXX"`, and `"STOP"` to control the windlass.
- **`navigation.anchor.chainDirection`**:  
  Updated to reflect the chain's movement direction (`"up"` or `"down"`).
- **`navigation.anchor.rodeDeployed`**:  
  Reports the current chain length deployed (in meters).
