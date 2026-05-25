#pragma once

#ifdef USE_NRF52

#include "esphome/components/output/float_output.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

namespace esphome::nrf52_pwm {

class Nrf52PWMOutput : public output::FloatOutput, public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_device(const device *dev) { this->dev_ = dev; }
  void set_channel(uint32_t channel) { this->channel_ = channel; }
  void set_frequency(float frequency) { this->frequency_ = frequency; }
  void set_runtime_frequency_mutable(bool mutable_frequency) { this->runtime_frequency_mutable_ = mutable_frequency; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void update_frequency(float frequency) override;

 protected:
  void write_state(float state) override;
  bool write_pwm_();
  uint32_t period_ns_() const;

  InternalGPIOPin *pin_{nullptr};
  const device *dev_{nullptr};
  uint32_t channel_{0};
  float frequency_{1000.0f};
  float last_output_{0.0f};
  bool runtime_frequency_mutable_{false};
};

template<typename... Ts> class SetFrequencyAction : public Action<Ts...> {
 public:
  explicit SetFrequencyAction(Nrf52PWMOutput *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, frequency);

  void play(const Ts &...x) override {
    float freq = this->frequency_.value(x...);
    this->parent_->update_frequency(freq);
  }

 protected:
  Nrf52PWMOutput *parent_;
};

}  // namespace esphome::nrf52_pwm

#endif  // USE_NRF52
