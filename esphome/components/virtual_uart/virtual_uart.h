#pragma once

#include "esphome/components/uart/uart_component.h"
#include <vector>
#include <queue>
#include <functional>

namespace esphome {
namespace virtual_uart {

class VirtualUARTComponent : public uart::UARTComponent, public Component {
 public:
  void set_tx_hook(std::function<void(const std::vector<uint8_t> &)> &&cb) { this->tx_hook_ = std::move(cb); }

  virtual void write_array(const uint8_t *data, size_t len) override {
    std::vector<uint8_t> buf(data, data + len);
    if (tx_hook_)
      tx_hook_(buf);
  }

  virtual bool peek_byte(uint8_t *data) override {
    if (rx_buffer_.empty())
      return false;
    *data = rx_buffer_.front();
    return true;
  }

  virtual bool read_array(uint8_t *data, size_t len) override {
    if (rx_buffer_.size() < len)
      return false;
    for (size_t i = 0; i < len; i++) {
      data[i] = rx_buffer_.front();
      rx_buffer_.pop();
    }
    return true;
  }

  virtual size_t available() override { return rx_buffer_.size(); }

  // For virtual UART, flush is a no-op since there's no actual hardware write buffer to flush.
  virtual void flush() override { return; }

  virtual void set_rx_full_threshold(size_t rx_full_threshold) { this->rx_full_threshold_ = rx_full_threshold; }
  virtual void set_rx_timeout(size_t rx_timeout) { this->rx_timeout_ = rx_timeout; }

  void inject_rx(const std::vector<uint8_t> &data) {
    for (uint8_t b : data)
      rx_buffer_.push(b);
  }
  void inject_rx(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++)
      rx_buffer_.push(data[i]);
  }

 protected:
  virtual void check_logger_conflict() override{};
  std::function<void(const std::vector<uint8_t> &)> tx_hook_;
  std::queue<uint8_t> rx_buffer_;
};

}  // namespace virtual_uart
}  // namespace esphome
