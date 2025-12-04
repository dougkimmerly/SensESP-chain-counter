# SensESP Chain Counter - Claude Context Guide

Marine anchor chain deployment/retrieval system on ESP32 with Signal K integration.

## Core Files
- [ChainController.cpp](src/ChainController.cpp) - Motor control, position tracking, catenary physics
- [DeploymentManager.cpp](src/DeploymentManager.cpp) - Automated deployment FSM
- [RetrievalManager.cpp](src/RetrievalManager.cpp) - Automated retrieval FSM

## Documentation (read as needed)
- [ARCHITECTURE_MAP.md](docs/ARCHITECTURE_MAP.md) - Visual diagrams, state flows, data flow
- [CODEBASE_OVERVIEW.md](docs/CODEBASE_OVERVIEW.md) - Architecture, file structure
- [mcp_chaincontroller.md](docs/mcp_chaincontroller.md) - ChainController API reference
- [SENSESP_GUIDE.md](docs/SENSESP_GUIDE.md) - SensESP framework patterns
- [SIGNALK_GUIDE.md](docs/SIGNALK_GUIDE.md) - Signal K paths and units

## State Machines
- **Deployment**: DROP → WAIT_TIGHT → HOLD_DROP → DEPLOY_30 → WAIT_30 → HOLD_30 → DEPLOY_75 → WAIT_75 → HOLD_75 → DEPLOY_100 → COMPLETE
- **Retrieval**: CHECKING_SLACK → RAISING → WAITING_FOR_SLACK → COMPLETE

## Critical Physics
- **Bow height**: 2m from bow to water surface
- **Slack calculation**: `slack = chain_length - sqrt(distance² + (depth + bow_height)²)` (excess chain beyond straight-line to anchor)
- **Final pull phase**: When rode < depth + 3m, skip slack-based control
- **Negative slack**: Indicates anchor drag (boat further than chain can reach)

## Key Constants
| Parameter | Value | Location |
|-----------|-------|----------|
| Bow height | 2.0m | ChainController |
| Final pull threshold | 3.0m | RetrievalManager.h |
| Retrieval cooldown | 3000ms | RetrievalManager.h |
| Pause slack | 0.2m | RetrievalManager.h |
| Resume slack ratio | 30% of depth | RetrievalManager.h |
| Min raise amount | 1.0m | RetrievalManager.h |
| Deploy pause slack | 120% of depth | DeploymentManager.h |
| Deploy resume slack | 60% of depth | DeploymentManager.h |

## Signal K Paths
**Published:** `navigation.anchor.rodeDeployed`, `navigation.anchor.chainSlack`, `navigation.anchor.chainDirection`, `navigation.anchor.autoStage`
**Subscribed:** `environment.depth.belowSurface`, `navigation.anchor.distanceFromBow`, `environment.tide.heightNow`, `environment.tide.heightHigh`
