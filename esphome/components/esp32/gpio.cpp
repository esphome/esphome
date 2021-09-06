#include "gpio.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32";

bool ESP32InternalGPIOPin::isr_service_installed_ = false;

struct ISRPinArg {
  gpio_num_t pin;
  bool inverted;
};

ISRInternalGPIOPin ESP32InternalGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void ESP32InternalGPIOPin::dump_config(const char *prefix) {
    ESP_LOGCONFIG(TAG, "%sNumber: GPIO%u", prefix, static_cast<uint32_t>(pin_));
    if (inverted_) {
      ESP_LOGCONFIG(TAG, "%sInverted: Yes", prefix);
    }
    const char *drive_strength_s = "unknown";
    switch (drive_strength_) {
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
    ESP_LOGCONFIG(TAG, "%sDrive Strength: %s", prefix, drive_strength_s);
    bool in = flags_ & GPIOFlags::INPUT;
    bool out = flags_ & GPIOFlags::OUTPUT;
    if (in && out) {
      ESP_LOGCONFIG(TAG, "%sMode: INPUT_OUTPUT", prefix);
    } else if (in) {
      ESP_LOGCONFIG(TAG, "%sMode: INPUT", prefix);
    } else if (out) {
      ESP_LOGCONFIG(TAG, "%sMode: OUTPUT", prefix);
    }
    if (flags_ & GPIOFlags::OPEN_DRAIN) {
      ESP_LOGCONFIG(TAG, "%sOpen drain: enabled", prefix);
    }
    if (flags_ & GPIOFlags::PULLDOWN) {
      ESP_LOGCONFIG(TAG, "%sPulldown: enabled", prefix);
    }
    if (flags_ & GPIOFlags::PULLUP) {
      ESP_LOGCONFIG(TAG, "%sPullup: enabled", prefix);
    }
  }

}  // namespace esp32

using namespace esp32;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  return bool(gpio_get_level(arg->pin)) != arg->inverted;
}
void ISRInternalGPIOPin::digital_write(bool value) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  gpio_set_level(arg->pin, value != arg->inverted ? 1 : 0);
}

}  // namespace esphome
