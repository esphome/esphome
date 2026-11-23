#pragma once

#include "uart_component.h"
#include "esphome/core/helpers.h"

#include <cstddef>
#include <cstdint>

namespace esphome::uart {

/// Receive half of a UART without a wire. The derived class feeds the reader with inject_rx() and does the writing.
/// Reads never wait; a short read takes nothing. Main loop only.
class VirtualUARTComponent : public UARTComponent {
 public:
  /// The RX ring is allocated here, once; a later set_rx_buffer_size() does not resize it.
  explicit VirtualUARTComponent(uint16_t rx_buffer_size) {
    this->rx_buffer_size_ = rx_buffer_size;
    this->rx_.init(rx_buffer_size);
  }

  /// Adds one whole block to the RX ring; false, and nothing kept, when it does not fit.
  bool inject_rx(const uint8_t *data, size_t len) {
    if (len > this->rx_.free_space())
      return false;
    this->rx_.push(data, len);
    return true;
  }

  bool peek_byte(uint8_t *data) override {
    if (this->rx_.empty())
      return false;
    *data = this->rx_.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx_.size() < len)
      return false;
    this->rx_.pop(data, len);
    this->debug_(UART_DIRECTION_RX, data, len);
    return true;
  }
  size_t available() override { return this->rx_.size(); }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  using UARTComponent::load_settings;
  // Nothing is clocked, so there is nothing to apply.
  void load_settings(bool dump_config) override {}
#endif

 protected:
  void check_logger_conflict() override {}
  /// For write_array() of the derived class: hands the written bytes to a configured UART debugger.
  void debug_tx_(const uint8_t *data, size_t len) { this->debug_(UART_DIRECTION_TX, data, len); }
  void debug_(UARTDirection direction, const uint8_t *data, size_t len) {
#ifdef USE_UART_DEBUGGER
    for (size_t i = 0; i < len; i++)
      this->debug_callback_.call(direction, data[i]);
#endif
  }

  FixedRingBuffer<uint8_t> rx_;
};

}  // namespace esphome::uart
