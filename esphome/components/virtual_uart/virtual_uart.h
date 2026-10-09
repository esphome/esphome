#pragma once

#include "esphome/components/uart/uart_virtual.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <span>
#include <utility>
#include <vector>

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
 public:
  void set_data_template(std::vector<uint8_t> (*func)(Ts...)) {
    this->code_.func = func;
    this->len_ = -1;
  }
  void set_data_static(const uint8_t *data, size_t len) {
    this->code_.data = data;
    this->len_ = len;
  }

  void play(const Ts &...x) override {
    if (this->len_ >= 0) {
      this->parent_->inject(this->code_.data, static_cast<size_t>(this->len_));
    } else {
      auto val = this->code_.func(x...);
      this->parent_->inject(val.data(), val.size());
    }
  }

 protected:
  ssize_t len_{-1};  // -1: template, else the length of static data in flash
  union Code {
    std::vector<uint8_t> (*func)(Ts...);
    const uint8_t *data;
  } code_;
};

}  // namespace esphome::virtual_uart
