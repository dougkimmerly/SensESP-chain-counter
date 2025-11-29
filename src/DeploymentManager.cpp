#include "DeploymentManager.h"
#include "events.h"
#include <cmath>
#include <Arduino.h>

DeploymentManager::DeploymentManager(ChainController* chainCtrl)
  : chainController(chainCtrl),
    currentStage(IDLE),
    isRunning(false),
    dropInitiated(false),
    updateEvent(nullptr) {
  ESP_LOGI(__FILE__, "DeploymentManager initialized");
}

void DeploymentManager::start() {
  if (isRunning) return; // already active
  if (!isAutoAnchorValid()) return;  // invalid conditions

  isRunning = true;

  float currentDepth = chainController->getDepthListener()->get();
  float currentDistance = chainController->getDistanceListener()->get();

  targetDropDepth = currentDepth + 4.0; // initial slack
  anchorDepth = targetDropDepth - 2.0;          // anchor depth
  totalChainLength = 5 * (targetDropDepth - 2);
  chain30 = 0.40 * totalChainLength;
  chain75 = 0.80 * totalChainLength;

  // Calculate catenary-adjusted distances using ChainController's physics model
  // ChainController now automatically accounts for catenary effects and wind force
  targetDistanceInit = computeTargetHorizontalDistance(targetDropDepth, anchorDepth);
  targetDistance30 = computeTargetHorizontalDistance(chain30, anchorDepth);
  targetDistance75 = computeTargetHorizontalDistance(chain75, anchorDepth);

  ESP_LOGI(__FILE__, "DeploymentManager: Target distances - Init: %.2f, 30%%: %.2f, 75%%: %.2f",
           targetDistanceInit, targetDistance30, targetDistance75);  

    if (totalChainLength < 10.0) { 
      ESP_LOGW(__FILE__, "DeploymentManager: Calculated totalChainLength (%.2f m) is too small. Capping at 10.0 m.", totalChainLength);
      totalChainLength = 10.0; 
  }
  if (anchorDepth < 0.0) { 
      ESP_LOGW(__FILE__, "DeploymentManager: Calculated anchorDepth (%.2f m) was negative, setting to 0.0 m.", anchorDepth);
      anchorDepth = 0.0;
  }
  
  // Reset stage and flags
  currentStage = DROP;
  dropInitiated = false;
  currentStageTargetLength = 0.0;

  // Initialize slack tracking
  lastSlack_ = chainController->getHorizontalSlackObservable()->get();
  lastSlackTime_ = millis();
  slackRate_ = 0.0;

  ESP_LOGI(__FILE__, "DeploymentManager: Starting autoDrop. Initial depth: %.2f, Target Drop Depth: %.2f, Total Chain: %.2f", (targetDropDepth-2), targetDropDepth, totalChainLength);

  // Schedule the tick/update
  if (updateEvent != nullptr) {
    sensesp::event_loop()->remove(updateEvent);
  }
  updateEvent = sensesp::event_loop()->onRepeat(1, std::bind(&DeploymentManager::updateDeployment, this));
}

void DeploymentManager::stop() {
  if (!isRunning) return;
  isRunning = false;
  if (updateEvent != nullptr) {
    sensesp::event_loop()->remove(updateEvent);
    updateEvent = nullptr;
  }
  if (deployPulseEvent != nullptr) {
    sensesp::event_loop()->remove(deployPulseEvent);
    deployPulseEvent = nullptr;
  }
  currentStage = IDLE;
}

void DeploymentManager::reset() {
  stop();
  // Reset internal states if needed
  // (e.g., reset flags, timers)
}

float DeploymentManager::computeTargetHorizontalDistance(float chainLength, float anchorDepth) {
  // Delegate to ChainController which now has the catenary-aware calculation
  return chainController->computeTargetHorizontalDistance(chainLength, anchorDepth);
}

