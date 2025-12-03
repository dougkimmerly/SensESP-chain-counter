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

  // Completion callback - called when retrieval finishes (success or stopped)
  void setCompletionCallback(std::function<void()> callback) { completionCallback_ = callback; }

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

  // Retrieval parameters with depth-based hysteresis
  static constexpr float STOP_SLACK_M = 0.0;             // Stop raising immediately when slack goes negative
  static constexpr float RESUME_SLACK_RATIO = 0.3;       // Resume raising when slack > 30% of depth
  static constexpr float MIN_RAISE_AMOUNT_M = 1.0;       // Only raise if we can get at least 1.0m
  static constexpr float COOLDOWN_AFTER_RAISE_MS = 3000; // Wait 3s after each raise before next one
  static constexpr float COMPLETION_THRESHOLD_M = 2.0;   // Rode length at which retrieval is complete
  static constexpr float FINAL_PULL_THRESHOLD_M = 10.0;  // When rode < depth + 10m, switch to continuous pull

  // Event handle for periodic update
  reactesp::Event* updateEvent_ = nullptr;

  // Timing tracking
  unsigned long lastRaiseTime_ = 0;  // Timestamp of last raise command

  // Completion callback
  std::function<void()> completionCallback_ = nullptr;

  // Private helper methods
  void updateRetrieval();              // Main retrieval logic called periodically
  float getChainSlack();              // Get current chain slack value
  float getRodeDeployed();            // Get current rode deployed
  float getDepth();                   // Get current depth
  void transitionTo(RetrievalState nextState);  // State transition handler
};

#endif