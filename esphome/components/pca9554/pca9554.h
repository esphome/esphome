#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"

namespace esphome::pca9554 {

class PCA9554Component final : public Component,
                               public i2c::I2CDevice,
                               public gpio_expander::CachedGpioExpander<uint16_t, 16> {
 public:
  PCA9554Component() = default;

  /// Check i2c availability and setup masks
  void setup() override;
  void loop() override;
  /// Helper function to set the pin mode of a pin.
  void pin_mode(uint8_t pin, gpio::Flags flags);

  float get_setup_priority() const override;

  void dump_config() override;

  void set_pin_count(size_t pin_count) { this->pin_count_ = pin_count; }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

 protected:
  static void IRAM_ATTR gpio_intr(PCA9554Component *arg);

  bool read_inputs_();
  bool write_register_(uint8_t reg, uint16_t value);

  // Virtual methods from CachedGpioExpander
  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  /// number of bits the expander has
  size_t pin_count_{8};
  /// width of registers
  size_t reg_width_{1};
  /// Mask for the pin config - 1 means OUTPUT, 0 means INPUT
  uint16_t config_mask_{0x00};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_mask_{0x00};
  /// The state of the actual input pin states - 1 means HIGH, 0 means LOW
  uint16_t input_mask_{0x00};
  /// Storage for last I2C error seen
  esphome::i2c::ErrorCode last_error_;
  InternalGPIOPin *interrupt_pin_{nullptr};
};

/// Helper class to expose a PCA9554 pin as an internal input GPIO pin.
using PCA9554GPIOPin = gpio_expander::ExpanderGPIOPin<PCA9554Component>;

}  // namespace esphome::pca9554

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::pca9554::PCA9554Component);
