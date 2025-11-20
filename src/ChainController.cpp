#include "ChainController.h"
#include <Arduino.h>     // For pinMode, digitalWrite, millis, etc.
#include <Preferences.h> // For saving/loading speeds
#include <cmath>         // For pow, sqrt, fabs, isnan, isinf

// ============================================================================
// Utility: computeTargetHorizontalDistance
// This calculates the horizontal distance if the chain were taut, given its
// length and the depth it's hanging in. It includes robust checks.
// ============================================================================
float ChainController::computeTargetHorizontalDistance(float chainLength, float depth) {
    // Guard against NaN/Inf inputs directly
    if (isnan(chainLength) || isinf(chainLength) || isnan(depth) || isinf(depth)) {
        ESP_LOGE(__FILE__, "ChainController::computeTargetHorizontalDistance: NaN/Inf input detected! chainLength=%.2f, depth=%.2f. Returning 0.0", chainLength, depth);
        return 0.0;
    }

    // Mathematically, chainLength must be >= depth for a real solution
    // If chainLength < depth, the value under sqrt would be negative.
    // Also, guard against floating point inaccuracies that might make arg very slightly negative.
    float arg = pow(chainLength, 2) - pow(depth, 2);
    if (arg < 0.0) { // Using 0.0 as threshold; a slightly more robust check might use a small epsilon like -0.001
        ESP_LOGW(__FILE__, "ChainController::computeTargetHorizontalDistance: Negative argument for sqrt! chainLength=%.2f, depth=%.2f, arg=%.2f. This usually means chainLength < depth. Returning 0.0", chainLength, depth, arg);
        return 0.0;
    }
    return sqrt(arg);
}


// ============================================================================
// Constructor
// ============================================================================
ChainController::ChainController(
                float min_length,
                float max_length,
                float stop_before_max,
                sensesp::Integrator<float, float>* acc,
                int downRelayPin,
                int upRelayPin
            )
  : min_length_(min_length),
    max_length_(max_length),
    stop_before_max_(stop_before_max),
    accumulator_(acc),
    downRelayPin_(downRelayPin),
    upRelayPin_(upRelayPin),
    state_(ChainState::IDLE), // Default state
    horizontalSlack_(new sensesp::ObservableValue<float>(0.0)),
    depthListener_(new sensesp::SKValueListener<float>("environment.depth.belowSurface", 2000, "/depth/sk")),
    distanceListener_(new sensesp::SKValueListener<float>("navigation.anchor.distanceFromBow", 2000, "/distance/sk"))
{
    // Ensure relays are off at startup. PinMode setup should happen in main.cpp.
    digitalWrite(upRelayPin_, LOW);
    digitalWrite(downRelayPin_, LOW);
    ESP_LOGI(__FILE__, "ChainController initialized. UpRelay: %d, DownRelay: %d.", upRelayPin_, downRelayPin_);
}


// ============================================================================
// Anchor Control Methods
// ============================================================================

void ChainController::lowerAnchor(float amount) {
    float current = accumulator_->get();
    start_position_ = current;
    movement_start_time_ = millis();
    ESP_LOGI(__FILE__, "lowerAnchor() called, start_time=%lu, start_pos=%.2f",
         movement_start_time_, start_position_);

    // Set target: amount is relative, target_ becomes absolute
    target_ = current + amount;

    // Apply limits:
    if (target_ > max_length_) {
        ESP_LOGW(__FILE__, "lowerAnchor: Requested target %.2f m exceeds max_length_ %.2f m. Limiting target.", target_, max_length_);
        target_ = max_length_;
    }
    // Also limit by stop_before_max_ if it's set to be less than max_length_
    if (target_ > stop_before_max_) {
        ESP_LOGW(__FILE__, "lowerAnchor: Requested target %.2f m exceeds stop_before_max_ %.2f m. Limiting target.", target_, stop_before_max_);
        target_ = stop_before_max_;
    }
    if (target_ < min_length_) { // Should not be an issue for lowering, but defensive
        ESP_LOGW(__FILE__, "lowerAnchor: Requested target %.2f m falls below min_length_ %.2f m. Limiting target.", target_, min_length_);
        target_ = min_length_;
    }

    updateTimeout(amount, downSpeed_); // Use the requested 'amount' for timeout calculation
    state_ = ChainState::LOWERING;

    // Activate relay immediately
    digitalWrite(downRelayPin_, HIGH);
    digitalWrite(upRelayPin_, LOW);

    ESP_LOGI(__FILE__, "lowerAnchor: lowering to absolute target %.2f m (requested %.2f m from current %.2f m)", target_, amount, current);

    // Ensure control reacts instantly. This will immediately check if target is met.
    control(current);
}

