#include "virtual_uart.h"
#include "esphome/core/log.h"

namespace esphome::virtual_uart {

ESPHOME_LOG_TAG(TAG, "virtual_uart");

void VirtualUART::write_array(const uint8_t *data, size_t len) {
  // An on_tx that writes to this UART again would recurse until the stack runs out
  if (this->in_tx_) {
    if (!this->loop_logged_) {
      this->loop_logged_ = true;
      ESP_LOGW(TAG, "on_tx wrote to its own UART, dropped");
    }
    return;
  }
  this->debug_tx_(data, len);
  this->in_tx_ = true;
  this->tx_callback_.call(std::span<const uint8_t>(data, len));
  this->in_tx_ = false;
}

void VirtualUART::inject(const uint8_t *data, size_t len) {
  if (!this->inject_rx(data, len)) {
    ESP_LOGW(TAG, "No room for %zu bytes in the RX buffer, dropped", len);
  }
}

void VirtualUART::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Virtual UART:\n"
                "  Baud Rate: %" PRIu32 " baud\n"
                "  Data Bits: %u\n"
                "  Parity: %s\n"
                "  Stop bits: %u\n"
                "  RX Buffer Size: %zu",
                this->baud_rate_, this->data_bits_, LOG_STR_ARG(uart::parity_to_str(this->parity_)), this->stop_bits_,
                this->rx_buffer_size_);
}

}  // namespace esphome::virtual_uart
