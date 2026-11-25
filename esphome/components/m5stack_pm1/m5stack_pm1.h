#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/gpio_expander/cached_gpio_component.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "const.h"

namespace esphome::m5stack_pm1 {

/// Hub for the M5Stack PM1 power management and GPIO expander.
class M5StackPM1Component final : public gpio_expander::CachedGpioExpanderComponent<uint8_t, 8>, public i2c::I2CDevice {
 public:
  explicit M5StackPM1Component(bool fast_mode) : fast_mode_(fast_mode) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  /// GPIO of this expander that drives the host interrupt line.
  void set_interrupt_gpio(uint8_t pin) { this->interrupt_gpio_ = pin; }
  /// Enable or disable one pin's interrupt; the mask bit is set to disable it.
  void set_gpio_interrupt_enabled(uint8_t pin, bool enabled);

  /// A masked pin never raises an interrupt, so it has to refresh its own cache.
  bool digital_read(uint8_t pin) { return this->cached_read_(pin, (this->irq_masks_.gpio & (1 << pin)) != 0); }

  void pin_mode(uint8_t pin, gpio::Flags flags);
  bool set_pin_function(uint8_t pin, M5StackPM1PinFunction function);
  /// Store the wake setting applied from pin_mode() when the pin is set up as an input.
  void set_pin_wakeup(uint8_t pin, M5StackPM1WakeupEdge edge);

 protected:
  static constexpr uint32_t PIN_CONFIG_RETRY_ID = 0;
  static constexpr uint32_t PIN_CONFIG_RETRY_INTERVAL_MS = 1000;

  bool digital_read_hw(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  bool configure_i2c_speed_();
  void apply_pin_config_(uint8_t pin, gpio::Flags flags);
  void set_pin_config_pending_(uint8_t pin, bool pending);
  void retry_pin_config_();
  bool set_power_config_(uint8_t mask, bool enable);
  bool write_pin_field_(uint8_t pin, uint8_t reg0, uint8_t reg1, uint8_t value);
  bool apply_pin_wakeup_(uint8_t pin);
  bool read_device_info_();
  bool read_initial_state_();
  bool process_irq_();
  bool read_irq_status_(M5StackPM1IRQStatus *status);
  bool clear_irq_status_();
  bool set_irq_masks_(const M5StackPM1IRQStatus &masks);
  bool valid_pin_(uint8_t pin) const { return pin < M5STACK_PM1_GPIO_COUNT; }

  M5StackPM1DeviceInfo device_info_{};
  uint8_t output_mask_{0};
  uint8_t power_config_{0};
  uint8_t wakeup_mask_{0};
  uint8_t wakeup_edge_mask_{0};
  bool fast_mode_{false};
  // M5Stack PM1 IRQ masks use 1 to disable an event. GPIO IRQs are enabled by input pins during code generation.
  M5StackPM1IRQStatus irq_masks_{0x1F, 0x3F, 0x07};
  std::array<gpio::Flags, M5STACK_PM1_GPIO_COUNT> pin_flags_{};
  uint8_t pending_pin_config_mask_{0};
  uint8_t interrupt_gpio_{0xFF};
};

using M5StackPM1GPIOPin = gpio_expander::ExpanderGPIOPin<M5StackPM1Component>;

}  // namespace esphome::m5stack_pm1

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::m5stack_pm1::M5StackPM1Component);
