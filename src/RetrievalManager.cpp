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

  // Initialize timing (allow immediate first raise)
  lastRaiseTime_ = 0;

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

      // Wait if ChainController is busy
      if (chainController->isActivelyControlling()) {
        // ChainController is actively raising, stay in CHECKING_SLACK and wait
        ESP_LOGD(__FILE__, "RetrievalManager: ChainController busy, waiting...");
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
        // Slack-based retrieval phase with hysteresis and minimum raise
        unsigned long now = millis();
        unsigned long timeSinceLastRaise = now - lastRaiseTime_;

        // Only consider raising if enough time has passed since last raise
        if (timeSinceLastRaise < COOLDOWN_AFTER_RAISE_MS) {
          ESP_LOGD(__FILE__, "RetrievalManager: In cooldown period (%.1fs remaining)",
                   (COOLDOWN_AFTER_RAISE_MS - timeSinceLastRaise) / 1000.0);
          transitionTo(RetrievalState::WAITING_FOR_SLACK);
        }
        // Check if we have enough slack to justify a raise
        else if (slack >= MIN_SLACK_TO_START_M) {
          // Calculate how much we should raise
          float raiseAmount = slack;

          // Only raise if amount is significant (prevents tiny raises)
          if (raiseAmount >= MIN_RAISE_AMOUNT_M) {
            ESP_LOGD(__FILE__, "RetrievalManager: Slack available (%.2fm), raising %.2fm",
                     slack, raiseAmount);
            chainController->raiseAnchor(raiseAmount);
            lastRaiseTime_ = now;  // Record raise time
            transitionTo(RetrievalState::RAISING);
          } else {
            ESP_LOGD(__FILE__, "RetrievalManager: Slack (%.2fm) below minimum raise threshold (%.2fm), waiting",
                     slack, MIN_RAISE_AMOUNT_M);
            transitionTo(RetrievalState::WAITING_FOR_SLACK);
          }
        } else {
          ESP_LOGD(__FILE__, "RetrievalManager: Insufficient slack (%.2fm < %.2fm), waiting",
                   slack, MIN_SLACK_TO_START_M);
          transitionTo(RetrievalState::WAITING_FOR_SLACK);
        }
      }
      break;

    case RetrievalState::RAISING:
      // Check if slack has gone negative during raising
      if (slack < 0 && chainController->isActive()) {
        ESP_LOGW(__FILE__, "RetrievalManager: Slack went negative (%.2fm) during raise - stopping chain", slack);
        chainController->stop();
        transitionTo(RetrievalState::WAITING_FOR_SLACK);
      }
      // Wait for the chain controller to finish raising
      else if (!chainController->isActive()) {
        ESP_LOGD(__FILE__, "RetrievalManager: Raising complete, rode now at %.2fm", rodeDeployed);
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
      } else if (slack >= MIN_SLACK_TO_START_M) {
        // Enough slack available, go back to checking
        ESP_LOGD(__FILE__, "RetrievalManager: Slack threshold met (%.2fm), checking for next raise", slack);
        transitionTo(RetrievalState::CHECKING_SLACK);
      }
      // Otherwise, keep waiting
      break;

    case RetrievalState::COMPLETE:
      // Done
      break;
  }
}