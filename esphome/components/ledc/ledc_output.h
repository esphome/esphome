#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/output/float_output.h"

#ifdef USE_ESP32

namespace esphome {
namespace ledc {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern uint8_t next_ledc_channel;

class LEDCOutput : public output::FloatOutput, public Component {
 public:
  explicit LEDCOutput(InternalGPIOPin *pin) : pin_(pin) { this->channel_ = next_ledc_channel++; }

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  void set_min_duty(float min_duty) { this->min_duty_ = min_duty; }
  void set_min_duty_turn_on(float min_duty_turn_on) { this->min_duty_turn_on_ = min_duty_turn_on; }
  /// Dynamically change frequency at runtime
  void update_frequency(float frequency) override;

  /// Setup LEDC.
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Override FloatOutput's write_state.
  void write_state(float state) override;

 protected:
  bool is_on(void);
  void write_state_final(float state);
  InternalGPIOPin *pin_;
  uint8_t channel_{};
  uint8_t bit_depth_{};
  float frequency_{};
  float duty_{0.0f};
  float min_duty_{0.0f};
  float min_duty_turn_on_{0.0f};
  bool initialized_ = false;
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...> {
 public:
  SetFrequencyAction(LEDCOutput *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, frequency);

  void play(Ts... x) {
    float freq = this->frequency_.value(x...);
    this->parent_->update_frequency(freq);
  }

 protected:
  LEDCOutput *parent_;
};

}  // namespace ledc
}  // namespace esphome

#endif
