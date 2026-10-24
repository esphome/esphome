#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/gpio_expander/cached_gpio_component.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::pca9505 {

/// Number of I/O pins on the PCA9505 and PCA9506.
static constexpr uint8_t PIN_COUNT = 40;
/// The pins are grouped into five ports of eight, each with its own set of registers.
static constexpr uint8_t BANK_COUNT = PIN_COUNT / 8;

class PCA9505Component final : public gpio_expander::CachedGpioExpanderComponent<uint8_t, PIN_COUNT>,
                               public i2c::I2CDevice {
 public:
  void setup() override;
  void pin_mode(uint8_t pin, gpio::Flags flags);

  void dump_config() override;

 protected:
  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  bool read_bank_(uint8_t bank);
  bool write_bank_(uint8_t reg, uint8_t bank, uint8_t value);
  bool write_all_banks_(uint8_t reg, uint8_t value);

  /// The pin config per bank - 1 means OUTPUT, 0 means INPUT
  uint8_t config_[BANK_COUNT]{};
  /// The output state per bank - 1 means HIGH, 0 means LOW
  uint8_t output_[BANK_COUNT]{};
  /// The input pin states per bank - 1 means HIGH, 0 means LOW
  uint8_t input_[BANK_COUNT]{};
  /// Which pins may pull INT low - 1 means masked, matching the chip
  uint8_t mask_[BANK_COUNT]{};
};

using PCA9505GPIOPin = gpio_expander::ExpanderGPIOPin<PCA9505Component>;

}  // namespace esphome::pca9505

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::pca9505::PCA9505Component);
