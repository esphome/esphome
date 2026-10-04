#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MODBUS_TCP

#include "esphome/components/uart/uart_component.h"
#include "esphome/core/log.h"

#include <cstddef>
#include <cstdint>

namespace esphome::modbus {

/// MBAP on one UART, one RTU frame toward the hub.
/// Client: a response is delivered only when it carries the transaction id of the request that was sent.
/// Server: the reply uses the id of the request the hub has not answered yet.
/// A frame the hub has not taken stays put. One it has taken, and not answered, is replaced.
/// A reply gets the unit id of the request it answers.
/// A bad MBAP with a usable length is skipped. Without a usable length the stream is discarded
/// until it has been quiet for RESYNC_QUIET_US, then parsing starts again.
/// Logs a warning at most once every 5 seconds per slot.
void log_throttled(uint32_t &last_ms, const LogString *message);
/// Reads and drops whatever the UART has buffered.
void drain_uart(uart::UARTComponent *uart);

class MbapLink {
 public:
  explicit MbapLink(bool server);

  void pump(uart::UARTComponent *uart);
  bool has_rtu() const { return this->rtu_len_ != 0; }
  size_t take_rtu(uint8_t *dst, size_t cap);
  /// Sends the frame, in pieces when the UART queue is short, or drops it with a warning.
  /// A frame partly sent is finished first. A new one meanwhile is dropped.
  void send_rtu(uart::UARTComponent *uart, const uint8_t *rtu, size_t len);

 protected:
  enum class Role : uint8_t { ROLE_CLIENT, ROLE_SERVER };

  void reset_();
  void note_txn_(uint16_t got);
  void consume_(size_t used);
  void flush_held_(uart::UARTComponent *uart);
  void bad_mbap_(uart::UARTComponent *uart);
  /// False while a skipped frame or a resync still holds back new bytes.
  bool skip_or_resync_(uart::UARTComponent *uart);
  /// True when a TCP frame was consumed and another may follow. False when the caller should stop.
  bool parse_(uart::UARTComponent *uart);

  static constexpr size_t TCP_FRAME_SIZE = 260;
  static constexpr size_t RTU_FRAME_SIZE = 256;
  static constexpr uint32_t RESYNC_QUIET_US = 100000;

  uint32_t drop_bad_ms_{0};
  uint32_t drop_stale_ms_{0};
  uint32_t drop_other_ms_{0};
  uint16_t txn_{0};
  uint16_t tcp_len_{0};
  uint16_t rtu_len_{0};
  uint16_t held_len_{0};
  // Bytes of tx_ already written. The rest of the frame goes before anything else.
  uint16_t tx_sent_{0};
  uint16_t skip_left_{0};
  uint32_t resync_from_us_{0};
  // Unit id of the request in flight. Its reply carries it, whatever unit id the server wrote.
  uint8_t unit_{0};
  Role role_{Role::ROLE_CLIENT};
  bool txn_pending_{false};
  bool resync_{false};
  bool link_was_up_{false};
  uint8_t tcp_buf_[TCP_FRAME_SIZE]{};
  uint8_t rtu_[RTU_FRAME_SIZE]{};
  uint8_t held_[RTU_FRAME_SIZE]{};
  uint8_t tx_[TCP_FRAME_SIZE]{};
};

}  // namespace esphome::modbus

#endif  // USE_MODBUS_TCP
