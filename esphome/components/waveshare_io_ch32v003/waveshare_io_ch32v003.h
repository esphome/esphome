#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::waveshare_io_ch32v003 {

class WaveshareIOCH32V003Component : public Component,
                                     public i2c::I2CDevice,
                                     public gpio_expander::CachedGpioExpander<uint8_t, 8> {
 public:
  WaveshareIOCH32V003Component() = default;

  void setup() override;
  void pin_mode(uint8_t pin, gpio::Flags flags);

  float get_setup_priority() const override;

  void dump_config() override;

  void loop() override;

  uint16_t get_adc_value();
  uint8_t get_rtc_interrupt_status();
  void set_pwm_value(uint8_t value);  // 0 - 255

 protected:
  friend class WaveshareIOCH32V003GPIOPin;

  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  uint8_t mode_mask_{0x00};    // Mask for the pin mode - 1 means output, 0 means input
  uint8_t output_mask_{0x00};  // The mask to write as output state - 1 means HIGH, 0 means LOW
  uint8_t input_mask_{0x00};   // The state read in digital_read_hw - 1 means HIGH, 0 means LOW

  bool write_gpio_modes_();
  bool write_gpio_outputs_();
};

/// Helper class to expose a WaveshareIO pin as a GPIO pin.
class WaveshareIOCH32V003GPIOPin : public GPIOPin, public Parented<WaveshareIOCH32V003Component> {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  uint8_t pin_{};
  bool inverted_{};
  gpio::Flags flags_{};
};

}  // namespace esphome::waveshare_io_ch32v003
