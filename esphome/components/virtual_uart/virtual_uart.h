#pragma once

#include "esphome/components/uart/uart_virtual.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <span>
#include <utility>

namespace esphome::virtual_uart {

// Not templated, so the log strings exist once for all RX sizes
void log_own_write_dropped();
void log_inject_dropped(size_t len);
void dump_virtual_uart_config(uart::UARTComponent *component);

/// A UART configured in YAML: what its reader writes goes to on_tx, and inject_rx hands it bytes to read.
template<size_t RX_BUFFER_SIZE>
class VirtualUART final : public uart::VirtualUARTComponent<RX_BUFFER_SIZE>, public Component {
 public:
  template<typename F> void add_on_tx_callback(F &&callback) { this->tx_callback_.add(std::forward<F>(callback)); }

  void write_array(const uint8_t *data, size_t len) override {
    // An on_tx that writes to this UART again would recurse until the stack runs out
    if (this->in_tx_) {
      log_own_write_dropped();
      return;
    }
    this->in_tx_ = true;
    this->debug_tx_(data, len);
    this->tx_callback_.call(std::span<const uint8_t>(data, len));
    this->in_tx_ = false;
  }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void dump_config() override { dump_virtual_uart_config(this); }

  /// Logs a block that does not fit the RX ring.
  void inject(const uint8_t *data, size_t len) {
    if (!this->inject_rx(data, len)) {
      log_inject_dropped(len);
    }
  }
  /// inject() for a constant payload in flash on ESP8266, read without a heap copy.
  void inject_progmem(const uint8_t *data, size_t len) {
    if (len > this->rx_.free_space()) {
      log_inject_dropped(len);
      return;
    }
    for (size_t i = 0; i < len; i++)
      this->rx_.push(progmem_read_byte(data + i));
  }

 protected:
  CallbackManager<void(std::span<const uint8_t>)> tx_callback_;
  bool in_tx_{false};
};

}  // namespace esphome::virtual_uart
