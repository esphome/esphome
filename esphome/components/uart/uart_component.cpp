#include "uart_component.h"

namespace esphome::uart {

static const char *const TAG = "uart";

bool UARTComponent::check_read_timeout_(size_t len) {
  if (this->available() >= int(len))
    return true;

  uint32_t start_time = millis();
  while (this->available() < int(len)) {
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Reading from UART timed out at byte %u!", this->available());
      return false;
    }
    yield();
  }
  return true;
}

void UARTComponent::set_rx_full_threshold_ms(uint8_t time) {
  uint8_t bytelength = this->data_bits_ + this->stop_bits_ + 1;
  if (this->parity_ != UARTParityOptions::UART_CONFIG_PARITY_NONE)
    bytelength += 1;
  int32_t val = clamp<int32_t>((this->baud_rate_ / (bytelength * 1000 / time)) - 1, 1, 120);
  this->set_rx_full_threshold(val);
}

}  // namespace esphome::uart
