#include "DeploymentManager.h"
#include "events.h"
#include <cmath>
#include <Arduino.h>

DeploymentManager::DeploymentManager(ChainController* chainCtrl,
                                         sensesp::ValueProducer<float>* distanceListener,
                                         sensesp::ValueProducer<float>* depthListener)
  : chainController(chainCtrl),
    distanceListener(distanceListener),
    depthListener(depthListener),
    currentStage(IDLE),
    isRunning(false),
    dropInitiated(false),
    updateEvent(nullptr), 
    speedUpdateEvent(nullptr),
    ewmaDistance(0.0), 
    lastRawDistanceForEWMA(0.0),
    _commandIssuedInCurrentDeployStage(false) {}

void DeploymentManager::strBoatSpeed() {
  startSpeedMeasurement();
}

void DeploymentManager::start() {
  if (isRunning) return; // already active
  if (!isAutoAnchorValid()) return;  // invalid conditions

  isRunning = true;

  // Set your parameters, e.g.:

  targetDropDepth = depthListener->get() + 4.0; // initial slack
  anchorDepth = targetDropDepth - 2.0;          // anchor depth
  totalChainLength = 5 * (targetDropDepth - 2);
  chain30 = 0.3 * totalChainLength;
  chain75 = 0.75 * totalChainLength;
  targetDistanceInit = computeTargetHorizontalDistance(targetDropDepth, anchorDepth);
  targetDistance30 = computeTargetHorizontalDistance(chain30, anchorDepth) * 0.95; // reduction factor
  targetDistance75 = computeTargetHorizontalDistance(chain75, anchorDepth) * 0.95; // reduction factor  

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
  _commandIssuedInCurrentDeployStage = false;

  startSpeedMeasurement(); 
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
  stopSpeedMeasurement();
  currentStage = IDLE;
}

void DeploymentManager::reset() {
  stop();
  // Reset internal states if needed
  // (e.g., reset flags, timers)
}

void DeploymentManager::startSpeedMeasurement() {
  measuring = true;
  lastDistance = distanceListener->get();
  lastTime = millis();
  ewmaSpeed = 0;
  ewmaDistance = lastDistance;
  lastRawDistanceForEWMA = lastDistance;

  speedUpdateEvent = sensesp::event_loop()->onRepeat(
    100,
    [this]() {
      if (this->measuring) {
        unsigned long now = millis();
        float currentRawDistance = this->distanceListener->get();
        this->ewmaDistance = distance_alpha * currentRawDistance + (1 - distance_alpha) * this->ewmaDistance;
        float dt = (now - this->lastTime) / 1000.0;

        float instSpeed = (currentRawDistance - this->lastDistance) / dt;
        this->ewmaSpeed = alpha * instSpeed + (1 - alpha) * this->ewmaSpeed;

        this->lastDistance = currentRawDistance;
        this->lastTime = now;
      }
    }
  );
}

void DeploymentManager::stopSpeedMeasurement() {
  measuring = false;
  if (speedUpdateEvent != nullptr) {
    sensesp::event_loop()->remove(speedUpdateEvent);
    speedUpdateEvent = nullptr;
  }
}

float DeploymentManager::getCurrentSpeed() {
  if (fabs(ewmaSpeed) < 0.001) return 0.0f;
  return ewmaSpeed;
}

float DeploymentManager::computeTargetHorizontalDistance(float chainLength, float anchorDepth) {
  return sqrt(pow(chainLength, 2) - pow(anchorDepth, 2));
}

// void DeploymentManager::waitUntilChainTight(float targetDistance, std::function<void()> onTightReached) {
//     std::function<void()> checkFunc;
//     checkFunc = [this, targetDistance, onTightReached, &checkFunc]() {
//     if (this->distanceListener->get() >= targetDistance) {
//       onTightReached();
//     } else {
//       sensesp::event_loop()->onDelay(100, checkFunc);
//     }
//   };
//   // Start immediate check
//   sensesp::event_loop()->onDelay(0, checkFunc);
// }

// void DeploymentManager::controlledChainDeployment(float targetChainLength) {
//   if (!isRunning) return;

//   float currentLength = chainController->getChainLength();

//   // Check if we've already deployed enough
//   if (currentLength >= targetChainLength) {
//     return; // done
//   }

//   // Calculate remaining chain to deploy
//   float remaining = targetChainLength - currentLength;

//   float boatSpeed = getCurrentSpeed();  // m/s
//   unsigned long now = millis();

