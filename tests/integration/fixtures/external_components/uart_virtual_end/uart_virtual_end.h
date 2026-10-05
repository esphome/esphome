#pragma once

// Host-only test component: one end of a virtual null-modem pair that logs the size of every write.

#include "esphome/components/uart/uart_virtual.h"
#include "esphome/core/component.h"

namespace esphome::uart_virtual_end {

class UartVirtualEnd : public uart::VirtualUARTComponent, public Component {
 public:
  UartVirtualEnd() : VirtualUARTComponent(256) {}

  void set_label(const char *label) { this->label_ = label; }
  void set_peer(UartVirtualEnd *peer) { this->peer_ = peer; }

  void write_array(const uint8_t *data, size_t len) override;
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }

 protected:
  const char *label_{""};
  UartVirtualEnd *peer_{nullptr};
};

}  // namespace esphome::uart_virtual_end
