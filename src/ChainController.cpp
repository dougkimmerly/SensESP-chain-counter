#include "ChainController.h"
#include <Arduino.h> // or your environment's main include
#include <Preferences.h>

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
    state_(ChainState::IDLE) {}

void ChainController::lowerAnchor(float amount) {
    float current = accumulator_->get();
    start_position_ = current;
    movement_start_time_ = millis();
        ESP_LOGI(__FILE__, "lowerAnchor() called, start_time=%lu, start_pos=%f", 
         movement_start_time_, start_position_);    
    // Set target
    target_ = current + amount;
    if (target_ > stop_before_max_) target_ = stop_before_max_;
    if (target_ < min_length_) target_ = min_length_;
    updateTimeout(amount, downSpeed_);
    // Set state to lowering
    state_ = ChainState::LOWERING;

    // Activate relay immediately
    digitalWrite(downRelayPin_, HIGH);
    digitalWrite(upRelayPin_, LOW);

    ESP_LOGI(__FILE__, "lowerAnchor: lowering to %f", target_);

    // Ensure control reacts instantly
    control(current);
}

void ChainController::raiseAnchor(float amount) {
    float current = accumulator_->get();
    start_position_ = current;
    movement_start_time_ = millis();
    
    // Set target
    target_ = current - amount; // raising decreases height
    if (target_ < min_length_) target_ = min_length_;
    if (target_ > max_length_) target_ = max_length_; // optional
    updateTimeout(amount, upSpeed_);
    // Set state to raising
    state_ = ChainState::RAISING;

    // Activate relay immediately
    digitalWrite(upRelayPin_, HIGH);
    digitalWrite(downRelayPin_, LOW);

    ESP_LOGI(__FILE__, "raiseAnchor: raising to %f", target_);

    // React immediately
    control(current);
}

void ChainController::control(float current_pos) {
    if (state_ == ChainState::IDLE) return;

    switch (state_) {
        case ChainState::LOWERING:
        // ESP_LOGI(__FILE__, "control() called, start_time=%lu, start_pos=%f", 
        //  movement_start_time_, start_position_);
            if (current_pos >= target_ || current_pos >= stop_before_max_) {
                digitalWrite(downRelayPin_, LOW);
                calcSpeed(movement_start_time_, start_position_);
                state_ = ChainState::IDLE;
                ESP_LOGI(__FILE__, "control: target reached (lowering), stop");
            } else {
                digitalWrite(downRelayPin_, HIGH);
            }
            break;

        case ChainState::RAISING:
            if (current_pos <= target_ || current_pos <= min_length_) {
                digitalWrite(upRelayPin_, LOW);
                calcSpeed(movement_start_time_, start_position_);
                state_ = ChainState::IDLE;
                ESP_LOGI(__FILE__, "control: target reached (raising), stop");
            } else {
                digitalWrite(upRelayPin_, HIGH);
            }
            break;
    }
}

void ChainController::stop() {
    if(state_ == ChainState::IDLE) {
        ESP_LOGI(__FILE__, "stop() called but already IDLE");
        return;
    }
    digitalWrite(upRelayPin_, LOW);
    digitalWrite(downRelayPin_, LOW);
    calcSpeed(movement_start_time_, start_position_);
    state_ = ChainState::IDLE;
    ESP_LOGI(__FILE__, "stop: all relays off");
}

bool ChainController::isActive() const {
    return state_ != ChainState::IDLE;
}
unsigned long ChainController::getTimeout() const {
    return move_timeout_;
}

void ChainController::loadSpeedsFromPrefs() {
    Preferences prefs;
    if (prefs.begin("speeds", true)) {  // true = read-only
        upSpeed_ = prefs.getFloat("upSpeed", 1000.0);    // default 1000 ms/m
        downSpeed_ = prefs.getFloat("downSpeed", 1000.0);
        prefs.end();
        ESP_LOGI(__FILE__, "Loaded speeds from prefs: upSpeed=%f ms/m, downSpeed=%f ms/m", upSpeed_, downSpeed_);   
    } else {
        // If begin() fails, we skip loading; keep defaults
        ESP_LOGW(__FILE__, "Preferences could not be opened");
    }
}

// Save speeds to preferences
void ChainController::saveSpeedsToPrefs() {
    Preferences prefs;
    prefs.begin("speeds", false);
    prefs.putFloat("upSpeed", upSpeed_);
    prefs.putFloat("downSpeed", downSpeed_);
    prefs.end();
}
void ChainController::calcSpeed(unsigned long start_time, float start_position) {
    unsigned long now = millis();
    float current_position = accumulator_->get();
    unsigned long duration_ms = now - start_time;
    float delta_distance = current_position - start_position;

    if (delta_distance != 0 && duration_ms > 0) {
        // raw speed: ms per meter (smaller is faster)
        float raw_speed = (float)duration_ms / fabsf(delta_distance);
        float* target_speed = nullptr;

        if (state_ == ChainState::LOWERING)
            target_speed = &downSpeed_;
        else if (state_ == ChainState::RAISING)
            target_speed = &upSpeed_;

        // Exponential smoothing
        *target_speed = smoothing_factor_ * raw_speed + (1 - smoothing_factor_) * (*target_speed);

        // Save new speed in preferences
        saveSpeedsToPrefs();
        movement_start_time_= 0.0;
        start_position_ = 0.0;
        ESP_LOGI(__FILE__, "Updated speed: %f ms/m", *target_speed);
    }
}
void ChainController::updateTimeout(float distance, float speed) {
    unsigned long expected_time_ms = (unsigned long)(distance * speed);
    move_timeout_ = expected_time_ms + 5000; // 5s buffer
    ESP_LOGI(__FILE__, "updateTimeout(): duration=%lu ms", move_timeout_);
}

float ChainController::getChainLength() const {
    return accumulator_->get();
}