void DeploymentManager::startDeployPulse(float stageTargetChainLength) {
    // If a pulse event is already scheduled, it means this call is from updateDeployment()
    // or a previous pulse setting up the next. Remove any existing to ensure a fresh schedule.
    if (deployPulseEvent != nullptr) {
        sensesp::event_loop()->remove(deployPulseEvent);
        deployPulseEvent = nullptr;
    }

    // Schedule the actual pulse logic as a one-shot event
    // Using a delay of 0 for immediate execution if no prior pulse was active,
    // or the calculated delay for subsequent pulses.
    deployPulseEvent = sensesp::event_loop()->onDelay(0, [this, stageTargetChainLength]() {
        // First, check if the DeploymentManager is still running and in an active DEPLOY stage
        if (!isRunning || (currentStage != DEPLOY_30 && currentStage != DEPLOY_75 && currentStage != DEPLOY_100)) {
            ESP_LOGD(__FILE__, "Deploy Pulse: DM not running or not in DEPLOY stage. Stopping pulses.");
            deployPulseEvent = nullptr; // Ensure event is cleared
            return; // Exit this lambda, no further pulse scheduled
        }

        // Validate that the captured target still matches the current stage target
        float current_stage_target = 0.0;
        if (currentStage == DEPLOY_30) {
            current_stage_target = chain30;
        } else if (currentStage == DEPLOY_75) {
            current_stage_target = chain75;
        } else if (currentStage == DEPLOY_100) {
            current_stage_target = totalChainLength;
        }

        // If the captured target doesn't match the current stage, the stage transitioned
        if (fabs(stageTargetChainLength - current_stage_target) > 0.01) {
            ESP_LOGW(__FILE__, "Deploy Pulse: Stage transition detected (target mismatch: %.2f vs %.2f). Stopping stale pulse.", stageTargetChainLength, current_stage_target);
            deployPulseEvent = nullptr;
            return;
        }

        // Wait if ChainController is busy
        if (this->chainController->isActivelyControlling()) {
            ESP_LOGD(__FILE__, "Deploy Pulse: ChainController busy, rescheduling...");
            deployPulseEvent = sensesp::event_loop()->onDelay(DECISION_WINDOW_MS, [this, stageTargetChainLength]() {
                startDeployPulse(stageTargetChainLength);
            });
            return;
        }

        // --- Get current state ---
        float current_chain = this->chainController->getChainLength();
        float current_slack = this->chainController->getHorizontalSlackObservable()->get();
        unsigned long now = millis();

        // --- Calculate slack rate-of-change ---
        float dt = (now - lastSlackTime_) / 1000.0;  // seconds
        if (dt > 0.1) {  // Update if at least 100ms elapsed
            slackRate_ = (current_slack - lastSlack_) / dt;  // m/s (negative = slack decreasing)
            lastSlack_ = current_slack;
            lastSlackTime_ = now;
        }

        // --- Calculate target slack: 10% of deployed chain ---
        float target_slack = current_chain * TARGET_SLACK_RATIO;
        float slack_deficit = target_slack - current_slack;  // Positive = need more slack (deploy chain)

        // --- Determine if we should deploy ---
        bool shouldDeploy = false;
        float deploy_amount = 0.0;

        if (slack_deficit > 0.2) {  // Need at least 20cm more slack to deploy
            shouldDeploy = true;

            // Choose deployment amount based on slack rate
            if (slackRate_ < AGGRESSIVE_SLACK_RATE_THRESHOLD) {
                // Slack dropping fast - deploy aggressively
                deploy_amount = AGGRESSIVE_DEPLOY_INCREMENT;
                ESP_LOGD(__FILE__, "Deploy Pulse: Slack dropping fast (%.3f m/s). Deploying %.2f m aggressively.",
                         slackRate_, deploy_amount);
            } else {
                // Normal deployment
                deploy_amount = NORMAL_DEPLOY_INCREMENT;
                ESP_LOGD(__FILE__, "Deploy Pulse: Slack deficit %.2f m, deploying %.2f m normally.",
                         slack_deficit, deploy_amount);
            }

            // Don't exceed stage target
            if (current_chain + deploy_amount > stageTargetChainLength) {
                deploy_amount = stageTargetChainLength - current_chain;
                if (deploy_amount < 0.1) shouldDeploy = false;  // Too close to target
            }
        }

        // --- Check maximum slack limit (safety brake) ---
        float current_depth = this->chainController->getCurrentDepth();
        float max_acceptable_slack = current_depth * 0.5;  // Still use 50% of depth as safety limit

        if (current_slack > max_acceptable_slack) {
            shouldDeploy = false;
            ESP_LOGD(__FILE__, "Deploy Pulse: Excessive slack (%.2f m > %.2f m). Pausing deployment.",
                     current_slack, max_acceptable_slack);
        }

        // --- Execute deployment or wait ---
        unsigned long next_pulse_delay_ms = DECISION_WINDOW_MS;

        if (shouldDeploy) {
            ESP_LOGD(__FILE__, "Deploy Pulse: Deploying %.2f m (chain: %.2f, slack: %.2f, target_slack: %.2f, rate: %.3f m/s)",
                     deploy_amount, current_chain, current_slack, target_slack, slackRate_);

            this->chainController->lowerAnchor(deploy_amount);

            // Wait for deployment to complete
            next_pulse_delay_ms = (unsigned long)(deploy_amount * this->chainController->getDownSpeed()) + 200;
            if (next_pulse_delay_ms < DECISION_WINDOW_MS) {
                next_pulse_delay_ms = DECISION_WINDOW_MS;
            }
        } else {
            ESP_LOGD(__FILE__, "Deploy Pulse: No deployment needed. Chain: %.2f, Slack: %.2f, Target: %.2f",
                     current_chain, current_slack, target_slack);
        }
        deployPulseEvent = sensesp::event_loop()->onDelay(next_pulse_delay_ms, [this, stageTargetChainLength]() {
            startDeployPulse(stageTargetChainLength);
        });
    });
}

