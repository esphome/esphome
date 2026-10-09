#pragma once

#include "esphome/components/uart/uart_virtual.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <span>
#include <utility>

namespace esphome::virtual_uart {

/// A UART configured in YAML: what its reader writes goes to on_tx, and inject_rx hands it bytes to read.
class VirtualUART final : public uart::VirtualUARTComponent, public Component {
 public:
  explicit VirtualUART(uint16_t rx_buffer_size) : VirtualUARTComponent(rx_buffer_size) {}

  template<typename F> void add_on_tx_callback(F &&callback) { this->tx_callback_.add(std::forward<F>(callback)); }

  void write_array(const uint8_t *data, size_t len) override;
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void dump_config() override;

  /// Logs a block that does not fit the RX ring.
  void inject(const uint8_t *data, size_t len);

 protected:
  LazyCallbackManager<void(std::span<const uint8_t>)> tx_callback_;
};

template<typename... Ts> class InjectRXAction final : public Action<Ts...>, public Parented<VirtualUART> {
  TEMPLATABLE_BYTES(data)

  void play(const Ts &...x) override {
    this->data_.visit([this](const uint8_t *data, size_t len) { this->parent_->inject(data, len); }, x...);
  }
};

}  // namespace esphome::virtual_uart
