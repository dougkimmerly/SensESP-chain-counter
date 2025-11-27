#include "RetrievalManager.h"
#include "events.h"
#include <cmath>
#include <Arduino.h>

RetrievalManager::RetrievalManager(ChainController* chainCtrl)
  : chainController(chainCtrl),
    state_(RetrievalState::IDLE),
    running_(false),
    completed_(false),
    updateEvent_(nullptr) {

  // Initialize chain slack listener
  chainSlackListener_ = new sensesp::SKValueListener<float>(
    "navigation.anchor.chainSlack",  // Signal K path for chain slack
    500,                               // Update interval: 500ms
    "/chainslack/sk"                   // Config path
  );

  ESP_LOGI(__FILE__, "RetrievalManager: Chain slack listener initialized");
}

void RetrievalManager::start() {
  if (running_) {
    ESP_LOGI(__FILE__, "RetrievalManager: Already running");
    return;
  }

  // Reset state
  running_ = true;
  completed_ = false;
  state_ = RetrievalState::CHECKING_SLACK;

  ESP_LOGI(__FILE__, "RetrievalManager: Starting auto-retrieve sequence");

  // Schedule the periodic update (every 100ms)
  if (updateEvent_ != nullptr) {
    sensesp::event_loop()->remove(updateEvent_);
  }
  updateEvent_ = sensesp::event_loop()->onRepeat(
    100,  // 100ms interval
    std::bind(&RetrievalManager::updateRetrieval, this)
  );
}

void RetrievalManager::stop() {
  if (!running_) {
    return;
  }

  ESP_LOGI(__FILE__, "RetrievalManager: Stopping auto-retrieve");

  running_ = false;
  state_ = RetrievalState::IDLE;

  // Remove the update event
  if (updateEvent_ != nullptr) {
    sensesp::event_loop()->remove(updateEvent_);
    updateEvent_ = nullptr;
  }

  // Stop any active chain movement
  if (chainController->isActive()) {
    chainController->stop();
  }
}

bool RetrievalManager::isRunning() const {
  return running_;
}

bool RetrievalManager::isComplete() const {
  return completed_;
}

float RetrievalManager::getChainSlack() {
  float slack = chainSlackListener_->get();

  // Guard against NaN/Inf
  if (isnan(slack) || isinf(slack)) {
    ESP_LOGW(__FILE__, "RetrievalManager: Chain slack is NaN/Inf (%.2f). Using 0.0", slack);
    return 0.0;
  }

  return slack;
}

float RetrievalManager::getRodeDeployed() {
  float rode = chainController->getChainLength();

  // Guard against NaN/Inf
  if (isnan(rode) || isinf(rode)) {
    ESP_LOGW(__FILE__, "RetrievalManager: Rode deployed is NaN/Inf (%.2f). Using 0.0", rode);
    return 0.0;
  }

  return rode;
}

float RetrievalManager::getDepth() {
  float depth = chainController->getCurrentDepth();

  // Guard against NaN/Inf
  if (isnan(depth) || isinf(depth)) {
    ESP_LOGW(__FILE__, "RetrievalManager: Depth is NaN/Inf (%.2f). Using 0.0", depth);
    return 0.0;
  }

  return depth;
}

void RetrievalManager::transitionTo(RetrievalState nextState) {
  const char* stateNames[] = {"IDLE", "CHECKING_SLACK", "RAISING", "WAITING_FOR_SLACK", "COMPLETE"};

  ESP_LOGI(__FILE__, "RetrievalManager: State transition %s -> %s",
           stateNames[static_cast<int>(state_)],
           stateNames[static_cast<int>(nextState)]);

  state_ = nextState;
}

void RetrievalManager::updateRetrieval() {
  if (!running_) {
    return;
  }

  float rodeDeployed = getRodeDeployed();
  float depth = getDepth();
  float slack = getChainSlack();

  switch (state_) {
    case RetrievalState::IDLE:
      // Should not be here if running
      break;

    case RetrievalState::CHECKING_SLACK:
      // Check if retrieval is complete
      if (rodeDeployed <= COMPLETION_THRESHOLD_M) {
        ESP_LOGI(__FILE__, "RetrievalManager: Retrieval complete! Rode: %.2fm", rodeDeployed);
        transitionTo(RetrievalState::COMPLETE);
        completed_ = true;
        stop();
        break;
      }

      // Check if we're in final pull phase (rode < depth + 10m)
      if (rodeDeployed < depth + FINAL_PULL_THRESHOLD_M) {
        // Final pull - raise everything except the completion threshold
        float amountToRaise = rodeDeployed - COMPLETION_THRESHOLD_M;
        if (amountToRaise > 0.1) {  // Only raise if there's a significant amount
          ESP_LOGI(__FILE__, "RetrievalManager: Final pull phase - raising %.2fm (rode: %.2fm, depth: %.2fm)",
                   amountToRaise, rodeDeployed, depth);
          chainController->raiseAnchor(amountToRaise);
          transitionTo(RetrievalState::RAISING);
        }
      } else {
        // Slack-based retrieval phase
        if (slack >= SLACK_THRESHOLD_M) {
          ESP_LOGI(__FILE__, "RetrievalManager: Slack available (%.2fm), raising chain", slack);
          chainController->raiseAnchor(slack);
          transitionTo(RetrievalState::RAISING);
        } else {
          ESP_LOGD(__FILE__, "RetrievalManager: Insufficient slack (%.2fm < %.2fm), waiting",
                   slack, SLACK_THRESHOLD_M);
          transitionTo(RetrievalState::WAITING_FOR_SLACK);
        }
      }
      break;

    case RetrievalState::RAISING:
      // Wait for the chain controller to finish raising
      if (!chainController->isActive()) {
        ESP_LOGI(__FILE__, "RetrievalManager: Raising complete, rode now at %.2fm", rodeDeployed);
        // After raising completes, wait for slack to build up again
        transitionTo(RetrievalState::WAITING_FOR_SLACK);
      }
      break;

    case RetrievalState::WAITING_FOR_SLACK:
      // Wait for slack to reach threshold, or switch to final pull if needed
      if (rodeDeployed <= COMPLETION_THRESHOLD_M) {
        // Check completion first
        transitionTo(RetrievalState::CHECKING_SLACK);
      } else if (rodeDeployed < depth + FINAL_PULL_THRESHOLD_M) {
        // Switch to final pull phase
        transitionTo(RetrievalState::CHECKING_SLACK);
      } else if (slack >= SLACK_THRESHOLD_M) {
        // Enough slack available, go back to checking
        ESP_LOGI(__FILE__, "RetrievalManager: Slack threshold met (%.2fm), checking for next raise", slack);
        transitionTo(RetrievalState::CHECKING_SLACK);
      }
      // Otherwise, keep waiting
      break;

    case RetrievalState::COMPLETE:
      // Done
      break;
  }
}