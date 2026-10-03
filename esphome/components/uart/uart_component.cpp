#include "uart_component.h"

#include <algorithm>

namespace esphome::uart {

static const char *const TAG = "uart";

bool UARTComponent::check_read_timeout_(size_t len) {
  if (this->available() >= len)
    return true;

  uint32_t start_time = millis();
  while (this->available() < len) {
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Reading from UART timed out at byte %zu!", this->available());
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

void UARTComponent::write_array_progmem(const uint8_t *data, size_t len) {
#ifdef USE_ESP8266
  uint8_t buf[32];
  while (len > 0) {
    const size_t chunk = std::min(len, sizeof(buf));
    progmem_memcpy(buf, data, chunk);
    this->write_array(buf, chunk);
    data += chunk;
    len -= chunk;
  }
#else
  this->write_array(data, len);
#endif
}

}  // namespace esphome::uart
