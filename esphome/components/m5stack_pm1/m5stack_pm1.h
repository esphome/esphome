#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/gpio_expander/cached_gpio_component.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"
#include "esphome/components/i2c/i2c.h"
#include "const.h"

namespace esphome::m5stack_pm1 {

/// Hub for the M5Stack PM1 power management and GPIO expander.
class M5StackPM1Component final : public gpio_expander::CachedGpioExpanderComponent<uint8_t, 8>, public i2c::I2CDevice {
 public:
  explicit M5StackPM1Component(bool fast_mode) : fast_mode_(fast_mode) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  /// GPIO of this expander that drives the host interrupt line, and the GPIO IRQ mask (a set bit disables a pin).
  void set_interrupt_gpio(uint8_t pin, uint8_t irq_gpio_mask) {
    this->interrupt_gpio_ = pin;
    this->irq_gpio_mask_ = irq_gpio_mask;
  }
  /// WAKE_EN and WAKE_CFG register values, written once in setup().
  void set_wake_config(uint8_t enable, uint8_t rising) {
    this->wake_enable_ = enable;
    this->wake_rising_ = rising;
  }

  /// A masked pin never raises an interrupt, so it has to refresh its own cache.
  bool digital_read(uint8_t pin) { return this->cached_read_(pin, (this->irq_gpio_mask_ & (1 << pin)) != 0); }

  void pin_mode(uint8_t pin, gpio::Flags flags);

 protected:
  static constexpr uint32_t PIN_CONFIG_RETRY_ID = 0;
  static constexpr uint32_t PIN_CONFIG_RETRY_INTERVAL_MS = 1000;
  /// Bit in pending_pin_config_mask_ for the wake registers; bits 0-4 are the pins
  static constexpr uint8_t WAKE_PENDING_BIT = 7;

  bool digital_read_hw(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  void apply_pin_config_(uint8_t pin, gpio::Flags flags);
  void set_pin_config_pending_(uint8_t pin, bool pending);
  void apply_wake_();
  void report_config_(uint8_t bit, bool success);
  void retry_pin_config_();
  bool read_device_info_();
  bool process_irq_();

  /// Cached GPIO register window; the input byte is not kept up to date.
  M5StackPM1GPIORegs gpio_regs_{};
  std::array<gpio::Flags, M5STACK_PM1_GPIO_COUNT> pin_flags_{};
  uint8_t hardware_revision_{0};
  uint8_t software_revision_{0};
  uint8_t wake_enable_{0};
  uint8_t wake_rising_{0};
  // GPIO IRQ mask; a set bit disables that pin's interrupt.
  uint8_t irq_gpio_mask_{0x1F};
  uint8_t pending_pin_config_mask_{0};
  uint8_t interrupt_gpio_{0};
  bool fast_mode_{false};
};

using M5StackPM1GPIOPin = gpio_expander::ExpanderGPIOPin<M5StackPM1Component>;

}  // namespace esphome::m5stack_pm1

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::m5stack_pm1::M5StackPM1Component);
