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
  void start();           // Begin/restart deployment sequence
  void stop();            // Cancel/deactivate/deployment stop
  void reset();           // Reset internal state for new deployment
  bool isAutoAnchorValid(); // Check if auto anchor deployment is valid

private:
  // References to core objects
  ChainController* chainController;

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

  // Slack-based control members
  float lastSlack_ = 0.0;              // Previous slack value for rate calculation
  unsigned long lastSlackTime_ = 0;    // Timestamp of last slack measurement
  float slackRate_ = 0.0;              // Rate of slack change (m/s), negative = decreasing

  // distance variables
  float anchorDepth;                  // Depth when deployment starts
  float targetChainLength;            // Total chain to deploy (duplicate of totalChainLength - consider consolidating)
  float targetDistanceInit;           // distance for initial drop (probably just targetDropDepth + slack)
  float targetDistance30;             // Horizontal distance for 30% chain
  float targetDistance75;             // Horizontal distance for 75%
  float chain30;                      // Actual chain length at 30% deployment
  float chain75;                      // Actual chain length at 75% deployment
  bool _commandIssuedInCurrentDeployStage = false;

  // Slack-based deployment constants
  static constexpr float TARGET_SLACK_RATIO = 0.10;                    // Target slack: 10% of deployed chain
  static constexpr float AGGRESSIVE_SLACK_RATE_THRESHOLD = -0.05;     // -5cm/s = deploy aggressively
  static constexpr float NORMAL_DEPLOY_INCREMENT = 0.5;               // 0.5m increments normally
  static constexpr float AGGRESSIVE_DEPLOY_INCREMENT = 1.0;           // 1.0m when slack dropping fast
  const unsigned long DECISION_WINDOW_MS = 500; // Check twice as often for faster response

  // Event handle for periodic update
  reactesp::Event* updateEvent = nullptr;
  reactesp::Event* deployPulseEvent = nullptr;

  // Private helper methods
  float computeTargetHorizontalDistance(float chainLength, float anchorDepth);
  float currentStageTargetLength = 0.0;
  void transitionTo(Stage nextStage);
  void startDeployPulse(float stageTargetChainLength);

  // Stage transition handler
  void onStageAdvance();                    // Advance to next stage
  void checkConditions();                   // Checks for stage triggers
  void updateDeployment();                  // Main deployment logic called per tick
};

#endif

