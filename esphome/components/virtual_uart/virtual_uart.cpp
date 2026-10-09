#include "virtual_uart.h"
#include "esphome/core/log.h"

namespace esphome::virtual_uart {

ESPHOME_LOG_TAG(TAG, "virtual_uart");

void VirtualUART::write_array(const uint8_t *data, size_t len) {
  this->debug_tx_(data, len);
  this->tx_callback_.call(std::span<const uint8_t>(data, len));
}

void VirtualUART::inject(const uint8_t *data, size_t len) {
  if (!this->inject_rx(data, len)) {
    ESP_LOGW(TAG, "RX buffer full, dropped %zu bytes", len);
  }
}

void VirtualUART::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Virtual UART:\n"
                "  Baud Rate: %" PRIu32 " baud\n"
                "  RX Buffer Size: %zu",
                this->baud_rate_, this->rx_buffer_size_);
}

}  // namespace esphome::virtual_uart
