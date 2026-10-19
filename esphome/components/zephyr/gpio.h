#pragma once

#ifdef USE_ZEPHYR
#include "esphome/core/gpio_pin.h"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
namespace esphome::zephyr {

// One POD struct so the static handler can recover the owning data from the
// callback pointer via CONTAINER_OF, avoiding a global pin->instance lookup table.
struct ZephyrGPIOInterrupt {
  struct gpio_callback callback;
  void (*func)(void *){nullptr};
  void *arg{nullptr};
};

class ZephyrGPIOPin final : public GPIOPin {
 public:
  // pin_name_prefix: secondary display label for chips with real GPIO port banks
  // (e.g. Nordic's "P0."/"P1."). Null/empty = flat numbering only.
  // zero_pad_pin: pad the pin-within-port number to 2 digits when printing it after
  // pin_name_prefix (Renesas RA's own "P106" notation, not "P16"). False for every
  // other scheme (lettered/dotted), which don't zero-pad.
  ZephyrGPIOPin(const device *gpio, int gpio_size, const char *pin_name_prefix = nullptr, bool zero_pad_pin = false) {
    this->gpio_ = gpio;
    this->gpio_size_ = gpio_size;
    this->pin_name_prefix_ = pin_name_prefix;
    this->zero_pad_pin_ = zero_pad_pin;
  }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;
  void detach_interrupt() const;
  template<typename T> void attach_interrupt(void (*func)(T *), T *arg, gpio::InterruptType type) const {
    this->attach_interrupt_(reinterpret_cast<void (*)(void *)>(func), arg, type);
  }
  ISRInternalGPIOPin to_isr() const;
#if defined(USE_ZEPHYR_VARIANT_FAMILY_NORDIC) || defined(USE_ZEPHYR_VARIANT_FAMILY_ESP32) || \
    defined(USE_ZEPHYR_VARIANT_FAMILY_STM32)
  /// Make the pin wake the chip from sys_poweroff() at the physical `level`; returns the Zephyr error code.
  int arm_wakeup(bool level);
#endif
  uint8_t get_pin() const { return this->pin_; }
  bool is_inverted() const { return this->inverted_; }
  bool is_internal() override { return true; }
  gpio::Flags get_flags() const override { return flags_; }

 protected:
  void attach_interrupt_(void (*func)(void *), void *arg, gpio::InterruptType type) const;
  const device *gpio_{nullptr};
  const char *pin_name_prefix_{nullptr};
  gpio::Flags flags_{};
  uint8_t pin_;
  uint8_t gpio_size_{};
  bool zero_pad_pin_{};
  bool inverted_{};
  bool value_{false};

  // attach_interrupt_()/detach_interrupt() are const, so their interrupt state is mutable.
  mutable ZephyrGPIOInterrupt interrupt_{};
};

}  // namespace esphome::zephyr

#endif  // USE_ZEPHYR
