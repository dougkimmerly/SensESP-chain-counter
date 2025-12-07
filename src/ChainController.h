// ChainController.h
#ifndef CHAINCONTROLLER_H
#define CHAINCONTROLLER_H

#include <Arduino.h>
#include "sensesp/transforms/linear.h"
#include "sensesp_app.h"
#include "sensesp/signalk/signalk_value_listener.h"

enum class ChainState {
    IDLE,
    LOWERING,
    RAISING
};

class ChainController {
public:
    ChainController(
        float min_length,
        float max_length,
        float stop_before_max,
        sensesp::Integrator<float, float>* acc,
        int downRelayPin,
        int upRelayPin
    );
    void lowerAnchor(float amount);
    void raiseAnchor(float amount);

    void stop();
    bool isActive() const;
    void control(float current_pos);
    void calcSpeed(unsigned long start_time, float start_position);
    void loadSpeedsFromPrefs();
    void saveSpeedsToPrefs();
    unsigned long getTimeout() const;
    float getChainLength() const;
    float getDownSpeed() const { return downSpeed_; }

    void setDepthBelowSurface(float depth);
    void setDistanceBowToAnchor(float distance);


    float getCurrentDepth() const; 
    float getCurrentDistance() const;

    void calculateAndPublishHorizontalSlack();

    // Method to allow the calculation logic (in main.cpp) to update the slack value
    

    // Getter to provide external access to the ObservableValue for Signal K connection in main.cpp
    sensesp::ObservableValue<float>* getHorizontalSlackObservable() const {
        return horizontalSlack_;
    }

    sensesp::SKValueListener<float>* getDepthListener() const { return depthListener_; }
    sensesp::SKValueListener<float>* getDistanceListener() const { return distanceListener_; }
    sensesp::SKValueListener<float>* getTideHeightNowListener() const { return tideHeightNowListener_; }
    sensesp::SKValueListener<float>* getTideHeightHighListener() const { return tideHeightHighListener_; }

    // Tide-related getters
    float getTideHeightNow() const;
    float getTideHeightHigh() const;
    float getTideAdjustedDepth() const;

    // Public method to check if controller is actively controlling the windlass
    bool isActivelyControlling() const { return state_ != ChainState::IDLE; }

    // Public catenary physics method for use by DeploymentManager
    float computeTargetHorizontalDistance(float chainLength, float depth);

    // Slack monitoring constants
    static constexpr float PAUSE_SLACK_M = 0.2;                      // Pause raising when slack drops below this
    static constexpr float RESUME_SLACK_M = 1.0;                     // Resume raising when slack builds to this
    static constexpr unsigned long SLACK_COOLDOWN_MS = 3000;         // 3s cooldown between pause/resume actions
    static constexpr float BOW_HEIGHT_M = 2.0;                       // Height from bow roller to water surface
    static constexpr float FINAL_PULL_THRESHOLD_M = 3.0;             // When rode < depth + bow + threshold, skip slack checks

private:
    float min_length_;
    float max_length_;
    float stop_before_max_;
    sensesp::Integrator<float, float>* accumulator_;
    int downRelayPin_;
    int upRelayPin_;
    float target_;
    ChainState state_;
    unsigned long movement_start_time_;
    float start_position_= 0.0;
    float move_speed_ms_per_m_;
    unsigned long move_timeout_;

    void updateTimeout(float, float);

    float upSpeed_ = 1000.0;   // default 1 sec per meter
    float downSpeed_ = 1000.0; // default 1 sec per meter
    const float smoothing_factor_ = 0.2;

    // Slack monitoring state
    bool paused_for_slack_ = false;
    unsigned long last_slack_action_time_ = 0;

    sensesp::SKValueListener<float>* depthListener_;
    sensesp::SKValueListener<float>* distanceListener_;
    sensesp::SKValueListener<float>* windSpeedListener_;  // Wind speed for catenary calculations
    sensesp::SKValueListener<float>* tideHeightNowListener_;   // Current tide height
    sensesp::SKValueListener<float>* tideHeightHighListener_;  // High tide height
    sensesp::ObservableValue<float>* horizontalSlack_; // The ObservableValue for the calculated slack

    void updateHorizontalSlack(float slack);

    // Catenary physics calculation methods (private helpers)
    float computeCatenaryReductionFactor(float chainLength, float anchorDepth, float horizontalForce);
    float estimateHorizontalForce();  // Estimate force from wind/current

    // Chain and boat physical constants
    static constexpr float CHAIN_WEIGHT_PER_METER_KG = 2.2;  // kg/m in water (adjusted for buoyancy)
    static constexpr float GRAVITY = 9.81;                    // m/s²
    static constexpr float BOAT_WINDAGE_AREA_M2 = 15.0;      // m² - effective windage area
    static constexpr float AIR_DENSITY = 1.225;               // kg/m³ at sea level
    static constexpr float DRAG_COEFFICIENT = 1.2;            // typical for boat hull + rigging

};

#endif // CHAINCONTROLLER_H
