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
    speedUpdateEvent(nullptr),
    ewmaDistance(0.0),
    lastRawDistanceForEWMA(0.0) {

  // Initialize wind speed listener for catenary calculations
  // Using apparent wind speed which is what the boat experiences at anchor
  windSpeedListener_ = new sensesp::SKValueListener<float>(
    "environment.wind.speedApparent",  // Signal K path for apparent wind speed (m/s)
    2000,                               // Update interval: 2000ms
    "/wind/sk"                          // Config path
  );

  ESP_LOGI(__FILE__, "DeploymentManager: Wind speed listener initialized");
}

void DeploymentManager::strBoatSpeed() {
  startSpeedMeasurement();
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

  // Get dynamic horizontal force estimate from wind conditions
  float estimatedHorizontalForce = estimateHorizontalForce();

  // Calculate catenary-adjusted distances using physics-based model
  targetDistanceInit = computeTargetHorizontalDistance(targetDropDepth, anchorDepth);
  float reductionFactor30 = computeCatenaryReductionFactor(chain30, anchorDepth, estimatedHorizontalForce);
  float reductionFactor75 = computeCatenaryReductionFactor(chain75, anchorDepth, estimatedHorizontalForce);
  targetDistance30 = computeTargetHorizontalDistance(chain30, anchorDepth) * reductionFactor30;
  targetDistance75 = computeTargetHorizontalDistance(chain75, anchorDepth) * reductionFactor75;

  ESP_LOGI(__FILE__, "DeploymentManager: Catenary calcs - Force: %.0fN, Factor30: %.3f, Factor75: %.3f",
           estimatedHorizontalForce, reductionFactor30, reductionFactor75);  

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

  startSpeedMeasurement(); 
  ESP_LOGI(__FILE__, "DeploymentManager: Starting autoDrop. Initial depth: %.2f, Target Drop Depth: %.2f, Total Chain: %.2f", (targetDropDepth-2), targetDropDepth, totalChainLength);

  _lastDistanceAtDeploymentCommand = ewmaDistance; 
  _accumulatedDeployDemand = 0.0;

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
  if (deployPulseEvent != nullptr) { // <--- ADDED
  sensesp::event_loop()->remove(deployPulseEvent);
  deployPulseEvent = nullptr;
  }
  _accumulatedDeployDemand = 0.0;
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
  float initialRawDistance = chainController->getDistanceListener()->get();
  lastDistance = initialRawDistance;
  ewmaDistance = initialRawDistance;
  lastTime = millis();
  ewmaSpeed = 0;
  lastRawDistanceForEWMA = lastDistance;

  speedUpdateEvent = sensesp::event_loop()->onRepeat(
    100,
    [this]() {
      if (this->measuring) {
        unsigned long now = millis();
        float currentRawDistance = this->chainController->getDistanceListener()->get();
        float currentRawDepth = this->chainController->getDepthListener()->get();
        
                  // Guard against NaN/Inf in live sensor readings for distance
          if (isnan(currentRawDistance) || isinf(currentRawDistance)) {
              ESP_LOGW(__FILE__, "Speed Measurement: Raw distance reading is NaN/Inf (%.2f). Skipping update for this cycle.", currentRawDistance);
              return;
          }
          // Guard against NaN/Inf for depth as well
          if (isnan(currentRawDepth) || isinf(currentRawDepth)) {
              ESP_LOGW(__FILE__, "Speed Measurement: Raw depth reading is NaN/Inf (%.2f). Skipping update for this cycle.", currentRawDepth);
              return;
          }
        
        
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

float DeploymentManager::estimateHorizontalForce() {
  // Estimate horizontal force on the boat from wind
  // This combines wind drag force with a baseline for current/resistance

  float windSpeed = windSpeedListener_->get(); // m/s from Signal K

  // Validate wind speed data - if invalid, use 10 knots as default
  if (isnan(windSpeed) || isinf(windSpeed) || windSpeed < 0.0) {
    windSpeed = 10.0 / 1.944; // 10 knots converted to m/s (~5.14 m/s)
    ESP_LOGW(__FILE__, "Invalid wind speed data. Using default: 10 knots (%.2f m/s)", windSpeed);
  }

  // Wind force formula: F = 0.5 * ρ * Cd * A * v²
  // where: ρ = air density, Cd = drag coefficient, A = windage area, v = wind speed
  float windForce = 0.5 * AIR_DENSITY * DRAG_COEFFICIENT * BOAT_WINDAGE_AREA_M2 * pow(windSpeed, 2);

  // Add baseline force for current and hull resistance (typically 20-50N)
  float baselineForce = 30.0;

  float totalForce = windForce + baselineForce;

  // Clamp to reasonable bounds (min 30N, max 2000N for safety)
  totalForce = fmax(30.0, fmin(2000.0, totalForce));

  ESP_LOGD(__FILE__, "Force estimate: wind=%.1f m/s (%.1f knots), windForce=%.0fN, total=%.0fN",
           windSpeed, windSpeed * 1.944, windForce, totalForce);

  return totalForce;
}

float DeploymentManager::computeCatenaryReductionFactor(float chainLength, float anchorDepth, float horizontalForce) {
  // This function calculates the reduction factor due to chain catenary
  // Based on catenary curve physics with chain weight and horizontal tension
  //
  // KEY PHYSICS:
  // - LOW force (light wind) → MORE sag → LESS horizontal distance → LOWER reduction factor
  // - HIGH force (strong wind) → LESS sag → MORE horizontal distance → HIGHER reduction factor

  // Chain weight per meter in water (N/m)
  float w = CHAIN_WEIGHT_PER_METER_KG * GRAVITY;

  // If horizontal force is very small, there's significant sag
  if (horizontalForce < 50.0) {
    // Light wind/current: LOTS of catenary sag (low tension)
    // Lower reduction factors = more sag = less horizontal distance
    float scopeRatio = chainLength / fmax(anchorDepth, 1.0);
    if (scopeRatio < 3.0) {
      return 0.90;  // Significant sag even on short scope
    } else if (scopeRatio < 5.0) {
      return 0.85;  // More sag on typical scope
    } else {
      return 0.80;  // Lots of sag on long scope
    }
  }

  // Calculate catenary parameter: a = H/w
  // where H is horizontal tension at anchor, w is weight per unit length
  float a = horizontalForce / w;

  // For a catenary with horizontal force H and vertical drop of anchorDepth,
  // the relationship is complex. Using approximation for moderate sag:
  // horizontal_distance ≈ sqrt(chainLength² - anchorDepth²) - (w * chainLength²)/(8*H)
  //
  // The sag reduction term (w * L²)/(8*H) gets SMALLER as H increases (tighter chain)

  // Calculate theoretical straight-line horizontal distance
  float straightLineDistance = sqrt(pow(chainLength, 2) - pow(anchorDepth, 2));

  // Calculate catenary sag reduction (horizontal distance lost to sag)
  // This gets SMALLER with higher force (less sag when chain is pulled tight)
  float catenarySagReduction = (w * pow(chainLength, 2)) / (8.0 * horizontalForce);

  // Actual horizontal distance accounting for catenary
  float actualHorizontalDistance = straightLineDistance - catenarySagReduction;

  // Reduction factor is the ratio
  // Higher force → smaller sag reduction → higher actual distance → higher factor
  float reductionFactor = actualHorizontalDistance / straightLineDistance;

  // Clamp between reasonable bounds (0.80 to 0.99)
  reductionFactor = fmax(0.80, fmin(0.99, reductionFactor));

  ESP_LOGD(__FILE__, "Catenary calc: chainLen=%.2f, depth=%.2f, force=%.0fN, a=%.2f, sagReduction=%.2f, factor=%.3f",
           chainLength, anchorDepth, horizontalForce, a, catenarySagReduction, reductionFactor);

  return reductionFactor;
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

        float current_boat_speed_mps = this->getCurrentSpeed(); // m/s (from EWMA)
        float current_chain_length = this->chainController->getChainLength();
        float windlass_deployment_rate_mps = 1000.0 / this->chainController->getDownSpeed(); // m/s (from ChainController's calibrated speed)
        float current_horizontal_slack = this->chainController->getHorizontalSlackObservable()->get();
        float current_depth = this->chainController->getCurrentDepth();
        float dynamic_max_acceptable_slack = current_depth * .5;



        // --- Deployment Tuning Parameters (can become configurable via SensESP UI) ---
        const float slack_factor = 1.1; // Multiplier to deploy slightly more chain than boat movement (e.g., 1.1x speed)
        const unsigned long decision_window_ms = 500; // How frequently we make a "deploy decision" if windlass is faster than boat
        const float min_deploy_amount_meters = 0.5; // Minimal amount to deploy if boat speed is very low/zero
        const float effectively_stopped_speed_mps = 0.1; // Threshold for boat speed (0.1 m/s = ~0.2 knots)

        // If ChainController is currently moving, we MUST wait for it to finish.
        if (this->chainController->isActive()) {
            ESP_LOGD(__FILE__, "Deploy Pulse: ChainController is active. Rescheduling pulse in %lu ms.", DECISION_WINDOW_MS);
            deployPulseEvent = sensesp::event_loop()->onDelay(DECISION_WINDOW_MS, [this, stageTargetChainLength]() {
                startDeployPulse(stageTargetChainLength);
            });
            return;
        }

        // --- Calculate Desired Amount to Deploy ---
        float desired_payout_this_cycle = 0.0;
        if (current_boat_speed_mps <= 0.1) { // effectively stopped
            desired_payout_this_cycle = 0.0; // Accumulate only via drift
            ESP_LOGD(__FILE__, "Deploy Pulse: Boat speed very low (%.2f m/s). Minimal demand this cycle.", current_boat_speed_mps);
        } else {  // Boat is moving back
            float boat_movement_in_window = current_boat_speed_mps * (DECISION_WINDOW_MS / 1000.0);
            desired_payout_this_cycle = boat_movement_in_window * slack_factor;
            ESP_LOGD(__FILE__, "Deploy Pulse: Boat speed: %.2f m/s. Desired payout this cycle: %.2f m", current_boat_speed_mps, desired_payout_this_cycle);
        }

        // --- 3. Accumulate Demand ---
        _accumulatedDeployDemand += desired_payout_this_cycle;
        ESP_LOGD(__FILE__, "Deploy Pulse: Accumulated demand: %.2f m", _accumulatedDeployDemand);


        // --- 4. Decide Whether to Deploy Now or Wait ---
        float actual_deploy_amount = 0.0;
        unsigned long next_pulse_delay_ms = DECISION_WINDOW_MS; // Default to waiting

        // Condition 1: Do we have enough accumulated demand?
        bool hasEnoughDemand = (_accumulatedDeployDemand >= MIN_DEPLOY_THRESHOLD_M);
        // Condition 2: Is there excessive positive slack?
        bool hasExcessSlack = (current_horizontal_slack > dynamic_max_acceptable_slack);

        if (hasEnoughDemand && !hasExcessSlack) {
            // Normal path: deploy the accumulated amount
            actual_deploy_amount = _accumulatedDeployDemand;
            _accumulatedDeployDemand = 0.0; // Reset accumulated demand after deploying

        // Don't overshoot the stage's total target
        float remaining_to_stage_target = stageTargetChainLength - current_chain_length;
        if (actual_deploy_amount > remaining_to_stage_target) {
            actual_deploy_amount = remaining_to_stage_target;
        }


        // Command the ChainController if actual_deploy_amount is significant
        if (actual_deploy_amount >= 0.01) { // Min 1cm payout
            ESP_LOGI(__FILE__, "Deploy Pulse: Triggering payout (Demand %.2f m, Slack %.2f m). Commanding lowerAnchor by %.2f m. Boat speed: %.2f m/s, Windlass rate: %.2f m/s (%.0f ms/m)",
                        actual_deploy_amount, current_horizontal_slack, actual_deploy_amount, current_boat_speed_mps, windlass_deployment_rate_mps, this->chainController->getDownSpeed());

            this->chainController->lowerAnchor(actual_deploy_amount);
            _lastDistanceAtDeploymentCommand = this->chainController->getCurrentDistance(); // Or this->ewmaDistance;

            // Schedule next pulse based on windlass operation time
            next_pulse_delay_ms = (unsigned long)(actual_deploy_amount * this->chainController->getDownSpeed()) + 200; // Windlass time + buffer
        } else {
            ESP_LOGD(__FILE__, "Deploy Pulse: Accumulated demand (%.2f m) below minimum effective payout (0.01 m). Resetting accumulated and waiting.", actual_deploy_amount);
            _accumulatedDeployDemand = 0.0; // Reset even if too small to avoid constant tiny attempts
        }

        } else if (hasExcessSlack) {
        // Too much slack, even if boat is moving back or we have demand. PAUSE DEPLOYMENT.
        ESP_LOGI(__FILE__, "Deploy Pulse: Excessive slack detected (%.2f m > %.2f m). Pausing deployment until slack reduces. Accumulated demand: %.2f m",
                    current_horizontal_slack, dynamic_max_acceptable_slack, _accumulatedDeployDemand);
        // next_pulse_delay_ms remains DECISION_WINDOW_MS (already set)
        } else {
            // Not enough accumulated demand yet. Wait for next decision window.
            ESP_LOGD(__FILE__, "Deploy Pulse: Not enough accumulated demand (%.2f m < %.2f m) and no excessive slack. Waiting. Next check in %lu ms.",
                     _accumulatedDeployDemand, MIN_DEPLOY_THRESHOLD_M, DECISION_WINDOW_MS);
            // next_pulse_delay_ms remains DECISION_WINDOW_MS (already set)
        }
        
        // Ensure a minimum delay
        if (next_pulse_delay_ms < DECISION_WINDOW_MS) {
            next_pulse_delay_ms = DECISION_WINDOW_MS;
        }

        ESP_LOGD(__FILE__, "Deploy Pulse: Next pulse scheduled in %lu ms.", next_pulse_delay_ms);
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
      ESP_LOGI(__FILE__, "DeploymentManager: In DEPLOY_30 stage. Chain: %.2f m, Target: %.2f m", currentChainLength, chain30);

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
      ESP_LOGI(__FILE__, "DeploymentManager: In DEPLOY_75 stage. Chain: %.2f m, Target: %.2f m", currentChainLength, chain75);
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
      ESP_LOGI(__FILE__, "DeploymentManager: In DEPLOY_100 stage. Chain: %.2f m, Target: %.2f m", currentChainLength, totalChainLength);
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

