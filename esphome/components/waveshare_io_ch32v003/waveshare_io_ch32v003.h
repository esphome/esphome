#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"

namespace esphome::waveshare_io_ch32v003 {

class WaveshareIOCH32V003Component
    : public Component,
      public gpio_expander::GpioExpanderInputs<gpio_expander::CachedGpioExpander<uint8_t, 8>>,
      public i2c::I2CDevice {
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
  /// Write an output level, 0.0 to 1.0, clamped to the board's safe PWM range.
  void write_pwm_level(float state, uint8_t min_value, uint8_t max_value);

 protected:
  bool digital_read_hw(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  uint8_t mode_mask_{0x00};    // Mask for the pin mode - 1 means output, 0 means input
  uint8_t output_mask_{0x00};  // The mask to write as output state - 1 means HIGH, 0 means LOW

  bool write_gpio_modes_();
  bool write_gpio_outputs_();
};

using WaveshareIOCH32V003GPIOPin = gpio_expander::ExpanderGPIOPin<WaveshareIOCH32V003Component>;

}  // namespace esphome::waveshare_io_ch32v003

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::waveshare_io_ch32v003::WaveshareIOCH32V003Component);
