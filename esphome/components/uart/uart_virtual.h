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

/// Receive half of a UART without a wire: the class that derives from it feeds its reader with inject_rx() and
/// implements write_array(), available_for_write() and flush() itself.
/// Reads never wait: a short read_array() or an empty peek_byte() returns false and consumes nothing.
/// read_array() of 0 bytes returns true, as on ESP8266, RP2040 and LibreTiny (ESP-IDF and host return false).
/// Main loop only.
class VirtualUARTComponent : public UARTComponent {
 public:
  /// The RX ring is allocated here, once; 0 means nothing is kept for a reader that is not attached.
  explicit VirtualUARTComponent(uint16_t rx_buffer_size);

  /// One whole block for this UART's reader, e.g. one frame: an attached reader gets it in one on_block() call,
  /// else it goes into the RX ring. Returns false when the ring has no room for all of it (nothing is kept then),
  /// or when the attached reader is still inside on_block() for an earlier block (a reader that writes back can be
  /// handed a block again; it is refused rather than recursing).
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

  UARTSink *rx_sink_{nullptr};
  FixedRingBuffer<uint8_t> rx_;
  bool in_rx_sink_{false};
};

}  // namespace esphome::uart
