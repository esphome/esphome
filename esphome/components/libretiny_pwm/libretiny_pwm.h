#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/output/float_output.h"

#ifdef USE_LIBRETINY

namespace esphome {
namespace libretiny_pwm {

class LibreTinyPWM : public output::FloatOutput, public Component {
 public:
  explicit LibreTinyPWM(InternalGPIOPin *pin) : pin_(pin) {}

  void set_frequency(float frequency) { this->frequency_ = frequency; }
  /// Dynamically change frequency at runtime
  void update_frequency(float frequency) override;

  /// Setup LibreTinyPWM.
  void setup() override;
  void dump_config() override;
  /// HARDWARE setup priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Override FloatOutput's write_state.
  void write_state(float state) override;

 protected:
  InternalGPIOPin *pin_;
  uint8_t bit_depth_{10};
  float frequency_{};
  float duty_{0.0f};
  bool initialized_ = false;
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...> {
 public:
  SetFrequencyAction(LibreTinyPWM *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, frequency);

  void play(Ts... x) {
    float freq = this->frequency_.value(x...);
    this->parent_->update_frequency(freq);
  }

 protected:
  LibreTinyPWM *parent_;
};

}  // namespace libretiny_pwm
}  // namespace esphome

#endif
