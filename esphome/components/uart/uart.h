#pragma once

#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "uart_component.h"

namespace esphome {
namespace uart {

class UARTDevice {
 public:
  UARTDevice() = default;
  UARTDevice(UARTComponent *parent) : parent_(parent) {}

  void set_uart_parent(UARTComponent *parent) { this->parent_ = parent; }

  void write_byte(uint8_t data) { this->parent_->write_byte(data); }

  void write_array(const uint8_t *data, size_t len) { this->parent_->write_array(data, len); }
  void write_array(const std::vector<uint8_t> &data) { this->parent_->write_array(data); }
  template<size_t N> void write_array(const std::array<uint8_t, N> &data) {
    this->parent_->write_array(data.data(), data.size());
  }

  void write_str(const char *str) { this->parent_->write_str(str); }

  bool read_byte(uint8_t *data) { return this->parent_->read_byte(data); }
  bool peek_byte(uint8_t *data) { return this->parent_->peek_byte(data); }

  bool read_array(uint8_t *data, size_t len) { return this->parent_->read_array(data, len); }
  template<size_t N> optional<std::array<uint8_t, N>> read_array() {  // NOLINT
    std::array<uint8_t, N> res;
    if (!this->read_array(res.data(), N)) {
      return {};
    }
    return res;
  }

  int available() { return this->parent_->available(); }

  void flush() { return this->parent_->flush(); }

  // Compat APIs
  int read() {
    uint8_t data;
    if (!this->read_byte(&data))
      return -1;
    return data;
  }
  size_t write(uint8_t data) {
    this->write_byte(data);
    return 1;
  }
  int peek() {
    uint8_t data;
    if (!this->peek_byte(&data))
      return -1;
    return data;
  }

  /// Check that the configuration of the UART bus matches the provided values and otherwise print a warning
  void check_uart_settings(uint32_t baud_rate, uint8_t stop_bits = 1,
                           UARTParityOptions parity = UART_CONFIG_PARITY_NONE, uint8_t data_bits = 8);

 protected:
  UARTComponent *parent_{nullptr};
};

}  // namespace uart
}  // namespace esphome
