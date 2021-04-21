#include "uart_multi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uart_multi {

static const char *TAG = "uart_multi";

void UARTMulti::loop() {
  uint8_t byte;
  while (this->available()) {
    this->read_byte(&byte);
    for (auto *device : this->devices_)
      device->on_uart_multi_byte(byte);
  }
}

void UARTMulti::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Multi:");
  ESP_LOGCONFIG(TAG, "%u configured devices", this->devices_.size());  // NOLINT
}
float UARTMulti::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}
void UARTMulti::send(const std::vector<uint8_t> &data) { this->write_array(data); }

}  // namespace uart_multi
}  // namespace esphome