void ChainController::raiseAnchor(float amount) {
    float current = accumulator_->get();
    start_position_ = current;
    movement_start_time_ = millis();
    ESP_LOGI(__FILE__, "raiseAnchor() called, start_time=%lu, start_pos=%.2f",
         movement_start_time_, start_position_);

    // Set target: amount is relative, target_ becomes absolute
    target_ = current - amount; // raising decreases height

    // Apply limits:
    if (target_ < min_length_) { // Min_length_ usually 0 for chain on deck
        ESP_LOGW(__FILE__, "raiseAnchor: Requested target %.2f m falls below min_length_ %.2f m. Limiting target.", target_, min_length_);
        target_ = min_length_;
    }
    if (target_ > max_length_) { // Should not be an issue for raising, but defensive
        ESP_LOGW(__FILE__, "raiseAnchor: Requested target %.2f m exceeds max_length_ %.2f m. Limiting target.", target_, max_length_);
        target_ = max_length_;
    }

    updateTimeout(amount, upSpeed_); // Use the requested 'amount' for timeout calculation
    state_ = ChainState::RAISING;

    // Activate relay immediately
    digitalWrite(upRelayPin_, HIGH);
    digitalWrite(downRelayPin_, LOW);

    ESP_LOGI(__FILE__, "raiseAnchor: raising to absolute target %.2f m (requested %.2f m from current %.2f m)", target_, amount, current);

    // React immediately
    control(current);
}

void ChainController::control(float current_pos) {
    if (state_ == ChainState::IDLE) return;

    switch (state_) {
        case ChainState::LOWERING:
            // Check if current position has reached or exceeded the target.
            // Also checking against stop_before_max_ ensures a stop if that limit is hit.
            if (current_pos >= target_ || current_pos >= stop_before_max_) {
                digitalWrite(downRelayPin_, LOW);
                calcSpeed(movement_start_time_, start_position_);
                state_ = ChainState::IDLE;
                ESP_LOGI(__FILE__, "control: target reached (lowering), stopping at %.2f m.", current_pos);
            } else {
                digitalWrite(downRelayPin_, HIGH); // Keep relay HIGH if still lowering
            }
            break;

        case ChainState::RAISING:
            // Check if current position has reached or fallen below the target.
            // Also checking against min_length_ ensures a stop if that limit is hit.
            if (current_pos <= target_ || current_pos <= min_length_) {
                digitalWrite(upRelayPin_, LOW);
                calcSpeed(movement_start_time_, start_position_);
                state_ = ChainState::IDLE;
                ESP_LOGI(__FILE__, "control: target reached (raising), stopping at %.2f m.", current_pos);
            } else {
                digitalWrite(upRelayPin_, HIGH); // Keep relay HIGH if still raising
            }
            break;
    }
}

void ChainController::stop() {
    if(state_ == ChainState::IDLE) {
        ESP_LOGI(__FILE__, "stop() called but already IDLE.");
        return;
    }
    digitalWrite(upRelayPin_, LOW);
    digitalWrite(downRelayPin_, LOW);
    calcSpeed(movement_start_time_, start_position_); // Calculate speed for the movement that was stopped
    state_ = ChainState::IDLE;
    ESP_LOGI(__FILE__, "stop: all relays off, state IDLE.");
}

bool ChainController::isActive() const {
    return state_ != ChainState::IDLE;
}


