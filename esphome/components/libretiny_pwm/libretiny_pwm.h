#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"

#ifdef USE_LIBRETINY

namespace esphome::libretiny_pwm {

class LibreTinyPWM final : public output::FloatOutput, public Component {
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

}  // namespace esphome::libretiny_pwm

#endif
