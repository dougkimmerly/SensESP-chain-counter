#include "RetrievalManager.h"
#include "events.h"
#include <cmath>
#include <Arduino.h>

RetrievalManager::RetrievalManager(ChainController* chainCtrl)
  : chainController(chainCtrl),
    state_(RetrievalState::IDLE),
    running_(false),
    completed_(false),
    updateEvent_(nullptr),
    chainSlackListener_(nullptr) {

  // Note: We read slack directly from ChainController's observable
  // to avoid Signal K round-trip delay

  ESP_LOGI(__FILE__, "RetrievalManager: Initialized (reading slack from ChainController)");
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

  // Notify completion callback if set
  if (completionCallback_) {
    completionCallback_();
  }
}

bool RetrievalManager::isRunning() const {
  return running_;
}

bool RetrievalManager::isComplete() const {
  return completed_;
}

float RetrievalManager::getChainSlack() {
  // Read directly from ChainController's observable for immediate value
  // (avoids Signal K round-trip delay)
  float slack = chainController->getHorizontalSlackObservable()->get();

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
        // Final pull phase - still need to respect cooldown to prevent relay cycling
        unsigned long now = millis();
        unsigned long timeSinceLastRaise = now - lastRaiseTime_;

        if (timeSinceLastRaise < COOLDOWN_AFTER_RAISE_MS) {
          // Still in cooldown, wait
          ESP_LOGD(__FILE__, "RetrievalManager: Final pull - in cooldown (%.1fs remaining)",
                   (COOLDOWN_AFTER_RAISE_MS - timeSinceLastRaise) / 1000.0);
          transitionTo(RetrievalState::WAITING_FOR_SLACK);
        } else {
          // Cooldown expired, raise everything except the completion threshold
          float amountToRaise = rodeDeployed - COMPLETION_THRESHOLD_M;
          if (amountToRaise > 0.1) {  // Only raise if there's a significant amount
            ESP_LOGI(__FILE__, "RetrievalManager: Final pull phase - raising %.2fm (rode: %.2fm, depth: %.2fm)",
                     amountToRaise, rodeDeployed, depth);
            chainController->raiseAnchor(amountToRaise);
            lastRaiseTime_ = now;  // Record raise time
            transitionTo(RetrievalState::RAISING);
          }
        }
      } else {
        // Slack-based retrieval phase with depth-based hysteresis
        unsigned long now = millis();
        unsigned long timeSinceLastRaise = now - lastRaiseTime_;
        float resumeThreshold = depth * RESUME_SLACK_RATIO;

        // Only consider raising if enough time has passed since last raise
        if (timeSinceLastRaise < COOLDOWN_AFTER_RAISE_MS) {
          ESP_LOGD(__FILE__, "RetrievalManager: In cooldown period (%.1fs remaining)",
                   (COOLDOWN_AFTER_RAISE_MS - timeSinceLastRaise) / 1000.0);
          transitionTo(RetrievalState::WAITING_FOR_SLACK);
        }
        // Check if we have enough slack to justify a raise (use depth-based RESUME threshold)
        else if (slack >= resumeThreshold) {
          // Calculate how much we should raise
          float raiseAmount = slack;

          // Only raise if amount is significant (prevents tiny raises)
          if (raiseAmount >= MIN_RAISE_AMOUNT_M) {
            ESP_LOGD(__FILE__, "RetrievalManager: Slack available (%.2fm >= %.2fm), raising %.2fm",
                     slack, resumeThreshold, raiseAmount);
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
                   slack, resumeThreshold);
          transitionTo(RetrievalState::WAITING_FOR_SLACK);
        }
      }
      break;

    case RetrievalState::RAISING: {
      // Skip slack-based pausing in final pull phase or when chain is vertical.
      // In final pull (rode < depth + 10m), the catenary model breaks down and slack
      // readings become unreliable (often negative). Just let it raise continuously.
      bool inFinalPull = (rodeDeployed < depth + FINAL_PULL_THRESHOLD_M);

      // Pause when slack drops below threshold (chain is getting tight)
      // But skip this check in final pull - just let it raise to completion
      if (!inFinalPull && slack < PAUSE_SLACK_M && chainController->isActive()) {
        ESP_LOGI(__FILE__, "RetrievalManager: Slack low (%.2fm < %.2fm) - pausing raise", slack, PAUSE_SLACK_M);
        chainController->stop();
        lastRaiseTime_ = millis();  // Start cooldown after pausing
        transitionTo(RetrievalState::WAITING_FOR_SLACK);
      }
      // Wait for the chain controller to finish raising
      else if (!chainController->isActive()) {
        ESP_LOGD(__FILE__, "RetrievalManager: Raising complete, rode now at %.2fm", rodeDeployed);
        // After raising completes, wait for slack to build up again
        transitionTo(RetrievalState::WAITING_FOR_SLACK);
      }
      break;
    }

    case RetrievalState::WAITING_FOR_SLACK: {
      // Wait for slack to reach threshold, or switch to final pull if needed
      // Also respect cooldown period to prevent rapid relay cycling
      unsigned long now = millis();
      unsigned long timeSinceLastRaise = now - lastRaiseTime_;
      bool cooldownExpired = (timeSinceLastRaise >= COOLDOWN_AFTER_RAISE_MS);
      float resumeThreshold = depth * RESUME_SLACK_RATIO;

      if (rodeDeployed <= COMPLETION_THRESHOLD_M) {
        // Check completion first (no cooldown needed)
        transitionTo(RetrievalState::CHECKING_SLACK);
      } else if (rodeDeployed < depth + FINAL_PULL_THRESHOLD_M && cooldownExpired) {
        // In final pull phase AND cooldown expired - go check for next raise
        transitionTo(RetrievalState::CHECKING_SLACK);
      } else if (slack >= resumeThreshold && cooldownExpired) {
        // Enough slack available AND cooldown expired, go back to checking
        ESP_LOGD(__FILE__, "RetrievalManager: Slack threshold met (%.2fm >= %.2fm), checking for next raise",
                 slack, resumeThreshold);
        transitionTo(RetrievalState::CHECKING_SLACK);
      }
      // Otherwise, keep waiting (either no slack or still in cooldown)
      break;
    }

    case RetrievalState::COMPLETE:
      // Done
      break;
  }
}