// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
// datasheet: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1210/IO_Expander_Datasheet_EN.pdf
#pragma once

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"

namespace esphome::m5ioe1 {

enum M5IOE1ADCChannel : uint8_t {
  ADC_NONE = 0,
  ADC_1 = 1,
  ADC_2 = 2,
  ADC_3 = 3,
  ADC_4 = 4,
  REF_VOLT = 5,
};

enum M5IOE1PWMChannel : uint8_t {
  PWM_1 = 0,
  PWM_2 = 1,
  PWM_3 = 2,
  PWM_4 = 3,
};

enum M5IOE1PWMPolarity : uint8_t {
  ACTIVE_HIGH = 0,
  ACTIVE_LOW = 1,
};

enum M5IOE1InterruptType : uint8_t {
  RISING_EDGE = 1,
  FALLING_EDGE = 0,
};

class M5IOE1Component : public Component, public i2c::I2CDevice, public gpio_expander::CachedGpioExpander<uint8_t, 14> {
 public:
  M5IOE1Component() = default;

  void setup() override;
  void pin_mode(uint8_t pin, gpio::Flags flags);

  void dump_config() override;
  void loop() override;

  float get_setup_priority() const override { return setup_priority::IO; }

  /// Indicate if the component should reset the state during setup
  void set_reset(bool reset) { this->reset_ = reset; }

  //  Set interrupt pin
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

  void setup_gpio_interrupt(uint8_t pin, M5IOE1InterruptType type);

  /// Set the pwm frequency
  void set_pwm_frequency(uint16_t freq) { this->pwm_freq_ = freq; }

  /// Setup PWM output
  void setup_pwm_channel(M5IOE1PWMChannel channel, M5IOE1PWMPolarity polar) {
    this->config_pwm_channel_(channel, polar);
  }
  /// Set PWM duty
  void set_pwm_value(M5IOE1PWMChannel channel, float duty) { this->write_pwm_duty_(channel, duty); }

  /// Read value from ADC registers
  float get_adc_value(M5IOE1ADCChannel channel) { return this->read_adc_(channel); }
  /// Read ADC reference voltage
  float get_adc_ref_voltage() { return this->read_adc_ref_(); }

  /// Read temperature value
  float get_temperature_value() { return this->read_temperature_(); }

  /// Setup NeoPixel LED
  void setup_neopixel(uint8_t num) { this->config_neopixel_(num); }

  /// Set LED color
  void set_led_color(const uint8_t *buf, size_t len) { this->write_led_ram_(buf, len); }

  /// Setup AW8737A
  void setup_aw8737a(uint8_t pin, uint8_t pulse_num) { this->config_aw8737a_(pin, pulse_num); }

  // enable aw8737a
  void start_aw8737a(bool enabled) { this->enable_aw8737a_pluse_(enabled); }

 protected:
  static void IRAM_ATTR gpio_intr(M5IOE1Component *arg);

  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  /// Mask for the pin mode - 1 means output, 0 means input
  uint16_t mode_mask_{0};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_mask_{0};
  /// The state read in digital_read_hw - 1 means HIGH, 0 means LOW
  uint16_t input_mask_{0};
  /// The mask to write as input buffer state - 1 means enabled, 0 means disabled
  uint16_t pull_enable_mask_{0};
  /// Mask for drive mode - 1 means open-drain, 0 means push-pull,
  //  default to push-pull
  uint16_t drv_mode_mask_{0x3FFF};
  /// The mask to write as pullup state
  uint16_t pull_up_mask_{0};
  /// The mask to write as pulldown state
  uint16_t pull_down_mask_{0};

  /// Cached interrupt configuration bitmap for all pins
  uint16_t interrupt_config_mask_{0};
  /// Cached interrupt status bitmap for pins that triggered interrupts
  uint16_t interrupt_result_mask_{0};

  bool reset_{true};

  bool read_gpio_modes_();
  bool write_gpio_modes_();
  bool read_gpio_outputs_();

  bool config_interrupt_edge_(uint8_t pin, M5IOE1InterruptType type);
  bool enable_gpio_interrupt_(uint8_t pin);
  bool disable_gpio_interrupt_(uint8_t pin);
  bool read_gpio_interrupt_();
  bool clear_gpio_interrupt_();

  /// Interrupt pin
  InternalGPIOPin *interrupt_pin_{nullptr};

  /// PWM channel
  uint16_t pwm_freq_{500};  // PWM frequency

  /// Configure PWM channel
  void config_pwm_channel_(M5IOE1PWMChannel channel, M5IOE1PWMPolarity polar);

  /// Write PWM duty to given channel
  void write_pwm_duty_(M5IOE1PWMChannel channel, float duty);

  /// Read value from given ADC channel
  float read_adc_(M5IOE1ADCChannel channel);

  /// Read adc reference voltage
  float read_adc_ref_();

  /// Read temperature value
  float read_temperature_();

  /// Configure on device NeoPixel
  void config_neopixel_(uint8_t num);

  /// Write LED RAM data
  void write_led_ram_(const uint8_t *buf, size_t len);

  /// configure on-device aw8737a pulse
  void config_aw8737a_(uint8_t pin, uint8_t pulse_num);

  /// enable pulse generation
  void enable_aw8737a_pluse_(bool enabled);

  uint16_t device_uid_{0};
  uint8_t firm_ver_{0};
};

class M5IOE1GPIOPin : public GPIOPin, public Parented<M5IOE1Component> {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;

  // Pin start with 1, but for convenience, we usually
  // calculate start with 0 internally.
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  void set_use_interrupt(bool use_interrupt) { this->use_interrupt_ = use_interrupt; }
  void set_interrupt_type(M5IOE1InterruptType type) { this->interrupt_type_ = type; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
  bool use_interrupt_{false};
  M5IOE1InterruptType interrupt_type_{FALLING_EDGE};
};

}  // namespace esphome::m5ioe1