// ============================================================================
// Speed Calculation and Persistence
// ============================================================================

unsigned long ChainController::getTimeout() const {
    return move_timeout_;
}

void ChainController::loadSpeedsFromPrefs() {
    Preferences prefs;
    if (prefs.begin("speeds", true)) {  // true = read-only
        upSpeed_ = prefs.getFloat("upSpeed", 1000.0);    // default 1000 ms/m
        downSpeed_ = prefs.getFloat("downSpeed", 1000.0);
        prefs.end();
        ESP_LOGI(__FILE__, "Loaded speeds from prefs: upSpeed=%.2f ms/m, downSpeed=%.2f ms/m", upSpeed_, downSpeed_);
    } else {
        // If begin() fails, we skip loading; keep defaults
        ESP_LOGW(__FILE__, "Preferences could not be opened for reading speeds.");
    }
}

void ChainController::saveSpeedsToPrefs() {
    Preferences prefs;
    if (prefs.begin("speeds", false)) { // false = writable
        prefs.putFloat("upSpeed", upSpeed_);
        prefs.putFloat("downSpeed", downSpeed_);
        prefs.end();
        ESP_LOGI(__FILE__, "Saved speeds to prefs: upSpeed=%.2f ms/m, downSpeed=%.2f ms/m", upSpeed_, downSpeed_);
    } else {
        ESP_LOGE(__FILE__, "Preferences could not be opened for writing speeds.");
    }
}

void ChainController::calcSpeed(unsigned long start_time, float start_position) {
    if (start_time == 0) return; // No movement recorded if start_time is 0
    
    unsigned long now = millis();
    float current_position = accumulator_->get();
    unsigned long duration_ms = now - start_time;
    float delta_distance = current_position - start_position;

    // Only calculate if significant movement (>= 1cm) and a measurable duration (>= 100ms)
    if (fabs(delta_distance) >= 0.01 && duration_ms >= 100) { 
        float raw_speed_ms_per_m = (float)duration_ms / fabs(delta_distance);
        float* target_speed_ptr = nullptr;

        if (state_ == ChainState::LOWERING) {
            target_speed_ptr = &downSpeed_;
        } else if (state_ == ChainState::RAISING) {
            target_speed_ptr = &upSpeed_;
        }

        if (target_speed_ptr != nullptr) {
            // Exponential smoothing
            *target_speed_ptr = smoothing_factor_ * raw_speed_ms_per_m + (1 - smoothing_factor_) * (*target_speed_ptr);
            saveSpeedsToPrefs();
            ESP_LOGI(__FILE__, "Updated %s speed: %.2f ms/m (raw %.2f ms/m)",
                     (state_ == ChainState::LOWERING ? "down" : "up"), *target_speed_ptr, raw_speed_ms_per_m);
        }
    }
    // Reset for next movement
    movement_start_time_ = 0; 
    start_position_ = 0.0;
}

void ChainController::updateTimeout(float distance, float speed_ms_per_m) {
    // Only calculate timeout if speed is valid and distance is positive
    if (speed_ms_per_m > 0.01 && distance > 0.01) {
        unsigned long expected_time_ms = (unsigned long)(distance * speed_ms_per_m);
        move_timeout_ = expected_time_ms + 5000; // 5s buffer
        ESP_LOGI(__FILE__, "updateTimeout(): Expected duration=%.0f ms (for %.2f m at %.2f ms/m). Actual timeout set to %lu ms.",
                 (float)expected_time_ms, distance, speed_ms_per_m, move_timeout_);
    } else {
        move_timeout_ = 10000; // Default timeout if speed/distance are invalid (e.g., 10 seconds)
        ESP_LOGW(__FILE__, "updateTimeout(): Invalid speed (%.2f ms/m) or distance (%.2f m) for timeout calculation. Using default %lu ms.",
                 speed_ms_per_m, distance, move_timeout_);
    }
}

float ChainController::getChainLength() const {
    return accumulator_->get();
}


// ============================================================================
// Horizontal Slack Calculation Methods
// ============================================================================