//   // Calculate expected boat movement in 5 seconds
//   float expectedMovement = boatSpeed * 5.0; // 5 seconds
// //   ESP_LOGI(__FILE__, "boat speed %f expected move in 5s %fm ", boatSpeed, expectedMovement);
//     // Decide how much to deploy this tick
//   float deployStep = expectedMovement; 
//   if (remaining < deployStep) {
//     deployStep = remaining;
//   }

//   // Deploy the chain
//   chainController->lowerAnchor(deployStep);

//   // Schedule next step after 5 seconds
//   sensesp::event_loop()->onDelay(5000, 
//     std::bind(&DeploymentManager::controlledChainDeployment, 
//         this, targetChainLength));
// }

void DeploymentManager::updateDeployment() {
  if (!isRunning) {
    ESP_LOGD(__FILE__, "DeploymentManager: updateDeployment called but not running. Current stage: %d", (int)currentStage);
    return; // exit if stopped
  }

  float currentChainLength = chainController->getChainLength();
 
 ////////////////////////////////////////////////////
  bool chainControllerActive = chainController->isActive();
 //////////////////////////////////take out later/////////
 
  float currentDistance = this->ewmaDistance; 

  switch (currentStage) {
    case IDLE:
      // Nothing to do
      break;

    case DROP:
      // currentChainLength and chainController->isActive() are already defined at the top of updateDeployment()
      ESP_LOGI(__FILE__, "DeploymentManager: In DROP stage. Chain: %.2f m, Target: %.2f m, Active: %d, CommandIssued: %d", currentChainLength, targetDropDepth, chainController->isActive(), _commandIssuedInCurrentDeployStage);

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
            ESP_LOGI(__FILE__, "DROP: Initial lowerAnchor complete (active: %d) or target %.2f m reached (current %.2f m). Transitioning to WAIT_TIGHT.", chainController->isActive(), currentStageTargetLength, currentChainLength);
            transitionTo(WAIT_TIGHT); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_TIGHT) stage
          } else {
            ESP_LOGD(__FILE__, "DROP: ChainController active (%d) and target not yet reached (current %.2f < target %.2f). Waiting for movement completion.", chainController->isActive(), currentChainLength, currentStageTargetLength);
          }
      }
      break;

    case WAIT_TIGHT:
      ESP_LOGI(__FILE__, "DeploymentManager: In WAIT_TIGHT stage. Current distanceFromBow: %.2f m, TargetDistanceInit: %.2f m", currentDistance, targetDistanceInit);
      if (currentDistance != -999.0 && currentDistance >= targetDistanceInit) {
        ESP_LOGI(__FILE__, "WAIT_TIGHT: Distance target met (%.2f >= %.2f). Transitioning to HOLD_DROP.", currentDistance, targetDistanceInit);
        transitionTo(HOLD_DROP);
        stageStartTime = millis();
      } else if (currentDistance == -999.0) {
          ESP_LOGW(__FILE__, "WAIT_TIGHT: distanceListener has no value yet!");
      }
      break;

    case HOLD_DROP:
      ESP_LOGI(__FILE__, "DeploymentManager: In HOLD_DROP stage. Time elapsed: %lu ms", millis() - stageStartTime);
      if (millis() - stageStartTime >= 2000) { // hold for 2s
        ESP_LOGI(__FILE__, "HOLD_DROP: Hold time complete. Transitioning to DEPLOY_30.");
        transitionTo(DEPLOY_30);
        currentStageTargetLength = 0.0; 
      }
      break;

    case DEPLOY_30:
      // currentChainLength and chainController->isActive() are already defined at the top of updateDeployment()
      ESP_LOGI(__FILE__, "DeploymentManager: In DEPLOY_30 stage. Chain: %.2f m, Target: %.2f m, Active: %d, CommandIssued: %d", currentChainLength, chain30, chainController->isActive(), _commandIssuedInCurrentDeployStage);

      // PART 1: Issue the command if it hasn't been issued yet for THIS DEPLOY_30 stage
      if (!_commandIssuedInCurrentDeployStage) {
        // Only command if we still need to lower more chain to reach chain30
        if (currentChainLength < chain30) {
          float amount_to_lower = chain30 - currentChainLength;
          if (amount_to_lower > 0.01) { // Command only if a significant amount remains (e.g., > 1 cm)
            ESP_LOGI(__FILE__, "DEPLOY_30: Commanding lowerAnchor by %.2f m to reach %.2f m", amount_to_lower, chain30);
            chainController->lowerAnchor(amount_to_lower);
            currentStageTargetLength = chain30; // Set the absolute target for this stage
            _commandIssuedInCurrentDeployStage = true; // Mark command as issued
            // Do NOT transition yet. We just started the movement.
          } else {
            // Already effectively at target or past it (within tolerance), so just transition
            ESP_LOGI(__FILE__, "DEPLOY_30: Already at or past %.2f m (current %.2f). Transitioning to WAIT_30.", chain30, currentChainLength);
            transitionTo(WAIT_30); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_30) stage
          }
        } else {
            // We started this DEPLOY_30 stage already at or past its target, so transition
            ESP_LOGI(__FILE__, "DEPLOY_30: Started at or past %.2f m (current %.2f). Transitioning to WAIT_30.", chain30, currentChainLength);
            transitionTo(WAIT_30); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_30) stage
        }
      } 
      // PART 2: If the command HAS been issued, now we wait for ChainController to finish that movement
      else { // _commandIssuedInCurrentDeployStage is true
          // Check the *current* state of ChainController directly
          if (!chainController->isActive() || currentChainLength >= currentStageTargetLength) {
            ESP_LOGI(__FILE__, "DEPLOY_30: ChainController reports inactive (%d) or target %.2f m reached (current %.2f m). Transitioning to WAIT_30.", chainController->isActive(), currentStageTargetLength, currentChainLength);
            transitionTo(WAIT_30); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_30) stage
          } else {
            ESP_LOGD(__FILE__, "DEPLOY_30: ChainController active (%d) and target not yet reached (current %.2f < target %.2f). Waiting for movement completion.", chainController->isActive(), currentChainLength, currentStageTargetLength);
          }
      }
      break;

    case WAIT_30:
      ESP_LOGI(__FILE__, "DeploymentManager: In WAIT_30 stage. Current distanceFromBow: %.2f m, TargetDistance30: %.2f m", currentDistance, targetDistance30);
      if (currentDistance != -999.0 && currentDistance >= targetDistance30) {
        ESP_LOGI(__FILE__, "WAIT_30: Distance target met (%.2f >= %.2f). Transitioning to HOLD_30.", currentDistance, targetDistance30);
        transitionTo(HOLD_30);
        stageStartTime = millis();
      } else if (currentDistance == -999.0) {
          ESP_LOGW(__FILE__, "WAIT_30: distanceListener has no value yet!");
      }
      break;

    case HOLD_30:
      ESP_LOGI(__FILE__, "DeploymentManager: In HOLD_30 stage. Time elapsed: %lu ms", millis() - stageStartTime);
      if (millis() - stageStartTime >= 30000) { // hold for 30s
        ESP_LOGI(__FILE__, "HOLD_30: Hold time complete. Transitioning to DEPLOY_75.");
        transitionTo(DEPLOY_75);
        currentStageTargetLength = 0.0;
      }
      break;

    case DEPLOY_75:
      // currentChainLength and chainController->isActive() are already defined at the top of updateDeployment()
      ESP_LOGI(__FILE__, "DeploymentManager: In DEPLOY_75 stage. Chain: %.2f m, Target: %.2f m, Active: %d, CommandIssued: %d", currentChainLength, chain75, chainController->isActive(), _commandIssuedInCurrentDeployStage);

      // PART 1: Issue the command if it hasn't been issued yet for THIS DEPLOY_75 stage
      if (!_commandIssuedInCurrentDeployStage) {
        // Only command if we still need to lower more chain to reach chain75
        if (currentChainLength < chain75) {
          float amount_to_lower = chain75 - currentChainLength;
          if (amount_to_lower > 0.01) { // Command only if a significant amount remains (e.g., > 1 cm)
            ESP_LOGI(__FILE__, "DEPLOY_75: Commanding lowerAnchor by %.2f m to reach %.2f m", amount_to_lower, chain75);
            chainController->lowerAnchor(amount_to_lower);
            currentStageTargetLength = chain75; // Set the absolute target for this stage
            _commandIssuedInCurrentDeployStage = true; // Mark command as issued
            // Do NOT transition yet. We just started the movement.
          } else {
            // Already effectively at target or past it (within tolerance), so just transition
            ESP_LOGI(__FILE__, "DEPLOY_75: Already at or past %.2f m (current %.2f). Transitioning to WAIT_75.", chain75, currentChainLength);
            transitionTo(WAIT_75); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_75) stage
          }
        } else {
            // We started this DEPLOY_75 stage already at or past its target, so transition
            ESP_LOGI(__FILE__, "DEPLOY_75: Started at or past %.2f m (current %.2f). Transitioning to WAIT_75.", chain75, currentChainLength);
            transitionTo(WAIT_75); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_75) stage
        }
      } 
      // PART 2: If the command HAS been issued, now we wait for ChainController to finish that movement
      else { // _commandIssuedInCurrentDeployStage is true
          // Check the *current* state of ChainController directly
          if (!chainController->isActive() || currentChainLength >= currentStageTargetLength) {
            ESP_LOGI(__FILE__, "DEPLOY_75: ChainController reports inactive (%d) or target %.2f m reached (current %.2f m). Transitioning to WAIT_75.", chainController->isActive(), currentStageTargetLength, currentChainLength);
            transitionTo(WAIT_75); // This will reset _commandIssuedInCurrentDeployStage
            stageStartTime = millis(); // Start the timer for the next (WAIT_75) stage
          } else {
            ESP_LOGD(__FILE__, "DEPLOY_75: ChainController active (%d) and target not yet reached (current %.2f < target %.2f). Waiting for movement completion.", chainController->isActive(), currentChainLength, currentStageTargetLength);
          }
      }
      break;

    case WAIT_75:
      ESP_LOGI(__FILE__, "DeploymentManager: In WAIT_75 stage. Current distanceFromBow: %.2f m, TargetDistance75: %.2f m", currentDistance, targetDistance75);
      if (currentDistance != -999.0 && currentDistance >= targetDistance75) {
        ESP_LOGI(__FILE__, "WAIT_75: Distance target met (%.2f >= %.2f). Transitioning to HOLD_75.", currentDistance, targetDistance75);
        transitionTo(HOLD_75);
        stageStartTime = millis();
      } else if (currentDistance == -999.0) {
          ESP_LOGW(__FILE__, "WAIT_75: distanceListener has no value yet!");
      }
      break;

    case HOLD_75:
      ESP_LOGI(__FILE__, "DeploymentManager: In HOLD_75 stage. Time elapsed: %lu ms", millis() - stageStartTime);
      if (millis() - stageStartTime >= 75000) { // hold for 75s
        ESP_LOGI(__FILE__, "HOLD_75: Hold time complete. Transitioning to DEPLOY_100.");
        transitionTo(DEPLOY_100);
        currentStageTargetLength = 0.0;
      }
      break;

    case DEPLOY_100:
      ESP_LOGI(__FILE__, "DeploymentManager: In DEPLOY_100 stage. Chain: %.2f m, Target: %.2f m, Active: %d, CommandIssued: %d", currentChainLength, totalChainLength, chainController->isActive(), _commandIssuedInCurrentDeployStage);
      if(!_commandIssuedInCurrentDeployStage) {
        if (currentChainLength < totalChainLength) {
            float amount_to_lower = totalChainLength - currentChainLength;
            if (amount_to_lower > 0.01) {
            ESP_LOGI(__FILE__, "DEPLOY_100: Commanding lowerAnchor by %.2f m to reach %.2f m", amount_to_lower, totalChainLength);
            chainController->lowerAnchor(amount_to_lower);
            _commandIssuedInCurrentDeployStage = true;
                      currentStageTargetLength = totalChainLength;
            } else {
            ESP_LOGI(__FILE__, "DEPLOY_100: Already at or past %.2f m (current %.2f). Transitioning to COMPLETE.", totalChainLength, currentChainLength);
            transitionTo(COMPLETE);
            currentStageTargetLength = 0.0;
            stop();
            }
        } else {
            
                ESP_LOGI(__FILE__, "DEPLOY_100: ChainController reports inactive or target %.2f m reached (current %.2f m). Transitioning to COMPLETE.", currentStageTargetLength, currentChainLength);
                transitionTo(COMPLETE);
                stop(); // Stop the entire process
                currentStageTargetLength = 0.0;
          } 
      }else {
        if (!chainController->isActive() || currentChainLength >= currentStageTargetLength) {
            ESP_LOGI(__FILE__, "DEPLOY_100: ChainController reports inactive (%d) or target %.2f m reached (current %.2f m). Transitioning to COMPLETE.", chainController->isActive(), currentStageTargetLength, currentChainLength);
            transitionTo(COMPLETE); // This will reset _commandIssuedInCurrentDeployStage
            stop(); // Stop the entire process
        } else {
                ESP_LOGD(__FILE__, "DEPLOY_100: ChainController active (%d) and target not yet reached (current %.2f < target %.2f). Waiting.", chainController->isActive(), currentChainLength, currentStageTargetLength);
            }
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
    float currentDepth = depthListener->get();
  if (isnan(currentDepth) || isinf(currentDepth))
    return false;
  if (currentDepth < 3.0f || currentDepth > 45.0f)
    return false;

  return true;
}

