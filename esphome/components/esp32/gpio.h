#include "esphome/core/esphal.h"
#include <driver/gpio.h>

namespace esphome {
namespace esp32 {

class ESP32InternalGPIOPin : public InternalGPIOPin {
 public:
  void set_pin(gpio_num_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_drive_strength(gpio_drive_cap_t drive_strength) { drive_strength_ = drive_strength; }
  void set_flags(GPIOFlags flags) { flags_ = flags; }

  void setup() override {
    pin_mode(flags_);
    gpio_set_drive_capability(pin_, drive_strength_);
  }
  void pin_mode(GPIOFlags flags) override {
    gpio_config_t conf{};
    conf.pin_bit_mask = 1 << static_cast<uint32_t>(pin_);
    conf.mode = flags_to_mode_(flags);
    conf.pull_up_en = flags & GPIOFlags::PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    conf.pull_down_en = flags & GPIOFlags::PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&conf);
    // TODO: check error
  }
  bool digital_read() override {
    return bool(gpio_get_level(pin_)) != inverted_;
  }
  void digital_write(bool value)  override {
    gpio_set_level(pin_, value != inverted_ ? 1 : 0);
  }
  void dump_config(const char *prefix) override;
  void detach_interrupt() const override {
    gpio_intr_disable(pin_);
  }
  ISRInternalGPIOPin to_isr() const override;

 protected:
  static gpio_mode_t flags_to_mode_(GPIOFlags flags) {
    if (flags == GPIOFlags::NONE) {
      return GPIO_MODE_DISABLE;
    } else if (flags == GPIOFlags::INPUT) {
      return GPIO_MODE_INPUT;
    } else if (flags == GPIOFlags::OUTPUT) {
      return GPIO_MODE_OUTPUT;
    } else if (flags == (GPIOFlags::OUTPUT | GPIOFlags::OPEN_DRAIN)) {
      return GPIO_MODE_OUTPUT_OD;
    } else if (flags == (GPIOFlags::INPUT | GPIOFlags::OUTPUT | GPIOFlags::OPEN_DRAIN)) {
      return GPIO_MODE_INPUT_OUTPUT_OD;
    } else if (flags == (GPIOFlags::INPUT | GPIOFlags::OUTPUT)) {
      return GPIO_MODE_INPUT_OUTPUT;
    } else {
      // unsupported
      return GPIO_MODE_DISABLE;
    }
  }
  void attach_interrupt_(void (*func)(void *), void *arg, GPIOInterruptType type) const override {
    gpio_int_type_t idf_type = GPIO_INTR_ANYEDGE;
    switch (type) {
      case GPIOInterruptType::RISING_EDGE:
        idf_type = inverted_ ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;
        break;
      case GPIOInterruptType::FALLING_EDGE:
        idf_type = inverted_ ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE;
        break;
      case GPIOInterruptType::ANY_EDGE:
        idf_type = GPIO_INTR_ANYEDGE;
        break;
      case GPIOInterruptType::LOW_LEVEL:
        idf_type = inverted_ ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
        break;
      case GPIOInterruptType::HIGH_LEVEL:
        idf_type = inverted_ ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
        break;
    }
    gpio_set_intr_type(pin_, idf_type);
    gpio_intr_enable(pin_);
    if (!isr_service_installed_) {
      gpio_install_isr_service(ESP_INTR_FLAG_LEVEL5);
      isr_service_installed_ = true;
    }
    gpio_isr_handler_add(pin_, func, arg);
  }

  gpio_num_t pin_;
  bool inverted_;
  gpio_drive_cap_t drive_strength_;
  GPIOFlags flags_;
  static bool isr_service_installed_;
};

}  // namespace esp32
}  // namespace esphome
