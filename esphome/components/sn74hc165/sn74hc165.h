#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <span>

namespace esphome::sn74hc165 {

class SN74HC165Component final : public Component {
 public:
  /// `input_bytes` holds one byte per register, a static array from code generation
  explicit SN74HC165Component(std::span<uint8_t> input_bytes) : input_bytes_(input_bytes) {}

  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_data_pin(GPIOPin *pin) { this->data_pin_ = pin; }
  void set_clock_pin(GPIOPin *pin) { this->clock_pin_ = pin; }
  void set_load_pin(GPIOPin *pin) { this->load_pin_ = pin; }
  void set_clock_inhibit_pin(GPIOPin *pin) { this->clock_inhibit_pin_ = pin; }

 protected:
  friend class SN74HC165GPIOPin;
  bool digital_read_(uint16_t pin);
  void read_gpio_();

  GPIOPin *data_pin_;
  GPIOPin *clock_pin_;
  GPIOPin *load_pin_;
  GPIOPin *clock_inhibit_pin_;
  std::span<uint8_t> input_bytes_;
};

/// Helper class to expose a SC74HC165 pin as an internal input GPIO pin.
class SN74HC165GPIOPin final : public GPIOPin, public Parented<SN74HC165Component> {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  bool digital_read() override;
  void digital_write(bool value) override {};
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_pin(uint16_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }

  /// Always returns `gpio::Flags::FLAG_INPUT`.
  gpio::Flags get_flags() const override { return gpio::Flags::FLAG_INPUT; }

 protected:
  uint16_t pin_;
  bool inverted_;
};

}  // namespace esphome::sn74hc165
