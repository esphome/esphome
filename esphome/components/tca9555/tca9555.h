#pragma once

#include "esphome/components/gpio_expander/cached_gpio_component.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::tca9555 {

class TCA9555Component final : public gpio_expander::CachedGpioExpanderComponent<uint8_t, 16>, public i2c::I2CDevice {
 public:
  TCA9555Component() = default;

  /// Check i2c availability and setup masks
  void setup() override;
  void pin_mode(uint8_t pin, gpio::Flags flags);

  void dump_config() override;

 protected:
  // Virtual methods from GpioExpander base class — names come from base
  bool digital_read_hw(uint8_t pin) override;               // NOLINT(readability-identifier-naming)
  bool digital_read_cache(uint8_t pin) override;            // NOLINT(readability-identifier-naming)
  void digital_write_hw(uint8_t pin, bool value) override;  // NOLINT(readability-identifier-naming)

  /// Mask for the pin mode - 1 means output, 0 means input
  uint16_t mode_mask_{0x00};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_mask_{0x00};
  /// The state read in digital_read_hw - 1 means HIGH, 0 means LOW
  uint16_t input_mask_{0x00};

  bool read_gpio_modes_();
  bool write_gpio_modes_();
  bool read_gpio_outputs_();
};

using TCA9555GPIOPin = gpio_expander::ExpanderGPIOPin<TCA9555Component>;

}  // namespace esphome::tca9555

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::tca9555::TCA9555Component);