float ChainController::getCurrentDepth() const {
    float depth = depthListener_->get(); 
    // If the listener has never received a value, or returns NaN/Inf/a very small number
    if (isnan(depth) || isinf(depth) || depth <= 0.01) { // Consider 0.01 as effectively zero for depth
        ESP_LOGD(__FILE__, "ChainController: getCurrentDepth() returning 0.0, depthListener has no valid data (%.2f).", depth);
        return 0.0;
    }
    return depth;
}

float ChainController::getCurrentDistance() const {
    float distance = distanceListener_->get(); // Get the current value
    // If the listener has never received a value, or returns NaN/Inf/a very small number
    if (isnan(distance) || isinf(distance) || distance <= 0.01) { // Consider 0.01 as effectively zero for distance
        ESP_LOGD(__FILE__, "ChainController: getCurrentDistance() returning 0.0, distanceListener has no valid data (%.2f).", distance);
        return 0.0;
    }
    return distance;
}

void ChainController::calculateAndPublishHorizontalSlack() {
    float current_chain = getChainLength();
    float current_depth = getCurrentDepth();
    float current_distance = getCurrentDistance();

    ESP_LOGD(__FILE__, "SLACK CALC DEBUG: Inputs: Chain=%.2f, Depth=%.2f, Dist=%.2f", current_chain, current_depth, current_distance);

    float calculated_slack = 0.0; // Initialize for final result
    float horizontal_distance_taut = 0.0; // Initialize

    // --- Start Robust Validation for Input Data Sanity (not physical impossibility for the slack value itself) ---
    // If any input is essentially zero or invalid, we cannot compute meaningful slack.
    if (current_chain <= 0.01 || current_depth <= 0.01 || current_distance <= 0.01 || isnan(current_chain) || isinf(current_chain) || isnan(current_depth) || isinf(current_depth) || isnan(current_distance) || isinf(current_distance)) {
        ESP_LOGW(__FILE__, "ChainController: Invalid (zero/NaN/Inf) inputs for slack calculation. Chain=%.2f, Depth=%.2f, Dist=%.2f. Setting slack to 0.0.", current_chain, current_depth, current_distance);
        calculated_slack = 0.0;
    }
    // --- End Robust Validation for Input Data Sanity ---
    else {
        // If inputs are sane, proceed with calculation.
        // computeTargetHorizontalDistance already handles chainLength < depth by returning 0.0,
        // which will then correctly lead to a large negative slack: (0.0 - current_distance)
        horizontal_distance_taut = computeTargetHorizontalDistance(current_chain, current_depth);
        
        // --- CORRECTED SLACK CALCULATION (allowing negative values) ---
        calculated_slack = horizontal_distance_taut - current_distance;
        // --- END CORRECTION ---

        // Check for NaN/Inf in the final result (should be caught earlier, but defensive check)
        if (isnan(calculated_slack) || isinf(calculated_slack)) {
             ESP_LOGE(__FILE__, "ChainController: Slack calculated as NaN/Inf despite sane inputs. Chain=%.2f, Depth=%.2f, Dist=%.2f. Setting slack to 0.0.", current_chain, current_depth, current_distance);
             calculated_slack = 0.0;
        }
        ESP_LOGD(__FILE__, "SLACK CALC DEBUG: TautHorizDist (based on Chain): %.2f m", horizontal_distance_taut);
        ESP_LOGD(__FILE__, "SLACK CALC DEBUG: Calculated Slack (TautHorizDist - Dist): %.2f m", calculated_slack);
    }

    // --- Update the ObservableValue (only if significantly changed) ---
    if (fabs(horizontalSlack_->get() - calculated_slack) > 0.01) { // 1cm tolerance
        horizontalSlack_->set(calculated_slack);
        ESP_LOGD(__FILE__, "ChainController: Horizontal Slack ObservableValue set to %.2f m", calculated_slack);
    } else {
        ESP_LOGD(__FILE__, "ChainController: Horizontal Slack calculated (%.2f m) but no significant change. Not updating observable.", calculated_slack);
    }
}

