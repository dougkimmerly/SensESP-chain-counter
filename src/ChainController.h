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
    float move_distance_= 0.0;        
    float move_speed_ms_per_m_;   
    unsigned long move_timeout_; 

    void updateTimeout(float, float); 
    

    float upSpeed_ = 1000.0;   // default 1 sec per meter
    float downSpeed_ = 1000.0; // default 1 sec per meter
    const float smoothing_factor_ = 0.2;
};

#endif // CHAINCONTROLLER_H
