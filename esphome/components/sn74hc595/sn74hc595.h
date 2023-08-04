#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace sn74hc595 {

class SN74HC595Component : public Component {
 public:
  SN74HC595Component() = default;

  void setup() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_data_pin(GPIOPin *pin) { data_pin_ = pin; }
  void set_clock_pin(GPIOPin *pin) { clock_pin_ = pin; }
  void set_latch_pin(GPIOPin *pin) { latch_pin_ = pin; }
  void set_oe_pin(GPIOPin *pin) {
    oe_pin_ = pin;
    have_oe_pin_ = true;
  }
  void set_sr_count(uint8_t count) {
    sr_count_ = count;
    this->output_bits_.resize(count * 8);
  }

 protected:
  friend class SN74HC595GPIOPin;
  void digital_write_(uint16_t pin, bool value);
  void write_gpio_();

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  GPIOPin *latch_pin_;
  GPIOPin *oe_pin_;
  uint8_t sr_count_;
  bool have_oe_pin_{false};
  std::vector<bool> output_bits_;
};

/// Helper class to expose a SC74HC595 pin as an internal output GPIO pin.
class SN74HC595GPIOPin : public GPIOPin, public Parented<SN74HC595Component> {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  bool digital_read() override { return false; }
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_pin(uint16_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }

 protected:
  uint16_t pin_;
  bool inverted_;
};

}  // namespace sn74hc595
}  // namespace esphome
