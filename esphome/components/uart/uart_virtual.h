#pragma once

#include "uart_component.h"
#include "esphome/core/helpers.h"

#include <cstddef>
#include <cstdint>

namespace esphome::uart {

/// Takes a block of bytes in one call, so the writer's frame boundaries survive.
class UARTSink {
 public:
  virtual void on_block(const uint8_t *data, size_t len) = 0;
};

/// Receive half of a UART without a wire. The derived class feeds the reader with inject_rx() and does the writing.
/// Reads never wait; a short read takes nothing. Main loop only.
class VirtualUARTComponent : public UARTComponent {
 public:
  /// The RX ring is allocated here, once; 0 means no ring.
  explicit VirtualUARTComponent(uint16_t rx_buffer_size);

  /// Hands one whole block to the attached reader, else adds it to the RX ring. False, and nothing kept, when it
  /// does not fit or the reader is still inside on_block().
  bool inject_rx(const uint8_t *data, size_t len);
  /// A reader that takes every received block as it arrives, so nothing waits in the ring.
  void set_rx_sink(UARTSink *sink) { this->rx_sink_ = sink; }

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
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

  UARTSink *rx_sink_{nullptr};
  FixedRingBuffer<uint8_t> rx_;
  bool in_rx_sink_{false};
};

}  // namespace esphome::uart
