#include "gpio.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp32 {

bool ESP32InternalGPIOPin::isr_service_installed_ = false;

struct ISRPinArg {
  gpio_num_t pin;
  bool inverted;
};

ISRInternalGPIOPin ESP32InternalGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};
  arg->pin = pin_;
  arg->inverted = inverted_;
  return new ISRInternalGPIOPin((void *) arg);
}

}  // namespace esp32

using namespace esp32;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  return bool(gpio_get_level(arg->pin)) != inverted_;
}
void ISRInternalGPIOPin::digital_write(bool value) {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  gpio_set_level(arg->pin, value != arg->inverted ? 1 : 0);
}

}  // namespace esphome
