#ifndef DEPLOYMENTMANAGER_H
#define DEPLOYMENTMANAGER_H

#include "ChainController.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "events.h"
#include <functional>
#include <cmath>

class DeploymentManager {
public:
  // Constructor
  DeploymentManager(ChainController* chainCtrl);
  
  // Public methods to control the deployment sequence
  void strBoatSpeed();    // Start the staged deployment process (begin speed measurement)
  void start();           // Begin/restart deployment sequence
  void stop();            // Cancel/deactivate/deployment stop
  void reset();           // Reset internal state for new deployment
  bool isAutoAnchorValid(); // Check if auto anchor deployment is valid

  // float getCurrentSpeed(); // Get smoothed boat speed (m/s)

private:
  // References to core objects
  ChainController* chainController;
  sensesp::SKValueListener<float>* windSpeedListener_;  // Apparent wind speed listener

  // Stage enumeration for finite-state machine
  enum Stage {
    IDLE,
    DROP,
    WAIT_TIGHT,
    HOLD_DROP,
    DEPLOY_30,
    WAIT_30,
    HOLD_30,
    DEPLOY_75,
    WAIT_75,
    HOLD_75,
    DEPLOY_100,
    COMPLETE
  };
  Stage currentStage = IDLE;

  // Flags for flow control
  bool isRunning = false;     // Is deployment active?
  bool dropInitiated = false; // Has the initial drop been triggered?

  // Timing and deployment parameters
  float targetDropDepth = 0.0;        // Target depth for initial drop
  float totalChainLength = 0.0;       // Total chain length to deploy
  unsigned long stageStartTime = 0;   // For timing hold periods

  // Speed measurement members
  float ewmaSpeed = 0.0;
  static constexpr float alpha = 0.3; // EWMA smoothing factor
  unsigned long lastTime = 0;
  float lastDistance = 0;
  bool measuring = false;
  float ewmaDistance = 0.0; // Smoothed distance
  static constexpr float distance_alpha = 0.1; 
  float lastRawDistanceForEWMA = 0.0;
  float _lastDistanceAtDeploymentCommand = 0.0;
  float _accumulatedDeployDemand = 0.0;

  // distance variables
  float anchorDepth;                  // Depth when deployment starts
  float targetChainLength;            // Total chain to deploy (duplicate of totalChainLength - consider consolidating)
  float targetDistanceInit;           // distance for initial drop (probably just targetDropDepth + slack)
  float targetDistance30;             // Horizontal distance for 30% chain
  float targetDistance75;             // Horizontal distance for 75%
  float chain30;                      // Actual chain length at 30% deployment
  float chain75;                      // Actual chain length at 75% deployment
  bool _commandIssuedInCurrentDeployStage = false;

  const float MIN_DEPLOY_THRESHOLD_M = 1.0;
  const unsigned long DECISION_WINDOW_MS = 1000;

  // Event handle for periodic update
  reactesp::Event* updateEvent = nullptr;
  reactesp::Event* speedUpdateEvent = nullptr;
  reactesp::Event* deployPulseEvent = nullptr;

  // Private helper methods
  void startSpeedMeasurement();             // Initiate background speed calculation
  void stopSpeedMeasurement();              // Stop speed background task
  float getCurrentSpeed();                  // Retrieve smoothed speed
  float computeTargetHorizontalDistance(float chainLength, float anchorDepth);
  float computeCatenaryReductionFactor(float chainLength, float anchorDepth, float horizontalForce);
  float estimateHorizontalForce();  // Estimate force from wind/current
  float currentStageTargetLength = 0.0;
  void transitionTo(Stage nextStage);
  void startDeployPulse(float stageTargetChainLength);

  // Chain physical constants (10mm galvanized)
  static constexpr float CHAIN_WEIGHT_PER_METER_KG = 2.2;  // kg/m in water (adjusted for buoyancy)
  static constexpr float GRAVITY = 9.81;                    // m/s²

  // Boat physical constants (adjust these for your boat)
  static constexpr float BOAT_WINDAGE_AREA_M2 = 15.0;      // m² - effective windage area (typical 10m sailboat)
  static constexpr float AIR_DENSITY = 1.225;               // kg/m³ at sea level
  static constexpr float DRAG_COEFFICIENT = 1.2;           // typical for boat hull + rigging    
  
  // Stage transition handler
  void onStageAdvance();                    // Advance to next stage
  void checkConditions();                   // Checks for stage triggers
  void updateDeployment();                  // Main deployment logic called per tick
};

#endif

