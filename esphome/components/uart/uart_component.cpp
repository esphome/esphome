#include "uart_component.h"
#include "esphome/core/application.h"

#include <algorithm>

namespace esphome::uart {

ESPHOME_LOG_TAG(TAG, "uart");

// Keeps the pacing product in 32 bits up to about 10 Mbaud.
static constexpr uint32_t MAX_PACE_SPAN_MS = 4000;

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

size_t UARTComponent::paced_write_room(uint32_t last_write_ms) {
  size_t room = this->available_for_write();
  if (room != SIZE_MAX) {
    return room;
  }
  // A pass woken early writes little.
  uint32_t span =
      std::min({App.get_loop_component_start_time() - last_write_ms, App.get_loop_interval(), MAX_PACE_SPAN_MS});
  // 10 bits per byte on the line.
  uint32_t paced = this->baud_rate_ / 10 * span / 1000;
  return std::max<size_t>(1, paced);
}

void UARTComponent::set_rx_full_threshold_ms(uint8_t time) {
  uint8_t bytelength = this->data_bits_ + this->stop_bits_ + 1;
  if (this->parity_ != UARTParityOptions::UART_CONFIG_PARITY_NONE)
    bytelength += 1;
  int32_t val = clamp<int32_t>((this->baud_rate_ / (bytelength * 1000 / time)) - 1, 1, 120);
  this->set_rx_full_threshold(val);
}

}  // namespace esphome::uart
