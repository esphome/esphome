#ifdef USE_HOST

#include "gpio.h"
#include "esphome/core/log.h"

namespace esphome {
namespace host {

static const char *const TAG = "host";

struct ISRPinArg {
  uint8_t pin;
  bool inverted;
};

ISRInternalGPIOPin HostGPIOPin::to_isr() const {
  auto *arg = new ISRPinArg{};  // NOLINT(cppcoreguidelines-owning-memory)
  arg->pin = pin_;
  arg->inverted = inverted_;
  return ISRInternalGPIOPin((void *) arg);
}

void HostGPIOPin::attach_interrupt(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  ESP_LOGD(TAG, "Attaching interrupt %p to pin %d and mode %d", func, pin_, (uint32_t) type);
}
void HostGPIOPin::pin_mode(gpio::Flags flags) { ESP_LOGD(TAG, "Setting pin %d mode to %02X", pin_, (uint32_t) flags); }

std::string HostGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "GPIO%u", pin_);
  return buffer;
}

bool HostGPIOPin::digital_read() { return inverted_; }
void HostGPIOPin::digital_write(bool value) {
  // pass
  ESP_LOGD(TAG, "Setting pin %d to %s", pin_, value != inverted_ ? "HIGH" : "LOW");
}
void HostGPIOPin::detach_interrupt() const {}

}  // namespace host

using namespace host;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  return arg->inverted;
}
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  // pass
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  auto *arg = reinterpret_cast<ISRPinArg *>(arg_);
  ESP_LOGD(TAG, "Clearing interrupt for pin %d", arg->pin);
}

}  // namespace esphome

#endif  // USE_HOST
