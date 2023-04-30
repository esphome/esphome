#ifdef USE_ESP32

#include "gpio.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32";

bool ESP32InternalGPIOPin::isr_service_installed = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static gpio_mode_t IRAM_ATTR flags_to_mode(gpio::Flags flags) {
  flags = (gpio::Flags)(flags & ~(gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN));
  if (flags == gpio::FLAG_INPUT) {
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
    // unsupported or gpio::FLAG_NONE
    return GPIO_MODE_DISABLE;
  }
}

struct ISRPinArg {
  gpio_num_t pin;
  bool inverted;
};

ISRInternalGPIOPin ESP32InternalGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void ESP32InternalGPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
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
    auto res = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    if (res != ESP_OK) {
      ESP_LOGE(TAG, "attach_interrupt(): call to gpio_install_isr_service() failed, error code: %d", res);
      return;
    }
    isr_service_installed = true;
  }
  gpio_isr_handler_add(pin_, func, arg);
}

std::string ESP32InternalGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%" PRIu32, static_cast<uint32_t>(pin_));
  return buffer;
}

void ESP32InternalGPIOPin::setup() {
  gpio_config_t conf{};
  conf.pin_bit_mask = 1ULL << static_cast<uint32_t>(pin_);
  conf.mode = flags_to_mode(flags_);
  conf.pull_up_en = flags_ & gpio::FLAG_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  conf.pull_down_en = flags_ & gpio::FLAG_PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
  conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&conf);
  if (flags_ & gpio::FLAG_OUTPUT) {
    gpio_set_drive_capability(pin_, drive_strength_);
  }
}

void ESP32InternalGPIOPin::pin_mode(gpio::Flags flags) {
  // can't call gpio_config here because that logs in esp-idf which may cause issues
  gpio_set_direction(pin_, flags_to_mode(flags));
  gpio_pull_mode_t pull_mode = GPIO_FLOATING;
  if ((flags & gpio::FLAG_PULLUP) && (flags & gpio::FLAG_PULLDOWN)) {
    pull_mode = GPIO_PULLUP_PULLDOWN;
  } else if (flags & gpio::FLAG_PULLUP) {
    pull_mode = GPIO_PULLUP_ONLY;
  } else if (flags & gpio::FLAG_PULLDOWN) {
    pull_mode = GPIO_PULLDOWN_ONLY;
  }
  gpio_set_pull_mode(pin_, pull_mode);
}

bool ESP32InternalGPIOPin::digital_read() { return bool(gpio_get_level(pin_)) != inverted_; }
void ESP32InternalGPIOPin::digital_write(bool value) { gpio_set_level(pin_, value != inverted_ ? 1 : 0); }
void ESP32InternalGPIOPin::detach_interrupt() const { gpio_intr_disable(pin_); }

}  // namespace esp32

using namespace esp32;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  return bool(gpio_get_level(arg->pin)) != arg->inverted;
}
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  gpio_set_level(arg->pin, value != arg->inverted ? 1 : 0);
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  // not supported
}
void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  gpio_set_direction(arg->pin, flags_to_mode(flags));
  gpio_pull_mode_t pull_mode = GPIO_FLOATING;
  if ((flags & gpio::FLAG_PULLUP) && (flags & gpio::FLAG_PULLDOWN)) {
    pull_mode = GPIO_PULLUP_PULLDOWN;
  } else if (flags & gpio::FLAG_PULLUP) {
    pull_mode = GPIO_PULLUP_ONLY;
  } else if (flags & gpio::FLAG_PULLDOWN) {
    pull_mode = GPIO_PULLDOWN_ONLY;
  }
  gpio_set_pull_mode(arg->pin, pull_mode);
}

}  // namespace esphome

#endif  // USE_ESP32
