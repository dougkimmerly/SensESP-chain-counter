#include "ChainController.h"
#include <Arduino.h>     // For pinMode, digitalWrite, millis, etc.
#include <Preferences.h> // For saving/loading speeds
#include <cmath>         // For pow, sqrt, fabs, isnan, isinf

// ============================================================================
// Utility: computeTargetHorizontalDistance
// This calculates the horizontal distance accounting for catenary sag,
// using chain weight and horizontal force from wind/current.
// ============================================================================
float ChainController::computeTargetHorizontalDistance(float chainLength, float depth) {
    // Guard against NaN/Inf inputs directly
    if (isnan(chainLength) || isinf(chainLength) || isnan(depth) || isinf(depth)) {
        ESP_LOGE(__FILE__, "ChainController::computeTargetHorizontalDistance: NaN/Inf input detected! chainLength=%.2f, depth=%.2f. Returning 0.0", chainLength, depth);
        return 0.0;
    }

    // Mathematically, chainLength must be >= depth for a real solution
    float arg = pow(chainLength, 2) - pow(depth, 2);
    if (arg < 0.0) {
        ESP_LOGW(__FILE__, "ChainController::computeTargetHorizontalDistance: Negative argument for sqrt! chainLength=%.2f, depth=%.2f, arg=%.2f. This usually means chainLength < depth. Returning 0.0", chainLength, depth, arg);
        return 0.0;
    }

    // Get estimated horizontal force for catenary calculation
    float horizontalForce = estimateHorizontalForce();

    // Calculate straight-line distance (Pythagorean)
    float straightLineDistance = sqrt(arg);

    // Apply catenary reduction factor to account for chain sag
    float reductionFactor = computeCatenaryReductionFactor(chainLength, depth, horizontalForce);

    return straightLineDistance * reductionFactor;
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
    distanceListener_(new sensesp::SKValueListener<float>("navigation.anchor.distanceFromBow", 2000, "/distance/sk")),
    windSpeedListener_(new sensesp::SKValueListener<float>("environment.wind.speedTrue", 30000, "/wind/sk")),  // 30s - only for catenary estimate
    tideHeightNowListener_(new sensesp::SKValueListener<float>("environment.tide.heightNow", 60000, "/tide/heightNow/sk")),  // 60s - tide changes slowly
    tideHeightHighListener_(new sensesp::SKValueListener<float>("environment.tide.heightHigh", 300000, "/tide/heightHigh/sk"))  // 5min - rarely changes
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
                ESP_LOGD(__FILE__, "control: target reached (lowering), stopping at %.2f m.", current_pos);
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
                ESP_LOGD(__FILE__, "control: target reached (raising), stopping at %.2f m.", current_pos);
            } else {
                digitalWrite(upRelayPin_, HIGH); // Keep relay HIGH if still raising
            }
            break;
    }
}

void ChainController::stop() {
    if(state_ == ChainState::IDLE) {
        ESP_LOGD(__FILE__, "stop() called but already IDLE.");
        return;
    }
    digitalWrite(upRelayPin_, LOW);
    digitalWrite(downRelayPin_, LOW);
    calcSpeed(movement_start_time_, start_position_); // Calculate speed for the movement that was stopped
    state_ = ChainState::IDLE;
    ESP_LOGD(__FILE__, "stop: all relays off, state IDLE.");
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
        // ESP_LOGI(__FILE__, "Loaded speeds from prefs: upSpeed=%.2f ms/m, downSpeed=%.2f ms/m", upSpeed_, downSpeed_);
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
        // ESP_LOGI(__FILE__, "Saved speeds to prefs: upSpeed=%.2f ms/m, downSpeed=%.2f ms/m", upSpeed_, downSpeed_);
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
            // ESP_LOGI(__FILE__, "Updated %s speed: %.2f ms/m (raw %.2f ms/m)",
            //          (state_ == ChainState::LOWERING ? "down" : "up"), *target_speed_ptr, raw_speed_ms_per_m);
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
        // ESP_LOGI(__FILE__, "updateTimeout(): Expected duration=%.0f ms (for %.2f m at %.2f ms/m). Actual timeout set to %lu ms.",
        //          (float)expected_time_ms, distance, speed_ms_per_m, move_timeout_);
    } else {
        move_timeout_ = 10000; // Default timeout if speed/distance are invalid (e.g., 10 seconds)
        // ESP_LOGW(__FILE__, "updateTimeout(): Invalid speed (%.2f ms/m) or distance (%.2f m) for timeout calculation. Using default %lu ms.",
        //          speed_ms_per_m, distance, move_timeout_);
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
        // ESP_LOGD(__FILE__, "ChainController: getCurrentDepth() returning 0.0, depthListener has no valid data (%.2f).", depth);
        return 0.0;
    }
    return depth;
}

float ChainController::getCurrentDistance() const {
    float distance = distanceListener_->get(); // Get the current value
    // If the listener has never received a value, or returns NaN/Inf/a very small number
    if (isnan(distance) || isinf(distance) || distance <= 0.01) { // Consider 0.01 as effectively zero for distance
        return 0.0;
    }
    return distance;
}