void DeploymentManager::updateDeployment() {
  if (!isRunning) {
    ESP_LOGD(__FILE__, "DeploymentManager: updateDeployment called but not running. Current stage: %d", (int)currentStage);
    return; // exit if stopped
  }

  float currentChainLength = chainController->getChainLength();
  float currentDistance = chainController->getDistanceListener()->get(); 

  switch (currentStage) {
    case IDLE:
      // Nothing to do
      break;

    case DROP:
      // PART 1: Issue the initial lowerAnchor command if it hasn't been issued yet for THIS DROP stage
      if (!_commandIssuedInCurrentDeployStage) {
        // We only command if the current chain length is less than the target drop depth
        if (currentChainLength < targetDropDepth) {
          float amount_to_lower = targetDropDepth - currentChainLength;
          if (amount_to_lower > 0.01) { // Command only if a significant amount remains (e.g., > 1 cm)
            ESP_LOGI(__FILE__, "DROP: Initiating initial lowerAnchor by %.2f m to reach %.2f m", amount_to_lower, targetDropDepth);
            chainController->lowerAnchor(amount_to_lower);
            currentStageTargetLength = targetDropDepth; // Set the absolute target for this stage
            _commandIssuedInCurrentDeployStage = true; // Mark command as issued
            dropInitiated = true; // Mark initial drop as initiated
            stageStartTime = millis(); // Mark start time for monitoring
            // Do NOT transition yet. We just started the movement.
          } else {
            // Already effectively at target or past it (within tolerance), so just transition
            ESP_LOGI(__FILE__, "DROP: Already at or past %.2f m (current %.2f). Transitioning to WAIT_TIGHT.", targetDropDepth, currentChainLength);
            transitionTo(WAIT_TIGHT); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_TIGHT) stage
          }
        } else {
            // We started this DROP stage already at or past its target, so transition
            ESP_LOGI(__FILE__, "DROP: Started at or past %.2f m (current %.2f). Transitioning to WAIT_TIGHT.", targetDropDepth, currentChainLength);
            transitionTo(WAIT_TIGHT); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_TIGHT) stage
        }
      }
      // PART 2: If the command HAS been issued, now we wait for ChainController to finish that movement
      else { // _commandIssuedInCurrentDeployStage is true
          // Check the *current* state of ChainController directly
          if (!chainController->isActive() || currentChainLength >= currentStageTargetLength) {
            ESP_LOGI(__FILE__, "DROP: Initial lowerAnchor complete or target %.2f m reached (current %.2f m). Transitioning to WAIT_TIGHT.", currentStageTargetLength, currentChainLength);
            transitionTo(WAIT_TIGHT); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_TIGHT) stage
          }
      }
      break;

    case WAIT_TIGHT:
      if (currentDistance != -999.0 && currentDistance >= targetDistanceInit) {
        ESP_LOGI(__FILE__, "WAIT_TIGHT: Distance target met (%.2f >= %.2f). Transitioning to HOLD_DROP.", currentDistance, targetDistanceInit);
        transitionTo(HOLD_DROP);
        stageStartTime = millis();
      } else if (currentDistance == -999.0) {
          ESP_LOGW(__FILE__, "WAIT_TIGHT: distanceListener has no value yet!");
      }
      break;

    case HOLD_DROP:
      if (millis() - stageStartTime >= 2000) { // hold for 2s
        ESP_LOGI(__FILE__, "HOLD_DROP: Hold time complete. Transitioning to DEPLOY_30.");
        transitionTo(DEPLOY_30);
        currentStageTargetLength = 0.0;
      }
      break;

     case DEPLOY_30:
      // Check if we've reached the stage's target
      if (currentChainLength >= chain30) {
        ESP_LOGI(__FILE__, "DEPLOY_30: Target %.2f m reached (current %.2f m). Transitioning to WAIT_30.", chain30, currentChainLength);
        if (deployPulseEvent != nullptr) {
            sensesp::event_loop()->remove(deployPulseEvent);
            deployPulseEvent = nullptr; // Stop the deployment pulses
        }
        transitionTo(WAIT_30);
        stageStartTime = millis();
        break; // Exit switch after transition
      }

      // If not at target, ensure pulses are started.
      if (deployPulseEvent == nullptr) {
        ESP_LOGI(__FILE__, "DEPLOY_30: Initiating first deployment pulse for stage.");
        startDeployPulse(chain30); // Pass the target for this stage
      }
      // The updateDeployment() loop will continue to run, and the deployPulseEvent
      // will handle commanding the ChainController in the background.
      // updateDeployment() will simply check 'currentChainLength >= chain30'
      // on each tick until it's met.
      break;

    case WAIT_30:
      if (currentDistance != -999.0 && currentDistance >= targetDistance30) {
        ESP_LOGI(__FILE__, "WAIT_30: Distance target met (%.2f >= %.2f). Transitioning to HOLD_30.", currentDistance, targetDistance30);
        transitionTo(HOLD_30);
        stageStartTime = millis();
      } else if (currentDistance == -999.0) {
          ESP_LOGW(__FILE__, "WAIT_30: distanceListener has no value yet!");
      }
      break;

    case HOLD_30:
      if (millis() - stageStartTime >= 30000) { // hold for 30s
        ESP_LOGI(__FILE__, "HOLD_30: Hold time complete. Transitioning to DEPLOY_75.");
        transitionTo(DEPLOY_75);
        currentStageTargetLength = 0.0;
      }
      break;

    case DEPLOY_75:
      if (currentChainLength >= chain75) {
        ESP_LOGI(__FILE__, "DEPLOY_75: Target %.2f m reached (current %.2f m). Transitioning to WAIT_75.", chain75, currentChainLength);
        if (deployPulseEvent != nullptr) {
            sensesp::event_loop()->remove(deployPulseEvent);
            deployPulseEvent = nullptr;
        }
        transitionTo(WAIT_75);
        stageStartTime = millis();
        break;
      }
      if (deployPulseEvent == nullptr) {
        ESP_LOGI(__FILE__, "DEPLOY_75: Initiating first deployment pulse for stage.");
        startDeployPulse(chain75);
      }
      break;

    case WAIT_75:
      if (currentDistance != -999.0 && currentDistance >= targetDistance75) {
        ESP_LOGI(__FILE__, "WAIT_75: Distance target met (%.2f >= %.2f). Transitioning to HOLD_75.", currentDistance, targetDistance75);
        transitionTo(HOLD_75);
        stageStartTime = millis();
      } else if (currentDistance == -999.0) {
          ESP_LOGW(__FILE__, "WAIT_75: distanceListener has no value yet!");
      }
      break;

    case HOLD_75:
      if (millis() - stageStartTime >= 75000) { // hold for 75s
        ESP_LOGI(__FILE__, "HOLD_75: Hold time complete. Transitioning to DEPLOY_100.");
        transitionTo(DEPLOY_100);
        currentStageTargetLength = 0.0;
      }
      break;

    case DEPLOY_100:
      if (currentChainLength >= totalChainLength) {
        ESP_LOGI(__FILE__, "DEPLOY_100: Target %.2f m reached (current %.2f m). Transitioning to COMPLETE.");
        if (deployPulseEvent != nullptr) {
            sensesp::event_loop()->remove(deployPulseEvent);
            deployPulseEvent = nullptr;
        }
        transitionTo(COMPLETE);
        stop(); // Ensure stop is called at completion
        break;
      }
      if (deployPulseEvent == nullptr) {
        ESP_LOGI(__FILE__, "DEPLOY_100: Initiating first deployment pulse for stage.");
        startDeployPulse(totalChainLength);
      }
      break;

    case COMPLETE:
      ESP_LOGI(__FILE__, "DeploymentManager: In COMPLETE stage. AutoDrop finished.");
      stop(); // or leave running as needed
      break;
  }
}

void DeploymentManager::transitionTo(Stage newStage) {
  if (currentStage != newStage) {
    ESP_LOGI(__FILE__, "AutoDeploy: Transitioning from stage %d to %d", (int)currentStage, (int)newStage);
    currentStage = newStage;
    _commandIssuedInCurrentDeployStage = false;
  }
}

bool DeploymentManager::isAutoAnchorValid() {
    float currentDepth = chainController->getDepthListener()->get();
  if (isnan(currentDepth) || isinf(currentDepth))
    return false;
  if (currentDepth < 3.0f || currentDepth > 45.0f)
    return false;

  return true;
}

