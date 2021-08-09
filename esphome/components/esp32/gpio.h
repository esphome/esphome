#include "esphome/core/esphal.h"
#include "esphome/core/log.h"
#include "esphome/core/optional.h"
#include <driver/gpio.h>

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32";

class ESP32InternalGPIOPin : public InternalGPIOPin {
 public:
  void set_pin(gpio_num_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_drive_strength(optional<gpio_drive_cap_t> drive_strength) { drive_strength_ = drive_strength; }
  void set_flags(GPIOFlags flags) { flags_ = flags; }

  void setup() override {
    pin_mode(flags_);
    if (drive_strength_.has_value()) {
      gpio_set_drive_capability(pin_, *drive_strength_);
    }
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
  void dump_config() override {
    ESP_LOGCONFIG(TAG, "    Pin: GPIO%u", static_cast<uint32_t>(pin_));
    if (inverted_) {
      ESP_LOGCONFIG(TAG, "    Inverted: Yes");
    }
    if (drive_strength_.has_value()) {
      const char *drive_strength_s = "unknown";
      switch (*drive_strength_) {
        case GPIO_DRIVE_CAP_0:
          drive_strength_s = "~5 mA";
          break;
        case GPIO_DRIVE_CAP_1:
          drive_strength_s = "~10 mA";
          break;
        case GPIO_DRIVE_CAP_2:
          drive_strength_s = "~20 mA";
          break;
        case GPIO_DRIVE_CAP_3:
          drive_strength_s = "~40 mA";
          break;
        default:
          break;
      }
      ESP_LOGCONFIG(TAG, "    Drive Strength: %s", drive_strength_s);
    }
    bool in = flags_ & GPIOFlags::INPUT;
    bool out = flags_ & GPIOFlags::OUTPUT;
    if (in && out) {
      ESP_LOGCONFIG(TAG, "    Mode: INPUT_OUTPUT");
    } else if (in) {
      ESP_LOGCONFIG(TAG, "    Mode: INPUT");
    } else if (out) {
      ESP_LOGCONFIG(TAG, "    Mode: OUTPUT");
    }
    if (flags_ & GPIOFlags::OPEN_DRAIN) {
      ESP_LOGCONFIG(TAG, "    Open drain: enabled");
    }
    if (flags_ & GPIOFlags::PULLDOWN) {
      ESP_LOGCONFIG(TAG, "    Pulldown: enabled");
    }
    if (flags_ & GPIOFlags::PULLUP) {
      ESP_LOGCONFIG(TAG, "    Pullup: enabled");
    }
  }
  void detach_interrupt() const override {
    gpio_intr_disable(pin_);
  }
  ISRInternalGPIOPin *to_isr() const override;

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
    }
  }
  void attach_interrupt_(void (*func)(void *), void *arg, GPIOInterruptType type) const override {
    gpio_int_type_t idf_type;
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
  optional<gpio_drive_cap_t> drive_strength_{};
  GPIOFlags flags_;
  static bool isr_service_installed_;
};

}  // namespace esp32
}  // namespace esphome
