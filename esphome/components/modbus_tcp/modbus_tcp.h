#pragma once

#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::tcp_uart {
class TcpUart;
}  // namespace esphome::tcp_uart

namespace esphome::modbus_tcp {

/// RTU toward the modbus hub, Modbus TCP on a raw tcp_uart.
/// Master side only: a response must carry the transaction id of the last request.
class ModbusTcp : public uart::UARTComponent, public Component {
 public:
  ModbusTcp() { this->rx_buffer_size_ = RX_BUFFER_SIZE; }

  void set_parent(tcp_uart::TcpUart *parent) { this->parent_ = parent; }

  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override { return static_cast<size_t>(this->rx_end_ - this->rx_start_); }
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
  void queue_rtu_(const uint8_t *data, size_t len);
  void send_rtu_as_mbap_();

  static constexpr size_t RX_BUFFER_SIZE = 1024;
  static constexpr size_t TCP_FRAME_SIZE = 260;
  static constexpr size_t RTU_FRAME_SIZE = 256;

  tcp_uart::TcpUart *parent_{nullptr};
  uint32_t last_drop_log_ms_{0};
  uint16_t txn_{0};
  uint16_t tcp_len_{0};
  uint16_t tx_len_{0};
  uint16_t rx_start_{0};
  uint16_t rx_end_{0};
  uint8_t tcp_buf_[TCP_FRAME_SIZE]{};
  uint8_t tx_[RTU_FRAME_SIZE]{};
  uint8_t rx_[RX_BUFFER_SIZE]{};
};

}  // namespace esphome::modbus_tcp
