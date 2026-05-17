#pragma once

#ifdef USE_ZEPHYR

#include "esphome/components/output/float_output.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <zephyr/device.h>

namespace esphome::zephyr_pwm {

class ZephyrPWMChannel : public output::FloatOutput, public Component {
 public:
  explicit ZephyrPWMChannel(const struct device *device, uint8_t channel, bool inverted)
      : device_(device), channel_(channel), inverted_(inverted) {}

  bool set_frequency(float frequency) {
    if (frequency < 1 || frequency > 1e7) {
      return false;
    }
    this->period_ns_ = 1e9f / frequency;
    return true;
  }
  /// Dynamically update frequency
  void update_frequency(float frequency) override {
    if (!this->set_frequency(frequency)) {
      return;
    }
    this->write_state(this->last_output_);
  }

  void setup() override;
  void dump_config() override;
  /// HARDWARE setup_priority
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;

  const struct device *device_;
  uint8_t channel_;
  bool inverted_;
  uint32_t period_ns_{1000 * 1000};  // default to 1kHz
  float last_output_{0.0};
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...> {
 public:
  SetFrequencyAction(ZephyrPWMChannel *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, frequency);

  void play(const Ts &...x) {
    float freq = this->frequency_.value(x...);
    this->parent_->update_frequency(freq);
  }

 protected:
  ZephyrPWMChannel *parent_;
};

}  // namespace esphome::zephyr_pwm

#endif  // USE_ZEPHYR
