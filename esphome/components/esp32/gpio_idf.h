#pragma once

#ifdef USE_ESP32_FRAMEWORK_ESP_IDF
#include "esphome/core/hal.h"
#include <driver/gpio.h>

namespace esphome {
namespace esp32 {

class IDFInternalGPIOPin : public InternalGPIOPin {
 public:
  void set_pin(gpio_num_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_drive_strength(gpio_drive_cap_t drive_strength) { drive_strength_ = drive_strength; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }

  void setup() override {
    pin_mode(flags_);
    gpio_set_drive_capability(pin_, drive_strength_);
  }
  void pin_mode(gpio::Flags flags) override {
    gpio_config_t conf{};
    conf.pin_bit_mask = 1ULL << static_cast<uint32_t>(pin_);
    conf.mode = flags_to_mode(flags);
    conf.pull_up_en = flags & gpio::FLAG_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    conf.pull_down_en = flags & gpio::FLAG_PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&conf);
  }
  bool digital_read() override { return bool(gpio_get_level(pin_)) != inverted_; }
  void digital_write(bool value) override { gpio_set_level(pin_, value != inverted_ ? 1 : 0); }
  std::string dump_summary() const override;
  void detach_interrupt() const override { gpio_intr_disable(pin_); }
  ISRInternalGPIOPin to_isr() const override;
  uint8_t get_pin() const override { return (uint8_t) pin_; }
  bool is_inverted() const override { return inverted_; }

 protected:
  static gpio_mode_t flags_to_mode(gpio::Flags flags) {
    flags = (gpio::Flags)(flags & ~(gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN));
    if (flags == gpio::FLAG_NONE) {
      return GPIO_MODE_DISABLE;
    } else if (flags == gpio::FLAG_INPUT) {
      return GPIO_MODE_INPUT;
    } else if (flags == gpio::FLAG_OUTPUT) {
      return GPIO_MODE_OUTPUT;
    } else if (flags == (gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
      return GPIO_MODE_OUTPUT_OD;
    } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) {
      return GPIO_MODE_INPUT_OUTPUT_OD;
    } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_OUTPUT)) {
      return GPIO_MODE_INPUT_OUTPUT;
    } else {
      // unsupported
      return GPIO_MODE_DISABLE;
    }
  }
  void attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const override {
    gpio_int_type_t idf_type = GPIO_INTR_ANYEDGE;
    switch (type) {
      case gpio::INTERRUPT_RISING_EDGE:
        idf_type = inverted_ ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;
        break;
      case gpio::INTERRUPT_FALLING_EDGE:
        idf_type = inverted_ ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE;
        break;
      case gpio::INTERRUPT_ANY_EDGE:
        idf_type = GPIO_INTR_ANYEDGE;
        break;
      case gpio::INTERRUPT_LOW_LEVEL:
        idf_type = inverted_ ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
        break;
      case gpio::INTERRUPT_HIGH_LEVEL:
        idf_type = inverted_ ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
        break;
    }
    gpio_set_intr_type(pin_, idf_type);
    gpio_intr_enable(pin_);
    if (!isr_service_installed) {
      gpio_install_isr_service(ESP_INTR_FLAG_LEVEL5);
      isr_service_installed = true;
    }
    gpio_isr_handler_add(pin_, func, arg);
  }

  gpio_num_t pin_;
  bool inverted_;
  gpio_drive_cap_t drive_strength_;
  gpio::Flags flags_;
  static bool isr_service_installed;
};

}  // namespace esp32
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ESP_IDF
