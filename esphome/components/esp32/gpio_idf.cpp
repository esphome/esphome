#ifdef USE_ESP_IDF

#include "gpio_idf.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp32 {

static const char *const TAG = "esp32";

bool IDFInternalGPIOPin::isr_service_installed_ = false;

struct ISRPinArg {
  gpio_num_t pin;
  bool inverted;
};

ISRInternalGPIOPin IDFInternalGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

std::string IDFInternalGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", static_cast<uint32_t>(pin_));
  return buffer;
}

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

}  // namespace esphome

#endif  // USE_ESP_IDF
