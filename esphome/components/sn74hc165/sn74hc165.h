#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace sn74hc165 {

class SN74HC165Component : public Component {
 public:
  SN74HC165Component() = default;

  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_data_pin(GPIOPin *pin) { this->data_pin_ = pin; }
  void set_clock_pin(GPIOPin *pin) { this->clock_pin_ = pin; }
  void set_load_pin(GPIOPin *pin) { this->load_pin_ = pin; }
  void set_clock_inhibit_pin(GPIOPin *pin) { this->clock_inhibit_pin_ = pin; }
  void set_sr_count(uint8_t count) {
    this->sr_count_ = count;
    this->input_bits_.resize(count * 8);
  }

 protected:
  friend class SN74HC165GPIOPin;
  bool digital_read_(uint16_t pin);
  void read_gpio_();

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  GPIOPin *load_pin_;
  GPIOPin *clock_inhibit_pin_;
  uint8_t sr_count_;
  std::vector<bool> input_bits_;
};

/// Helper class to expose a SC74HC165 pin as an internal input GPIO pin.
class SN74HC165GPIOPin : public GPIOPin, public Parented<SN74HC165Component> {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  bool digital_read() override;
  void digital_write(bool value) override{};
  std::string dump_summary() const override;

  void set_pin(uint16_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }

 protected:
  uint16_t pin_;
  bool inverted_;
};

}  // namespace sn74hc165
}  // namespace esphome
