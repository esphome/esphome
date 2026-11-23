#pragma once

#include "uart_component.h"
#include "esphome/core/helpers.h"

namespace esphome::uart {

#ifdef USE_UART_RX_SINK
/// Takes a block of bytes in one call, so the writer's frame boundaries survive.
class UARTSink {
 public:
  virtual void on_block(const uint8_t *data, size_t len) = 0;
};
#endif

/// UART without a wire: the derived class writes and feeds reads with inject_rx(). Reads never wait. Main loop only.
/// The RX_BUFFER_SIZE byte ring lives in the object; set_rx_buffer_size() does not resize it.
template<size_t RX_BUFFER_SIZE> class VirtualUARTComponent : public UARTComponent {
 public:
  VirtualUARTComponent() { this->rx_buffer_size_ = RX_BUFFER_SIZE; }

  /// Hands one whole block to the attached sink, else adds it to the RX ring. False, and nothing kept, when it does
  /// not fit or the sink is still inside on_block().
  bool inject_rx(const uint8_t *data, size_t len) {
#ifdef USE_UART_RX_SINK
    if (this->rx_sink_ != nullptr) {
      if (this->in_rx_sink_)
        return false;
      this->in_rx_sink_ = true;
      this->rx_sink_->on_block(data, len);
      this->in_rx_sink_ = false;
      return true;
    }
#endif
    if (len > this->rx_.free_space())
      return false;
    this->rx_.push(data, len);
    return true;
  }

#ifdef USE_UART_RX_SINK
  /// A reader that takes every received block as it arrives, so nothing waits in the ring.
  void set_rx_sink(UARTSink *sink) { this->rx_sink_ = sink; }
#endif

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
#ifdef USE_UART_DEBUGGER
    for (size_t i = 0; i < len; i++)
      this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
#endif
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
  void debug_tx_(const uint8_t *data, size_t len) {
#ifdef USE_UART_DEBUGGER
    for (size_t i = 0; i < len; i++)
      this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
#endif
  }

  StaticRingBuffer<uint8_t, RX_BUFFER_SIZE> rx_;
#ifdef USE_UART_RX_SINK
  UARTSink *rx_sink_{nullptr};
  bool in_rx_sink_{false};
#endif
};

}  // namespace esphome::uart
