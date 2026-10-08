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
  if (len > static_cast<size_t>(this->rx_.capacity() - this->rx_.size())) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    this->rx_.push(data[i]);
  }
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
  for (size_t i = 0; i < len; i++) {
    data[i] = this->rx_.front();
    this->rx_.pop();
#ifdef USE_UART_DEBUGGER
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
#endif
  }
  return true;
}

}  // namespace esphome::uart

#endif  // USE_UART_VIRTUAL
