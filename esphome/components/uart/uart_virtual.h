#pragma once

#include "uart_component.h"
#include "esphome/core/helpers.h"

#include <cstddef>
#include <cstdint>

namespace esphome::uart {

/// Receive half of a UART without a wire: the class that derives from it feeds its reader with inject_rx() and
/// implements write_array(), available_for_write() and flush() itself.
/// Reads never wait: a short read_array() or an empty peek_byte() returns false and consumes nothing.
/// read_array() of 0 bytes returns true, as on ESP8266, RP2040 and LibreTiny (ESP-IDF and host return false).
/// Main loop only.
class VirtualUARTComponent : public UARTComponent {
 public:
  /// The RX ring is allocated here, once.
  explicit VirtualUARTComponent(uint16_t rx_buffer_size);

  /// One whole block for this UART's reader, e.g. one frame, into the RX ring. Returns false when the ring has no
  /// room for all of it; nothing is kept then.
  bool inject_rx(const uint8_t *data, size_t len);

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

  FixedRingBuffer<uint8_t> rx_;
};

}  // namespace esphome::uart
