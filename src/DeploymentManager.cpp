#include "DeploymentManager.h"
#include "events.h"
#include <cmath>
#include <Arduino.h>

DeploymentManager::DeploymentManager(ChainController* chainCtrl)
  : chainController(chainCtrl),
    currentStage(IDLE),
    isRunning(false),
    dropInitiated(false),
    updateEvent(nullptr),
    autoStageObservable_(new sensesp::ObservableValue<String>("Idle")) {

  // Connect autoStage observable to Signal K output
  autoStageObservable_->connect_to(
      new sensesp::SKOutputString("navigation.anchor.autoStage", "/anchor/autoStage")
  );

  ESP_LOGI(__FILE__, "DeploymentManager initialized, autoStage publishing to Signal K");
}

void DeploymentManager::start(float scopeRatio) {
  if (isRunning) return; // already active
  if (!isAutoAnchorValid()) return;  // invalid conditions

  isRunning = true;

  // Validate and clamp scope ratio
  if (scopeRatio < MIN_SCOPE_RATIO) {
      ESP_LOGW(__FILE__, "Scope ratio %.1f below minimum, clamping to %.1f", scopeRatio, MIN_SCOPE_RATIO);
      scopeRatio = MIN_SCOPE_RATIO;
  } else if (scopeRatio > MAX_SCOPE_RATIO) {
      ESP_LOGW(__FILE__, "Scope ratio %.1f above maximum, clamping to %.1f", scopeRatio, MAX_SCOPE_RATIO);
      scopeRatio = MAX_SCOPE_RATIO;
  }
  scopeRatio_ = scopeRatio;

  // Get current depth and tide-adjusted depth for chain calculations
  float currentDepth = chainController->getCurrentDepth();
  float tideAdjustedDepth = chainController->getTideAdjustedDepth();
  float currentDistance = chainController->getDistanceListener()->get();

  // Calculate total chain needed based on scope ratio
  // Use tide-adjusted depth for deployment calculations to ensure adequate chain for high tide
  anchorDepth = tideAdjustedDepth + 2.0;  // Add 2m for bow height
  totalChainLength = scopeRatio_ * anchorDepth;  // Scope based on total depth including bow height
  chain30 = 0.40 * totalChainLength;
  chain75 = 0.80 * totalChainLength;

  // Calculate catenary-adjusted distances for each stage
  // These are the horizontal distances the boat should drift to
  targetDistance30 = computeTargetHorizontalDistance(chain30, anchorDepth);
  targetDistance75 = computeTargetHorizontalDistance(chain75, anchorDepth);

  // IMPROVED: Calculate initial drop to ensure boat can drift to proper distance
  // The initial drop should create enough slack for boat to drift to ~50% of scope
  // For a 5:1 scope, we want boat to initially drift to about 2.5x depth
  // With catenary sag, we need MORE chain than the straight-line distance
  //
  // Strategy: Deploy enough chain so that with ~2-3m of slack, the boat can
  // drift to approximately half the final scope distance
  float desiredInitialDistance = 0.5 * computeTargetHorizontalDistance(totalChainLength, anchorDepth);

  // To find chain length needed for this distance with slack:
  // We want: chain = distance_needed + slack_buffer
  // Use iterative approach: start with straight-line estimate, adjust for catenary
  float straightLineToDesiredDistance = sqrt(pow(desiredInitialDistance, 2) + pow(anchorDepth, 2));

  // Add slack buffer (4-6 meters depending on depth)
  float slackBuffer = fmin(6.0, fmax(4.0, tideAdjustedDepth * 0.3));
  targetDropDepth = straightLineToDesiredDistance + slackBuffer;

  // Ensure initial drop is at least depth + bow height + minimum slack
  float minimumInitialDrop = anchorDepth + 3.0;
  targetDropDepth = fmax(targetDropDepth, minimumInitialDrop);

  // Calculate what distance this initial drop will produce
  targetDistanceInit = computeTargetHorizontalDistance(targetDropDepth, anchorDepth);

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

  // Publish initial stage to Signal K
  publishStage(currentStage);

  ESP_LOGI(__FILE__, "DeploymentManager: Starting autoDrop. Scope: %.1f:1, Current depth: %.2f, Tide-adjusted: %.2f, Total Chain: %.2f",
           scopeRatio_, currentDepth, tideAdjustedDepth, totalChainLength);

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

  // Publish Idle state to Signal K
  publishStage(IDLE);

  // Notify completion callback if set
  if (completionCallback_) {
    completionCallback_();
  }
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

void DeploymentManager::startContinuousDeployment(float stageTargetChainLength) {
    // Issue the full deployment command for the stage target
    float current_chain = chainController->getChainLength();
    float amount_to_deploy = stageTargetChainLength - current_chain;

    if (amount_to_deploy > 0.1) {  // Only deploy if significant amount remains
        ESP_LOGI(__FILE__, "DeploymentManager: Starting continuous deployment of %.2fm to reach %.2fm",
                 amount_to_deploy, stageTargetChainLength);
        chainController->lowerAnchor(amount_to_deploy);
    }

    // Schedule monitoring check every 500ms
    if (deployPulseEvent != nullptr) {
        sensesp::event_loop()->remove(deployPulseEvent);
    }
    deployPulseEvent = sensesp::event_loop()->onRepeat(MONITOR_INTERVAL_MS, [this, stageTargetChainLength]() {
        monitorDeployment(stageTargetChainLength);
    });
}

void DeploymentManager::monitorDeployment(float stageTargetChainLength) {
    // Check if still in deployment stage
    if (!isRunning || (currentStage != DEPLOY_30 && currentStage != DEPLOY_75 && currentStage != DEPLOY_100)) {
        if (deployPulseEvent != nullptr) {
            sensesp::event_loop()->remove(deployPulseEvent);
            deployPulseEvent = nullptr;
        }
        return;
    }

    float current_chain = chainController->getChainLength();
    float current_slack = chainController->getHorizontalSlackObservable()->get();
    float current_depth = chainController->getCurrentDepth();

    // Check if stage target reached (will be handled by updateDeployment())
    if (current_chain >= stageTargetChainLength) {
        return;  // Let updateDeployment() handle stage transition
    }

    // Safety brake with hysteresis: pause at MAX_SLACK_RATIO, resume at RESUME_SLACK_RATIO
    float pause_threshold = current_depth * MAX_SLACK_RATIO;
    float resume_threshold = current_depth * RESUME_SLACK_RATIO;

    if (current_slack > pause_threshold) {
        // Stop deployment due to excessive slack
        if (chainController->isActive()) {
            ESP_LOGI(__FILE__, "DeploymentManager: Excessive slack (%.2fm > %.2fm). Pausing deployment.",
                     current_slack, pause_threshold);
            chainController->stop();
        }
    }
    // Resume deployment only when slack drops below the lower threshold (hysteresis)
    else if (current_slack < resume_threshold && !chainController->isActive() && current_chain < stageTargetChainLength) {
        float amount_remaining = stageTargetChainLength - current_chain;
        if (amount_remaining > 0.1) {
            ESP_LOGI(__FILE__, "DeploymentManager: Slack below resume threshold (%.2fm < %.2fm), resuming deployment of %.2fm",
                     current_slack, resume_threshold, amount_remaining);
            chainController->lowerAnchor(amount_remaining);
        }
    }
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
            ESP_LOGD(__FILE__, "DROP: Initial lowerAnchor complete or target %.2f m reached (current %.2f m). Transitioning to WAIT_TIGHT.", currentStageTargetLength, currentChainLength);
            transitionTo(WAIT_TIGHT); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_TIGHT) stage
          }
      }
      break;

    case WAIT_TIGHT: {
      // WAIT_TIGHT: Wait for boat to drift away from anchor to target distance
      // The chain is already deployed, now we wait for wind/current to move the boat
      float currentSlack = chainController->getHorizontalSlackObservable()->get();

      // Check if boat has reached target distance
      if (currentDistance != -999.0 && currentDistance >= targetDistanceInit) {
        ESP_LOGI(__FILE__, "WAIT_TIGHT: Distance target met (%.2f >= %.2f). Transitioning to HOLD_DROP.", currentDistance, targetDistanceInit);
        transitionTo(HOLD_DROP);
        stageStartTime = millis();
        break;
      }

      // If slack is tight or negative, it means boat has drifted to/past target distance
      // Tight chain = boat has reached desired scope, transition to next stage
      if (currentSlack < 0.5) {
        ESP_LOGI(__FILE__, "WAIT_TIGHT: Chain tight (slack=%.2f m), boat has reached target distance. Transitioning to HOLD_DROP.", currentSlack);
        transitionTo(HOLD_DROP);
        stageStartTime = millis();
        break;
      }

      // Still waiting for boat to drift - just monitor
      if (currentDistance == -999.0) {
        ESP_LOGD(__FILE__, "WAIT_TIGHT: Waiting for distance sensor data (slack=%.2f m)", currentSlack);
      } else {
        ESP_LOGD(__FILE__, "WAIT_TIGHT: Waiting for drift - current=%.2f m, target=%.2f m, slack=%.2f m",
                 currentDistance, targetDistanceInit, currentSlack);
      }
      break;
    }

    case HOLD_DROP:
      if (millis() - stageStartTime >= 2000) { // hold for 2s
        ESP_LOGD(__FILE__, "HOLD_DROP: Hold time complete. Transitioning to DEPLOY_30.");
        transitionTo(DEPLOY_30);
        currentStageTargetLength = 0.0;
      }
      break;

     case DEPLOY_30:
      // Check if we've reached the stage's target
      if (currentChainLength >= chain30) {
        ESP_LOGI(__FILE__, "DEPLOY_30: Target %.2f m reached. Transitioning to WAIT_30.", chain30);
        if (deployPulseEvent != nullptr) {
            sensesp::event_loop()->remove(deployPulseEvent);
            deployPulseEvent = nullptr;
        }
        if (chainController->isActive()) {
            chainController->stop();
        }
        transitionTo(WAIT_30);
        stageStartTime = millis();
        break;
      }

      // Start continuous deployment if not already started
      if (deployPulseEvent == nullptr) {
        ESP_LOGI(__FILE__, "DEPLOY_30: Starting continuous deployment to %.2fm", chain30);
        startContinuousDeployment(chain30);
      }
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
        ESP_LOGD(__FILE__, "HOLD_30: Hold time complete. Transitioning to DEPLOY_75.");
        transitionTo(DEPLOY_75);
        currentStageTargetLength = 0.0;
      }
      break;

    case DEPLOY_75:
      if (currentChainLength >= chain75) {
        ESP_LOGI(__FILE__, "DEPLOY_75: Target %.2f m reached. Transitioning to WAIT_75.", chain75);
        if (deployPulseEvent != nullptr) {
            sensesp::event_loop()->remove(deployPulseEvent);
            deployPulseEvent = nullptr;
        }
        if (chainController->isActive()) {
            chainController->stop();
        }
        transitionTo(WAIT_75);
        stageStartTime = millis();
        break;
      }
      // Start continuous deployment if not already started
      if (deployPulseEvent == nullptr) {
        ESP_LOGI(__FILE__, "DEPLOY_75: Starting continuous deployment to %.2fm", chain75);
        startContinuousDeployment(chain75);
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
        ESP_LOGD(__FILE__, "HOLD_75: Hold time complete. Transitioning to DEPLOY_100.");
        transitionTo(DEPLOY_100);
        currentStageTargetLength = 0.0;
      }
      break;

    case DEPLOY_100:
      if (currentChainLength >= totalChainLength) {
        ESP_LOGI(__FILE__, "DEPLOY_100: Target %.2f m reached. Transitioning to COMPLETE.", totalChainLength);
        if (deployPulseEvent != nullptr) {
            sensesp::event_loop()->remove(deployPulseEvent);
            deployPulseEvent = nullptr;
        }
        if (chainController->isActive()) {
            chainController->stop();
        }
        transitionTo(COMPLETE);
        stop(); // Ensure stop is called at completion
        break;
      }
      // Start continuous deployment if not already started
      if (deployPulseEvent == nullptr) {
        ESP_LOGI(__FILE__, "DEPLOY_100: Starting continuous deployment to %.2fm", totalChainLength);
        startContinuousDeployment(totalChainLength);
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

    // Publish new stage to Signal K
    publishStage(newStage);
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

String DeploymentManager::getStageDisplayName(Stage stage) const {
    switch (stage) {
        case IDLE:
            return "Idle";
        case DROP:
            return "Initial Drop";
        case WAIT_TIGHT:
        case HOLD_DROP:
            return "Alignment";
        case DEPLOY_30:
            return "Deploy 40";
        case WAIT_30:
        case HOLD_30:
            return "Digin 40";
        case DEPLOY_75:
            return "Deploy 80";
        case WAIT_75:
        case HOLD_75:
            return "Digin 80";
        case DEPLOY_100:
            return "Final Deploy";
        case COMPLETE:
            return "Idle";
        default:
            return "Unknown";
    }
}

void DeploymentManager::publishStage(Stage stage) {
    String displayName = getStageDisplayName(stage);
    autoStageObservable_->set(displayName);
}

