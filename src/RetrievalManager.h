#ifndef RETRIEVALMANAGER_H
#define RETRIEVALMANAGER_H

#include "ChainController.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "events.h"
#include <functional>
#include <cmath>

class RetrievalManager {
public:
  // Constructor
  RetrievalManager(ChainController* chainCtrl);

  // Public methods to control the retrieval sequence
  void start();           // Begin auto-retrieve sequence
  void stop();            // Cancel/stop retrieval
  bool isRunning() const; // Check if retrieval is active
  bool isComplete() const; // Check if retrieval is finished

private:
  // References to core objects
  ChainController* chainController;

  // SKValueListeners for monitoring chain slack
  sensesp::SKValueListener<float>* chainSlackListener_;  // Monitor navigation.anchor.chainSlack

  // State enumeration for finite-state machine
  enum class RetrievalState {
    IDLE,              // Not running
    CHECKING_SLACK,    // Evaluate current rode and slack
    RAISING,           // Chain is actively being raised
    WAITING_FOR_SLACK, // Waiting for boat to move and create slack
    COMPLETE           // Retrieval finished (rode <= 2m)
  };
  RetrievalState state_ = RetrievalState::IDLE;

  // Flags for flow control
  bool running_ = false;     // Is retrieval active?
  bool completed_ = false;   // Has retrieval finished?

  // Retrieval parameters
  static constexpr float SLACK_THRESHOLD_M = 2.0;      // Minimum slack needed to raise (meters)
  static constexpr float COMPLETION_THRESHOLD_M = 2.0; // Rode length at which retrieval is complete
  static constexpr float FINAL_PULL_THRESHOLD_M = 10.0; // When rode < depth + 10m, switch to continuous pull

  // Event handle for periodic update
  reactesp::Event* updateEvent_ = nullptr;

  // Private helper methods
  void updateRetrieval();              // Main retrieval logic called periodically
  float getChainSlack();              // Get current chain slack value
  float getRodeDeployed();            // Get current rode deployed
  float getDepth();                   // Get current depth
  void transitionTo(RetrievalState nextState);  // State transition handler
};

#endif