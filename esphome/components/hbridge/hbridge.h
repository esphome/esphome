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

enum HBridgeRampMode {
  HBRIDGE_RAMP_IDLE = 0,
  HBRIDGE_RAMP_UP = 1,
  HBRIDGE_RAMP_DOWN = 2,
};

class HBridge {
 public:
  //Config set functions
  void set_hbridge_pin_a(output::FloatOutput *pin_a) { pin_a_ = pin_a; }
  void set_hbridge_pin_b(output::FloatOutput *pin_b) { pin_b_ = pin_b; }
  void set_hbridge_enable_pin(output::FloatOutput *enable) { enable_pin_ = enable; }

  //ESPHome component interfacing
  void setup();
  void loop();

  //Hbridge control
  void hbridge_set_state(HBridgeMode mode, float dutycycle);
  void hbridge_ramp_to_dutycycle(float target_dutycycle, uint32_t ramp_duration_ms);

 protected:
  //Output pins
  output::FloatOutput *pin_a_;
  output::FloatOutput *pin_b_;
  output::FloatOutput *enable_pin_{nullptr};
  
  //Object state(s)
  float current_dutycycle_ = 0;
  HBridgeMode current_mode_ = HBRIDGE_MODE_OFF;

  void hbridge_set_output_state(HBridgeMode mode, float dutycycle);

 private:
  //Ramp up-down state(s)
  HBridgeRampMode ramp_to_mode = HBRIDGE_RAMP_IDLE;
  float ramp_to_target_dutycycle_ = 0;
  float ramp_to_dutycycle_step_per_ms_ = 0;
  uint32_t ramp_to_duration_ms_ = 0;
  uint32_t ramp_to_last_step_time_ = 0;
};

}  // namespace hbridge
}  // namespace esphome
