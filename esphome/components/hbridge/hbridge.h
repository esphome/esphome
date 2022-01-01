#pragma once

#include "esphome/components/output/float_output.h"

namespace esphome {
namespace hbridge {

enum HBridgeMode {
  HBRIDGE_MODE_OFF = 0,
  HBRIDGE_MODE_DIRECTION_A = 1,
  HBRIDGE_MODE_DIRECTION_B = 2,
  HBRIDGE_MODE_SHORT = 3,
};

enum HBridgeTransitionState {
  HBRIDGE_TRANSITIONING_STATE_OFF = 0,
  HBRIDGE_TRANSITIONING_STATE_SHORTING_BUILDUP = 1,
  HBRIDGE_TRANSITIONING_STATE_FULL_SHORT = 2,
  HBRIDGE_TRANSITIONING_STATE_DUTYCYCLE_TRANSITIONING = 3,
};

class HBridge {
 public:
  //Config set functions
  void set_hbridge_pin_a(output::FloatOutput *pin_a) { pin_a_ = pin_a; }
  void set_hbridge_pin_b(output::FloatOutput *pin_b) { pin_b_ = pin_b; }
  void set_hbridge_enable_pin(output::FloatOutput *enable) { enable_pin_ = enable; }
  void set_setting_transition_delta_per_ms(float val) { setting_transition_delta_per_ms = val; }
  void set_setting_transition_shorting_buildup_duration_ms(uint32_t val) { setting_transition_shorting_buildup_duration_ms_ = val; }
  void set_setting_transition_full_short_duration_ms(uint32_t val) { setting_transition_full_short_duration_ms_ = val; }

  //ESPHome component interfacing
  void setup();
  void loop();

  //Hbridge control
  void transition_to_state(HBridgeMode target_mode, float target_dutycycle, float dutycycle_delta_per_ms, 
    uint32_t shorting_buildup_duration_ms, uint32_t full_short_duration_ms);
  void set_state(HBridgeMode mode, float dutycycle);

 protected:
  //Output pins
  output::FloatOutput *pin_a_;
  output::FloatOutput *pin_b_;
  output::FloatOutput *enable_pin_{nullptr};

  //Transition settings (storage only)
  float setting_transition_delta_per_ms = 0;
  uint32_t setting_transition_shorting_buildup_duration_ms_ = 0;
  uint32_t setting_transition_full_short_duration_ms_ = 0;
    
  //Object state(s)
  float current_relative_dutycycle_ = 0;
  HBridgeMode current_mode_ = HBRIDGE_MODE_OFF;

  void set_output_state_by_relative_dutycycle(float relative_dutycycle);
  void set_output_state(HBridgeMode mode, float dutycycle);

 private:
  // - State transitioning context -
  HBridgeTransitionState transition_state = HBRIDGE_TRANSITIONING_STATE_OFF;
  uint32_t transition_mode_start_time = 0;
  uint32_t transition_last_step_time_ = 0;

  //Target vars
  HBridgeMode transition_target_mode_ = HBRIDGE_MODE_OFF;
  float transition_target_mode_dutycycle_ = 0;
  float transition_target_relative_dutycycle_ = 0;

  //Dutycycle buildup state vars
  float transition_relative_dutycycle_delta_per_ms_ = 0;  

  //Shorting buildup state vars
  float transition_shorting_dutycycle_delta_per_ms_ = 0;
  float transition_shorting_dutycycle_ = 0;
  uint32_t transition_shorting_buildup_duration_ms_ = 0;
  
  //Full short state vars
  uint32_t transition_full_short_duration_ms_ = 0;
};

}  // namespace hbridge
}  // namespace esphome
