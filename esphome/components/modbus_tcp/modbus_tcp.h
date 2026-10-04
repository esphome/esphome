#pragma once

#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::tcp_uart {
class TcpUart;
}  // namespace esphome::tcp_uart

namespace esphome::modbus_tcp {

/// RTU toward the modbus hub, Modbus TCP on a raw tcp_uart.
/// Client: a response is delivered only when it carries that request's transaction id, with the request's unit.
/// Server: one request at a time is delivered as one RTU frame, and the reply uses that id. The next stays in the
/// TCP buffer until the reply went out or REPLY_TIMEOUT_MS passed. A reply must match the request's unit and function.
/// With a unit list, a request to another unit is dropped, since no other device on this link answers it.
/// A bad MBAP with a usable length is skipped. Without one, bytes are dropped until the peer has been quiet.
class ModbusTcp : public uart::UARTComponent, public Component {
 public:
  ModbusTcp() { this->rx_buffer_size_ = RTU_FRAME_SIZE; }

  void set_parent(tcp_uart::TcpUart *parent) { this->parent_ = parent; }
  void set_server(bool server) { this->server_ = server; }
  // Server: the units that the server devices on the hub answer.
  void set_units(const uint8_t *units, uint8_t count) {
    this->units_ = units;
    this->units_count_ = count;
  }

  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override { return this->rx_len_; }
  size_t available_for_write() override;
  uart::UARTFlushResult flush() override;
  bool is_connected() override;
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

 protected:
  void check_logger_conflict() override {}
  void read_parent_();
  void deliver_mbap_();
  void send_rtu_as_mbap_();
  void clear_tx_();
  void discard_parent_();

  // One stamp per message. A stale transaction must not hide a failed send.
  enum DropKind : uint8_t {
    DROP_INCOMPLETE = 0,
    DROP_REPLY_PREVIOUS,
    DROP_HELD,
    DROP_TOO_LONG,
    DROP_READ,
    DROP_REPLACED,
    DROP_STALE,
    DROP_RX_FULL,
    DROP_NOT_CONNECTED,
    DROP_NO_REQUEST,
    DROP_ENCODE,
    DROP_SEND,
    DROP_BAD_MBAP,
    DROP_KIND_COUNT,
  };

  static constexpr size_t TCP_FRAME_SIZE = 260;
  // One RTU frame. The hub reads it before the next request, so nothing else is waiting.
  static constexpr size_t RTU_FRAME_SIZE = 256;
  // Server: how long the next request waits for the reply to the current one.
  static constexpr uint32_t REPLY_TIMEOUT_MS = 1000;

  tcp_uart::TcpUart *parent_{nullptr};
  const uint8_t *units_{nullptr};
  uint32_t drop_log_ms_[DROP_KIND_COUNT]{};
  // When tx_ holds an unfinished frame. A later write can tell a pause from a copy still in progress.
  uint32_t tx_partial_ms_{0};
  uint32_t resync_from_us_{0};
  // Server: when the open request was delivered.
  uint32_t request_ms_{0};
  uint16_t txn_{0};
  uint16_t tcp_len_{0};
  uint16_t tx_len_{0};
  uint16_t rx_len_{0};
  uint8_t units_count_{0};
  // The open request's unit. Client: the response's RTU address. Server: with the function, it marks the reply.
  uint8_t unit_{0};
  uint8_t function_{0};
  // Client: set after a request is sent. Server: set after a request is delivered.
  // txn_ starts at 0. For a client that is not a request. For a server it may be.
  bool txn_pending_{false};
  bool server_{false};
  // The hold warning is logged once per frame, so it does not hide a later drop.
  bool tx_hold_logged_{false};
  // Edge for the disconnect log.
  bool link_was_up_{false};
  // A header without a usable length. Nothing is parsed until the peer has been quiet.
  bool resync_{false};
  uint8_t tcp_buf_[TCP_FRAME_SIZE]{};
  uint8_t tx_[RTU_FRAME_SIZE]{};
  uint8_t rx_[RTU_FRAME_SIZE]{};
};

}  // namespace esphome::modbus_tcp
