#pragma once

#include "esphome/components/output/float_output.h"

namespace esphome {
namespace hbridge {

enum class HBridgeMode {
  OFF = 0,
  DIRECTION_A = 1,
  DIRECTION_B = 2,
  SHORT = 3,
};

enum class TransitionState {
  OFF = 0,
  SHORTING_BUILDUP = 1,
  FULL_SHORT = 2,
  RAMP_UP = 3,
  RAMP_DOWN = 4,
};

enum class CurrentDecayMode {
  SLOW = 0,
  FAST = 1,
};

class HBridge : public Component {
 public:
  // Config set functions
  void set_hbridge_pin_a(output::FloatOutput *pin_a) { pin_a_ = pin_a; }
  void set_hbridge_pin_b(output::FloatOutput *pin_b) { pin_b_ = pin_b; }
  void set_hbridge_enable_pin(output::FloatOutput *enable) { enable_pin_ = enable; }
  void set_hbridge_decay_mode(CurrentDecayMode decay_mode) { current_decay_mode_ = decay_mode; }

  void set_setting_rampup_time_ms(uint32_t val) { setting_full_rampup_time_ms_ = val; }
  void set_setting_rampdown_time_ms(uint32_t val) { setting_full_rampdown_time_ms_ = val; }
  void set_setting_short_buildup_time_ms(uint32_t val) { setting_short_buildup_time_ms_ = val; }
  void set_setting_short_time_ms(uint32_t val) { setting_short_time_ms_ = val; }
  void set_setting_min_dutycycle(float val) { setting_min_dutycycle = val; }

  // Component interfacing
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Hbridge control
  void transition_to_state(HBridgeMode target_mode, float target_dutycycle, uint32_t full_rampup_duration_ms,
                           uint32_t full_rampdown_duration_ms, uint32_t short_buildup_duration_ms,
                           uint32_t short_duration_ms);
  void set_state(HBridgeMode mode, float dutycycle);

 protected:
  // Output pins
  output::FloatOutput *pin_a_;
  output::FloatOutput *pin_b_;
  output::FloatOutput *enable_pin_{nullptr};

  // HBridge control settings

  // Current decay mode: See DRV8833 datasheet - "7.3.2 Bridge Control and Decay Modes" for details
  CurrentDecayMode current_decay_mode_{CurrentDecayMode::SLOW};

  // Settings (storage only)
  uint32_t setting_full_rampup_time_ms_ = 0;
  uint32_t setting_full_rampdown_time_ms_ = 0;
  uint32_t setting_short_buildup_time_ms_ = 0;
  uint32_t setting_short_time_ms_ = 0;
  float setting_min_dutycycle = 0;

  // Object state(s)
  HBridgeMode current_mode_ = HBridgeMode::OFF;
  float current_mode_dutycycle_ = 0;
  void set_output_state_(HBridgeMode mode, float dutycycle);

  // Interface method to get log tag of inherited instance
  virtual const char *get_log_tag();

 private:
  // - State transitioning context -
  TransitionState transition_state_ = TransitionState::OFF;
  uint32_t transition_mode_start_time_ = 0;
  uint32_t transition_last_step_time_ = 0;

  // Target vars
  HBridgeMode transition_target_mode_ = HBridgeMode::OFF;
  float transition_target_mode_dutycycle_ = 0;

  // Ramp-up state vars
  float transition_rampup_dutycycle_delta_per_ms_ = 0;

  // Ramp-down state vars
  float transition_rampdown_dutycycle_delta_per_ms_ = 0;

  // Shorting buildup state vars
  float transition_shorting_buildup_dutycycle_delta_per_ms_ = 0;
  float transition_shorting_dutycycle_ = 0;
  uint32_t transition_shorting_buildup_duration_ms_ = 0;

  // short state vars
  uint32_t transition_short_duration_ms_ = 0;
};

}  // namespace hbridge
}  // namespace esphome
