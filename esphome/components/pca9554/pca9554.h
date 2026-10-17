#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/gpio_expander/cached_gpio.h"

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
#ifdef USE_PCA9554_PCAL
  /// The PCAL parts add pull resistors, input latches, an interrupt mask and drive strength.
  void set_pcal(bool pcal) { this->pcal_ = pcal; }
  void set_latch(uint8_t pin, bool latch);
  /// A masked pin never asserts INT; the register powers up with every pin masked.
  void set_interrupt(uint8_t pin, bool enabled);
  /// 0 is a quarter of the full drive strength, 3 the full strength.
  void set_drive_strength(uint8_t pin, uint8_t strength);
#endif

 protected:
  static void IRAM_ATTR gpio_intr(PCA9554Component *arg);

  bool read_inputs_();
  bool write_register_(uint8_t reg, uint16_t value);
#ifdef USE_PCA9554_PCAL
  bool read_register_(uint8_t reg, uint16_t &value);
  /// Read-modify-write of one bit; only the PCAL registers, written once at setup, go through here.
  bool set_register_bit_(uint8_t reg, uint8_t bit, bool value);
  /// Writes the power-on defaults, which only a power cycle restores on the chip itself.
  void reset_pcal_registers_();
#endif

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
  /// The state of the actual input pins - 1 means HIGH, 0 means LOW
  uint16_t input_mask_{0x00};
  /// Storage for last I2C error seen
  esphome::i2c::ErrorCode last_error_;
  InternalGPIOPin *interrupt_pin_{nullptr};
#ifdef USE_PCA9554_PCAL
  bool pcal_{false};
#endif
};

/// Helper class to expose a PCA9554 pin as an internal input GPIO pin.
class PCA9554GPIOPin final : public GPIOPin {
 public:
  // User provided, not "= default": `new(p) PCA9554GPIOPin()` would zero-fill .bss that is already zero.
  PCA9554GPIOPin() {}

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_parent(PCA9554Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }
#ifdef USE_PCA9554_PCAL
  void set_latch(bool latch) { this->latch_ = latch; }
  void set_interrupt(bool interrupt) { this->interrupt_ = interrupt; }
  void set_drive_strength(uint8_t drive_strength) { this->drive_strength_ = drive_strength; }
#endif

  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  PCA9554Component *parent_{nullptr};
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{};
#ifdef USE_PCA9554_PCAL
  bool latch_{false};
  bool interrupt_{false};
  uint8_t drive_strength_{3};
#endif
};

}  // namespace esphome::pca9554
