#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MODBUS_TCP

#include "esphome/components/uart/uart_component.h"
#include "esphome/core/log.h"

#include <cstddef>
#include <cstdint>

namespace esphome::modbus {

enum class GatewayResult : uint8_t { GATEWAY_RESULT_SENT, GATEWAY_RESULT_LATER, GATEWAY_RESULT_DROPPED };

/// MBAP on one UART, one RTU frame toward the hub.
/// Client: a response is delivered only when it carries the transaction id of the request that was sent.
/// Server: the reply uses the id of the request the hub has not answered yet.
/// Gateway: the same wire is the server when a request arrives and the client when this hub sends one.
/// A frame the hub has not taken stays put. One it has taken, and not answered, is replaced.
/// A reply gets the unit id of the request it answers.
/// A bad MBAP with a usable length is skipped. Without a usable length the stream is discarded
/// until it has been quiet for RESYNC_QUIET_US, then parsing starts again.
/// Logs a warning at most once every 5 seconds per slot. True when it logged.
bool log_throttled(uint32_t &last_ms, const LogString *message);
/// Reads and drops whatever the UART has buffered.
void drain_uart(uart::UARTComponent *uart);

class MbapLink {
 public:
  explicit MbapLink(bool server);
  struct GatewayTag {};
  static constexpr GatewayTag GATEWAY{};
  explicit MbapLink(GatewayTag);

  void pump(uart::UARTComponent *uart);
  bool has_rtu() const { return this->rtu_len_ != 0; }
  size_t take_rtu(uint8_t *dst, size_t cap);
  /// Sends the frame, in pieces when the UART queue is short, or drops it with a warning.
  /// A frame partly sent is finished first. A new one meanwhile is dropped.
  void send_rtu(uart::UARTComponent *uart, const uint8_t *rtu, size_t len);

  /// Gateway only. response selects a matching reply; otherwise a new or replacement request.
  /// Bytes stay buffered until this is called, so an unread request is not parsed as something else.
  bool take_gateway(uart::UARTComponent *uart, uint8_t *dst, size_t cap, uint16_t *out_len, uint16_t *txn,
                    bool response);
  /// as_response writes the stored request id. A request allocates one.
  /// LATER commits nothing. FAILED after write_array of a request still owns the id, so the
  /// reply is consumed as that reply instead of parsed later as a new request.
  /// A frame the UART queue takes only in part is SENT. pump() sends the rest before the next frame.
  GatewayResult send_gateway(uart::UARTComponent *uart, const uint8_t *rtu, size_t len, bool as_response);
  void clear_rx();
  bool dropping() const { return this->resync_; }

 protected:
  enum class Role : uint8_t { ROLE_CLIENT, ROLE_SERVER, ROLE_GATEWAY };
  enum class Phase : uint8_t { PHASE_IDLE, PHASE_OWE_REPLY, PHASE_AWAIT };

  void reset_();
  void note_txn_(uint16_t got);
  void consume_(size_t used);
  /// Writes what the UART takes of the n bytes in tx_. True once the last one is written.
  bool write_tx_(uart::UARTComponent *uart, size_t n);
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
  Phase phase_{Phase::PHASE_IDLE};
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