float ChainController::getTideHeightNow() const {
    float tideNow = tideHeightNowListener_->get();
    if (isnan(tideNow) || isinf(tideNow)) {
        return 0.0;
    }
    return tideNow;
}

float ChainController::getTideHeightHigh() const {
    float tideHigh = tideHeightHighListener_->get();
    if (isnan(tideHigh) || isinf(tideHigh)) {
        return 0.0;
    }
    return tideHigh;
}

float ChainController::getTideAdjustedDepth() const {
    float currentDepth = getCurrentDepth();
    float tideNow = getTideHeightNow();
    float tideHigh = getTideHeightHigh();

    // If tide data unavailable (both return 0), use current depth
    if (tideNow == 0.0 && tideHigh == 0.0) {
        return currentDepth;
    }

    // Adjust depth for high tide: depth_at_high_tide = current_depth - tide_now + tide_high
    float adjustedDepth = currentDepth - tideNow + tideHigh;

    // Ensure adjusted depth is not negative
    return fmax(0.0, adjustedDepth);
}

void ChainController::calculateAndPublishHorizontalSlack() {
    float current_chain = getChainLength();
    float current_depth = getCurrentDepth();
    float current_distance = getCurrentDistance();

    // Account for bow height (2m above water) when calculating effective depth for catenary math
    static constexpr float BOW_HEIGHT_M = 2.0;
    float effective_depth = fmax(0.0, current_depth - BOW_HEIGHT_M);

    float calculated_slack = 0.0; // Initialize for final result
    float horizontal_distance_taut = 0.0; // Initialize

    // --- Start Robust Validation for Input Data Sanity ---
    // We need chain and depth to calculate slack. Distance can be 0 (boat directly over anchor).
    // Note: Slack = horizontal_distance_taut - current_distance
    // If distance=0, slack = all of the horizontal_distance_taut (chain sag on seabed)
    if (current_chain <= 0.01 || current_depth <= 0.01 || isnan(current_chain) || isinf(current_chain) || isnan(current_depth) || isinf(current_depth) || isnan(current_distance) || isinf(current_distance)) {
        calculated_slack = 0.0;
    }
    // --- Check if anchor has touched bottom ---
    // If chain length < total distance from bow to seabed (bow_height + water_depth),
    // the anchor is still falling and chain hangs vertically - there is no horizontal slack yet.
    // Total drop distance = BOW_HEIGHT_M (bow to water) + current_depth (water to seabed)
    else if (current_chain < current_depth + BOW_HEIGHT_M) {
        calculated_slack = 0.0;
    }
    // --- End Robust Validation for Input Data Sanity ---
    else {
        // Anchor is on bottom - calculate slack as chain lying on seabed.
        //
        // SLACK CALCULATION APPROACH:
        // Slack = total_chain - chain_needed_to_reach_current_position
        //
        // The chain needed to reach the anchor at current_distance is at minimum
        // the straight-line distance from bow to anchor (Pythagorean theorem).
        // In reality, catenary sag means we need slightly MORE chain, but for
        // slack calculation we use the minimum (straight line) to be conservative.
        //
        // total_depth_from_bow = BOW_HEIGHT_M + current_depth (bow to seabed)
        // minimum_chain_needed = sqrt(current_distance² + total_depth_from_bow²)
        // slack = current_chain - minimum_chain_needed

        float total_depth_from_bow = BOW_HEIGHT_M + current_depth;

        // If distance is unavailable (0.0), minimum chain is just vertical drop
        float minimum_chain_needed;
        if (current_distance <= 0.01) {
            minimum_chain_needed = total_depth_from_bow;
        } else {
            // Straight-line distance from bow to anchor on seabed
            minimum_chain_needed = sqrt(pow(current_distance, 2) + pow(total_depth_from_bow, 2));
        }

        // Slack is excess chain beyond what's needed
        // Positive = chain lying on seabed
        // Negative = anchor has dragged (boat further than chain can reach)
        calculated_slack = current_chain - minimum_chain_needed;

        // Check for NaN/Inf in the final result
        if (isnan(calculated_slack) || isinf(calculated_slack)) {
             calculated_slack = 0.0;
        }
    }

    // --- Update the ObservableValue (only if significantly changed) ---
    if (fabs(horizontalSlack_->get() - calculated_slack) > 0.01) { // 1cm tolerance
        horizontalSlack_->set(calculated_slack);
        // ESP_LOGD(__FILE__, "ChainController: Horizontal Slack ObservableValue set to %.2f m", calculated_slack);
    } else {
        // ESP_LOGD(__FILE__, "ChainController: Horizontal Slack calculated (%.2f m) but no significant change. Not updating observable.", calculated_slack);
    }
}

// ============================================================================
// Catenary Physics Calculations
// ============================================================================

float ChainController::estimateHorizontalForce() {
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

    return totalForce;
}

float ChainController::computeCatenaryReductionFactor(float chainLength, float anchorDepth, float horizontalForce) {
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

    // ESP_LOGD(__FILE__, "Catenary calc: chainLen=%.2f, depth=%.2f, force=%.0fN, a=%.2f, sagReduction=%.2f, factor=%.3f",
    //          chainLength, anchorDepth, horizontalForce, a, catenarySagReduction, reductionFactor);

    return reductionFactor;
}

