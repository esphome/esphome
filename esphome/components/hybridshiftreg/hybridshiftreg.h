#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace hybridshiftreg {

class HybridShiftComponent : public Component {
 public:
  HybridShiftComponent() = default;

  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_data_out_pin(GPIOPin *pin) { data_out_pin_ = pin; }
  void set_data_in_pin(GPIOPin *pin) { data_in_pin_ = pin; }
  void set_clock_pin(GPIOPin *pin) { clock_pin_ = pin; }
  void set_inh_latch_pin(GPIOPin *pin) { inh_latch_pin_ = pin; }
  void set_load_oe_pin(GPIOPin *pin) { load_oe_pin_ = pin; }
  void set_sr_count(uint8_t count) {
    sr_count_ = count;
    this->output_bits_.resize(count * 8);
    this->input_bits_.resize(count * 8);
  }

 protected:
  friend class HybridShiftGPIOPin;
  void digital_write_(uint16_t pin, bool value);
  bool digital_read_(uint16_t pin);
  void read_write_gpio_();

  GPIOPin *data_out_pin_;
  GPIOPin *data_in_pin_;
  GPIOPin *clock_pin_;
  GPIOPin *inh_latch_pin_;
  GPIOPin *load_oe_pin_;
  uint8_t sr_count_;
  std::vector<bool> output_bits_;
  std::vector<bool> input_bits_;
};

/// Helper class to expose a HybridShiftRegister pin as an internal input/output GPIO pin.
class HybridShiftGPIOPin : public GPIOPin, public Parented<HybridShiftComponent> {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_pin(uint16_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }

 protected:
  uint16_t pin_;
  bool inverted_;
};

}  // namespace hybridshiftreg
}  // namespace esphome
