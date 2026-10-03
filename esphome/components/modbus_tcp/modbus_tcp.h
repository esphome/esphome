#pragma once

#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::tcp_uart {
class TcpUart;
}  // namespace esphome::tcp_uart

namespace esphome::modbus_tcp {

/// RTU toward the modbus hub, Modbus TCP on a raw tcp_uart.
/// Client: a response is delivered only when it carries that request's transaction id.
/// Server: a request is delivered as one RTU frame, and the hub's reply uses that id.
class ModbusTcp : public uart::UARTComponent, public Component {
 public:
  ModbusTcp() { this->rx_buffer_size_ = RTU_FRAME_SIZE; }

  void set_parent(tcp_uart::TcpUart *parent) { this->parent_ = parent; }
  void set_server(bool server) { this->server_ = server; }

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

  static constexpr size_t TCP_FRAME_SIZE = 260;
  // One RTU frame. The hub reads it before the next request, so nothing else is waiting.
  static constexpr size_t RTU_FRAME_SIZE = 256;

  tcp_uart::TcpUart *parent_{nullptr};
  uint32_t last_drop_log_ms_{0};
  uint16_t txn_{0};
  uint16_t tcp_len_{0};
  uint16_t tx_len_{0};
  uint16_t rx_len_{0};
  // Client: set after a request is sent. Server: set after a request is delivered.
  // txn_ starts at 0. For a client that is not a request. For a server it may be.
  bool txn_pending_{false};
  bool server_{false};
  uint8_t tcp_buf_[TCP_FRAME_SIZE]{};
  uint8_t tx_[RTU_FRAME_SIZE]{};
  uint8_t rx_[RTU_FRAME_SIZE]{};
};

}  // namespace esphome::modbus_tcp
