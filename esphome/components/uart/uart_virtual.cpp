#include "esphome/core/defines.h"
#ifdef USE_UART_VIRTUAL

#include "uart_virtual.h"

namespace esphome::uart {

VirtualUARTComponent::VirtualUARTComponent(uint16_t rx_buffer_size) {
  this->rx_buffer_size_ = rx_buffer_size;
  if (rx_buffer_size != 0) {
    this->rx_.init(rx_buffer_size);
  }
}

bool VirtualUARTComponent::inject_rx(const uint8_t *data, size_t len) {
  if (len > this->rx_.free_space()) {
    return false;
  }
  this->rx_.push(data, len);
  return true;
}

bool VirtualUARTComponent::peek_byte(uint8_t *data) {
  if (this->rx_.empty()) {
    return false;
  }
  *data = this->rx_.front();
  return true;
}

bool VirtualUARTComponent::read_array(uint8_t *data, size_t len) {
  if (this->rx_.size() < len) {
    return false;
  }
  this->rx_.pop(data, len);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++)
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
#endif
  return true;
}

}  // namespace esphome::uart

#endif  // USE_UART_VIRTUAL
