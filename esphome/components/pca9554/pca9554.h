#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/gpio_expander/cached_gpio_component.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"

namespace esphome::pca9554 {

class PCA9554Component final : public gpio_expander::CachedGpioExpanderComponent<uint16_t, 16>, public i2c::I2CDevice {
 public:
  explicit PCA9554Component(uint8_t pin_count) : pin_count_(pin_count), reg_width_((pin_count + 7) / 8) {}

  /// Check i2c availability and setup masks
  void setup() override;
  /// Helper function to set the pin mode of a pin.
  void pin_mode(uint8_t pin, gpio::Flags flags);

  void dump_config() override;

#ifdef USE_PCA9554_PCAL
  /// The PCAL parts add pull resistors, input latches, an interrupt mask and drive strength.
  void set_pcal() { this->pcal_ = true; }
  /// Only updates the shadow, which setup() writes.
  void set_latch(uint8_t pin) { this->latch_ |= 1U << pin; }
  /// Only updates the shadow, which setup() writes. 0 is a quarter of the full drive strength, 3 the full strength.
  void set_drive_strength(uint8_t pin, uint8_t strength) {
    const uint8_t shift = 2 * pin;
    this->drive_ = (this->drive_ & ~(3UL << shift)) | (uint32_t(strength) << shift);
  }
#endif

 protected:
  bool read_inputs_();
  bool write_register_(uint8_t reg, uint16_t value) { return this->write_address_(reg * this->reg_width_, value); }
  bool write_address_(uint8_t address, uint16_t value);
#ifdef USE_PCA9554_PCAL
  bool write_pcal_register_(uint8_t reg, uint16_t value);
  /// Write every PCAL shadow to the chip; false when any write failed
  bool write_pcal_shadows_();
  /// Retry the shadows when an earlier PCAL write failed
  void retry_pcal_shadows_() {
    if (this->pcal_dirty_)
      this->write_pcal_shadows_();
  }
  /// Changes the masked bits of a shadow and writes the register only when it changed.
  void update_pcal_register_(uint8_t reg, uint16_t &shadow, uint16_t mask, uint16_t value);
#endif

  // Virtual methods from CachedGpioExpander
  bool digital_read_hw(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

#ifdef USE_PCA9554_PCAL
  /// Drive strength shadow, two bits per pin; the low reg_width_ bytes go to OUTPUT_DRIVE_0
  uint32_t drive_{0xFFFFFFFF};
#endif
  /// Mask for the pin config - 1 means OUTPUT, 0 means INPUT
  uint16_t config_mask_{0x00};
  /// The mask to write as output state - 1 means HIGH, 0 means LOW
  uint16_t output_mask_{0x00};
  /// The state of the actual input pins - 1 means HIGH, 0 means LOW
#ifdef USE_PCA9554_PCAL
  // Shadows of the PCAL registers. setup() writes them, so every part starts with the pulls disconnected
  // whatever its power-on state is (the PCAL9555 powers up with pull-ups enabled).
  uint16_t pupd_enable_{0x0000};
  uint16_t pupd_select_{0xFFFF};
  uint16_t latch_{0x0000};
  uint16_t interrupt_mask_{0xFFFF};
#endif
  /// number of bits the expander has
  const uint8_t pin_count_;
  /// width of registers
  const uint8_t reg_width_;
#ifdef USE_PCA9554_PCAL
  bool pcal_{false};
  bool pcal_dirty_{false};  // a PCAL write failed; retried after the next good read or output write
#endif
};

using PCA9554GPIOPin = gpio_expander::ExpanderGPIOPin<PCA9554Component>;

}  // namespace esphome::pca9554

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::pca9554::PCA9554Component);
