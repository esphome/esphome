#pragma once

#ifdef USE_ZEPHYR
#include "esphome/core/hal.h"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
namespace esphome::zephyr {

// Bundles the Zephyr gpio_callback together with the ESPHome ISR function and
// argument. Keeping them in one POD struct lets the static handler recover the
// owning data straight from the callback pointer via CONTAINER_OF, so no global
// pin->instance lookup table is needed.
struct ZephyrGPIOInterrupt {
  struct gpio_callback callback;
  void (*func)(void *){nullptr};
  void *arg{nullptr};
};

class ZephyrGPIOPin : public InternalGPIOPin {
 public:
  ZephyrGPIOPin(const device *gpio, int gpio_size, const char *pin_name_prefix) {
    this->gpio_ = gpio;
    this->gpio_size_ = gpio_size;
    this->pin_name_prefix_ = pin_name_prefix;
  }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;
  void detach_interrupt() const override;
  ISRInternalGPIOPin to_isr() const override;
  uint8_t get_pin() const override { return this->pin_; }
  bool is_inverted() const override { return this->inverted_; }
  gpio::Flags get_flags() const override { return flags_; }

 protected:
  void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const override;
  const device *gpio_{nullptr};
  const char *pin_name_prefix_{nullptr};
  gpio::Flags flags_{};
  uint8_t pin_;
  uint8_t gpio_size_{};
  bool inverted_{};
  bool value_{false};

  // attach_interrupt()/detach_interrupt() are const (matching the base class), so
  // the interrupt state they manage has to be mutable.
  mutable ZephyrGPIOInterrupt interrupt_{};
};

}  // namespace esphome::zephyr

#endif  // USE_ZEPHYR
