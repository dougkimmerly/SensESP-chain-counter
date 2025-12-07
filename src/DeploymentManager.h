#ifndef DEPLOYMENTMANAGER_H
#define DEPLOYMENTMANAGER_H

#include "ChainController.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/observable.h"
#include "events.h"
#include <functional>
#include <cmath>

class DeploymentManager {
public:
  // Constructor
  DeploymentManager(ChainController* chainCtrl);
  
  // Scope ratio constants
  static constexpr float MIN_SCOPE_RATIO = 3.0;
  static constexpr float MAX_SCOPE_RATIO = 10.0;
  static constexpr float DEFAULT_SCOPE_RATIO = 5.0;

  // Public methods to control the deployment sequence
  void start(float scopeRatio = DEFAULT_SCOPE_RATIO);  // Begin deployment with optional scope ratio
  void stop();            // Cancel/deactivate/deployment stop
  void reset();           // Reset internal state for new deployment
  bool isAutoAnchorValid(); // Check if auto anchor deployment is valid

  // Completion callback - called when deployment finishes (success or stopped)
  void setCompletionCallback(std::function<void()> callback) { completionCallback_ = callback; }

private:
  // References to core objects
  ChainController* chainController;

  // Stage enumeration for finite-state machine
  enum Stage {
    IDLE,
    DROP,
    WAIT_TIGHT,
    HOLD_DROP,
    DEPLOY_FIRST,
    WAIT_FIRST,
    HOLD_FIRST,
    DEPLOY_SECOND,
    WAIT_SECOND,
    HOLD_SECOND,
    DEPLOY_100,
    COMPLETE
  };
  Stage currentStage = IDLE;

  // Flags for flow control
  bool isRunning = false;     // Is deployment active?
  bool dropInitiated = false; // Has the initial drop been triggered?

  // Scope ratio for chain length calculation
  float scopeRatio_ = DEFAULT_SCOPE_RATIO;

  // Timing and deployment parameters
  float targetDropDepth = 0.0;        // Target depth for initial drop
  float totalChainLength = 0.0;       // Total chain length to deploy
  unsigned long stageStartTime = 0;   // For timing hold periods

  // distance variables
  float anchorDepth;                  // Depth when deployment starts
  float targetChainLength;            // Total chain to deploy (duplicate of totalChainLength - consider consolidating)
  float targetDistanceInit;           // distance for initial drop (probably just targetDropDepth + slack)
  float targetDistance30;             // Horizontal distance for 30% chain
  float targetDistance75;             // Horizontal distance for 75%
  float chain30;                      // Actual chain length at 30% deployment
  float chain75;                      // Actual chain length at 75% deployment
  bool _commandIssuedInCurrentDeployStage = false;

  // Continuous deployment monitoring with hysteresis
  static constexpr float MAX_SLACK_RATIO = 1.2;                       // Pause deployment when slack exceeds 120% of depth
  static constexpr float RESUME_SLACK_RATIO = 0.6;                    // Resume deployment when slack drops below 60% of depth
  static constexpr unsigned long MONITOR_INTERVAL_MS = 500;           // Check conditions every 500ms

  // Event handle for periodic update
  reactesp::Event* updateEvent = nullptr;
  reactesp::Event* deployPulseEvent = nullptr;

  // Signal K stage publishing
  sensesp::ObservableValue<String>* autoStageObservable_;

  // Completion callback
  std::function<void()> completionCallback_ = nullptr;

  // Private helper methods
  float computeTargetHorizontalDistance(float chainLength, float anchorDepth);
  float currentStageTargetLength = 0.0;
  void transitionTo(Stage nextStage);
  void startContinuousDeployment(float stageTargetChainLength);
  void monitorDeployment(float stageTargetChainLength);

  // Stage publishing helpers
  String getStageDisplayName(Stage stage) const;
  void publishStage(Stage stage);

  // Stage transition handler
  void onStageAdvance();                    // Advance to next stage
  void checkConditions();                   // Checks for stage triggers
  void updateDeployment();                  // Main deployment logic called per tick
};

#endif

