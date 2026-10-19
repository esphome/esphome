#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"

namespace esphome::pca6416a {

class PCA6416AComponent final : public Component,
                                public i2c::I2CDevice,
                                public gpio_expander::CachedGpioExpander<uint8_t, 16> {
 public:
  PCA6416AComponent() = default;

  /// Check i2c availability and setup masks
  void setup() override;
  void loop() override;
  /// Helper function to set the pin mode of a pin.
  void pin_mode(uint8_t pin, gpio::Flags flags);

  float get_setup_priority() const override;

  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

 protected:
  static void IRAM_ATTR gpio_intr(PCA6416AComponent *arg);
  // Virtual methods from CachedGpioExpander
  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  bool read_register_(uint8_t reg, uint8_t *value);
  bool write_register_(uint8_t reg, uint8_t value);
  void update_register_(uint8_t pin, bool pin_value, uint8_t reg_addr);

  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint8_t output_0_{0x00};
  uint8_t output_1_{0x00};
  /// Cache for input values (16-bit combined for both banks)
  uint16_t input_mask_{0x00};
  /// Storage for last I2C error seen
  esphome::i2c::ErrorCode last_error_;
  /// Only the PCAL6416A has pull-up resistors
  bool has_pullup_{false};
  InternalGPIOPin *interrupt_pin_{nullptr};
};

using PCA6416AGPIOPin = gpio_expander::ExpanderGPIOPin<PCA6416AComponent>;

}  // namespace esphome::pca6416a

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::pca6416a::PCA6416AComponent);
