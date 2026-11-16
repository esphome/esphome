#ifdef USE_HOST

#include "gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace host {

ESPHOME_LOG_TAG(TAG, "host");

ISRInternalGPIOPin HostGPIOPin::to_isr() const { return ISRInternalGPIOPin(this); }

void HostGPIOPin::attach_interrupt_(void (*func)(void *), void *arg, gpio::InterruptType type) const {
  ESP_LOGD(TAG, "Attaching interrupt %p to pin %d and mode %d", func, pin_, (uint32_t) type);
}
void HostGPIOPin::pin_mode(gpio::Flags flags) { ESP_LOGD(TAG, "Setting pin %d mode to %02X", pin_, (uint32_t) flags); }

size_t HostGPIOPin::dump_summary(char *buffer, size_t len) const { return snprintf(buffer, len, "GPIO%u", this->pin_); }

bool HostGPIOPin::digital_read() { return inverted_; }
void HostGPIOPin::digital_write(bool value) {
  // pass
  ESP_LOGD(TAG, "Setting pin %d to %s", pin_, value != inverted_ ? "HIGH" : "LOW");
}
void HostGPIOPin::detach_interrupt() const {}

}  // namespace host

using namespace host;

bool IRAM_ATTR ISRInternalGPIOPin::digital_read() { return static_cast<HostGPIOPin *>(this->arg_)->is_inverted(); }
void IRAM_ATTR ISRInternalGPIOPin::digital_write(bool value) {
  // pass
}
void IRAM_ATTR ISRInternalGPIOPin::clear_interrupt() {
  ESP_LOGD(TAG, "Clearing interrupt for pin %d", static_cast<HostGPIOPin *>(this->arg_)->get_pin());
}
void IRAM_ATTR ISRInternalGPIOPin::pin_mode(gpio::Flags flags) {
  // pass
}

}  // namespace esphome

#endif  // USE_HOST
