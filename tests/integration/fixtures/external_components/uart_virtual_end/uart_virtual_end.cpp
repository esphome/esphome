#include "uart_virtual_end.h"

#include "esphome/core/log.h"

namespace esphome::uart_virtual_end {

static const char *const TAG = "uart_virtual_end";

void UartVirtualEnd::write_array(const uint8_t *data, size_t len) {
  ESP_LOGD(TAG, "%s TX %zu", this->label_, len);
  // What one end writes, its peer receives as one block.
  if (!this->peer_->inject_rx(data, len)) {
    ESP_LOGW(TAG, "%s RX full, dropped %zu", this->peer_->label_, len);
  }
}

}  // namespace esphome::uart_virtual_end
