#pragma once

#include "esphome/components/uart/uart_virtual.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include <cstdint>

namespace esphome::tcp_uart {
class TcpUart;
}  // namespace esphome::tcp_uart

namespace esphome::modbus_tcp_uart {

/// RTU toward the reader, Modbus TCP on a tcp_uart. Client: a response must match the request's transaction id.
/// Server: one request at a time; the reply gets its transaction id.
class ModbusTcpUart : public uart::VirtualUARTComponent, public Component {
 public:
  ModbusTcpUart() : VirtualUARTComponent(RTU_FRAME_SIZE) {}

  void set_parent(tcp_uart::TcpUart *parent) { this->parent_ = parent; }
  void set_server(bool server) { this->server_ = server; }
  void set_reply_timeout(uint32_t ms) { this->reply_timeout_ms_ = ms; }
  // Server: the units that the server devices on the hub answer.
  void set_units(const uint8_t *units, uint8_t count) {
    this->units_ = units;
    this->units_count_ = count;
  }

  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void write_array(const uint8_t *data, size_t len) override;
  size_t available_for_write() override;
  uart::UARTFlushResult flush() override;
  bool is_connected() override;

 protected:
  void read_parent_();
  void deliver_mbap_();
  void send_tx_(size_t held);
  void send_rtu_as_mbap_();
  void consume_tx_(size_t len);
  void clear_tx_();
  void discard_parent_();
  bool drop_log_due_();
  void note_drop_(const LogString *message);

  static constexpr size_t TCP_FRAME_SIZE = 260;
  // One RTU frame.
  static constexpr uint16_t RTU_FRAME_SIZE = 256;

  tcp_uart::TcpUart *parent_{nullptr};
  const uint8_t *units_{nullptr};
  // One rate limit for all drop warnings.
  uint32_t drop_log_ms_{0};
  uint32_t resync_from_ms_{0};
  // Server: how long the next request waits for the reply.
  uint32_t reply_timeout_ms_{1000};
  // Server: when the last request was handed to the hub.
  uint32_t request_ms_{0};
  uint16_t txn_{0};
  uint16_t tcp_len_{0};
  // tx_[0, tx_len_): written, not sent; a whole frame of tx_frame_len_ bytes at the front waits for room.
  uint16_t tx_len_{0};
  uint16_t tx_frame_len_{0};
  uint8_t units_count_{0};
  // The open request's unit and function.
  uint8_t unit_{0};
  uint8_t function_{0};
  // Client: a request was sent. Server: a request was delivered.
  bool txn_pending_{false};
  bool server_{false};
  bool tx_hold_logged_{false};
  bool link_was_up_{false};
  // After a header without a usable length: wait until the peer has been quiet.
  bool resync_{false};
  uint8_t tcp_buf_[TCP_FRAME_SIZE]{};
  uint8_t tx_[RTU_FRAME_SIZE]{};
};

}  // namespace esphome::modbus_tcp_uart
