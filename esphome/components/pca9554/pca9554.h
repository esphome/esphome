#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/gpio_expander/cached_gpio.h"

namespace esphome {
namespace pca9554 {

class PCA9554Component : public Component,
                         public i2c::I2CDevice,
                         public gpio_expander::CachedGpioExpander<uint16_t, 16> {
 public:
  PCA9554Component() = default;

  /// Check i2c availability and setup masks
  void setup() override;
  void loop() override;
  /// Helper function to set the pin mode of a pin.
  void pin_mode(uint8_t pin, gpio::Flags flags);
  void set_latch(uint8_t pin, bool latch_state);
  void set_interrupt(uint8_t pin, bool interrupt_state);
  void set_drive_strength(uint8_t pin, uint16_t strength_to_set);
  void set_open_drain(uint8_t bank_to_set, bool state_open_drain);
  uint16_t get_interrupt_status();

  float get_setup_priority() const override;

  void dump_config() override;

  void set_pin_count(size_t pin_count) { this->pin_count_ = pin_count; }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_capability(uint8_t capability) { this->capability_ = capability; }

 protected:
  static void IRAM_ATTR gpio_intr(PCA9554Component *arg);

  bool read_inputs_();
  uint8_t get_register_address_(uint8_t reg);
  bool write_register_(uint8_t reg, uint16_t value);
  bool read_register_(uint8_t reg, uint16_t &register_value);
  bool set_register_bit_(uint8_t reg, uint8_t bit, bool value);

  // Virtual methods from CachedGpioExpander
  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  /// number of bits the expander has
  size_t pin_count_{8};
  /// width of registers
  size_t reg_width_{1};
  /// The state of the actual pins - 1 means HIGH, 0 means LOW
  uint16_t input_states_{0x00};
  /// Storage for last I2C error seen
  esphome::i2c::ErrorCode last_error_;
  InternalGPIOPin *interrupt_pin_{nullptr};

  uint8_t capability_{0};
};

/// Helper class to expose a PCA9554 pin as an internal input GPIO pin.
class PCA9554GPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_parent(PCA9554Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }
  void set_latch(bool latch) { this->latch_ = latch; }
  void set_interrupt(bool interrupt) { this->interrupt_ = interrupt; }
  void set_drive_strength(uint8_t drive_strength) { this->drive_strength_ = drive_strength; }

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  PCA9554Component *parent_;
  uint8_t pin_;
  bool inverted_;
  bool latch_{false};
  bool interrupt_{false};
  gpio::Flags flags_;
  uint8_t drive_strength_;
};

}  // namespace pca9554
}  // namespace esphome
