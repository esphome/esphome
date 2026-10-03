#pragma once

#include "mbap.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/socket/tcp_client_link.h"
#ifdef USE_SOCKET_TCP_LISTENER
#include "esphome/components/socket/tcp_listener.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome::modbus_tcp {

/// Hardware UART on one side, Modbus TCP on a socket this component opens.
/// One transaction at a time. uart_tcp is not in this path.
class ModbusTcpUart : public Component, public uart::UARTDevice {
 public:
  void set_host(const char *host) { this->link_.set_host(host); }
  void set_port(uint16_t port) { this->link_.set_port(port); }
  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }
  void set_timeout(uint32_t ms) { this->timeout_ms_ = ms; }
  void set_send_wait_time(uint32_t ms) { this->send_wait_ms_ = ms; }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }
#ifdef USE_SOCKET_TCP_LISTENER
  void set_server(bool server) { this->server_ = server; }
#ifdef USE_SOCKET_IPV4_ALLOW
  void set_allow(const socket::Ipv4AllowEntry *entries, size_t count) { this->listener_.set_allow(entries, count); }
#endif
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  bool maintain_link_();
  void sync_link_();
  void check_timeout_();
  void note_io_();
  void note_uart_();
  void discard_uart_();
  void pump_modbus_();
  void read_tcp_buf_();
  // Copies the PDU out before the TCP buffer slides.
  MbapTake take_tcp_(uint8_t *pdu, size_t cap, size_t *pdu_len, uint16_t *txn, uint8_t *unit);
  void pull_uart_buf_();
  bool take_rtu_(uint8_t *pdu, size_t *pdu_len, uint8_t *unit);
  bool write_rtu_(const uint8_t *pdu, size_t pdu_len, uint8_t unit);
  bool drain_rtu_();
  bool send_mbap_(uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len);
  void send_exception_();
  uint32_t frame_gap_us_() const;

  static constexpr size_t READ_CHUNK = 128;
  static constexpr size_t TCP_FRAME_SIZE = 260;
  static constexpr size_t RTU_FRAME_SIZE = 256;
  static constexpr size_t PDU_MAX = 253;

  socket::TcpClientLink link_;
#ifdef USE_SOCKET_TCP_LISTENER
  socket::TcpListener listener_;
#endif
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  uint32_t timeout_ms_{0};
  uint32_t last_io_ms_{0};
  uint32_t last_uart_us_{0};
  uint32_t wait_started_ms_{0};
  uint32_t send_wait_ms_{2000};
  uint32_t last_drop_log_ms_{0};
  uint16_t txn_{0};
  uint16_t tcp_len_{0};
  uint16_t uart_len_{0};
  uint16_t rtu_tx_len_{0};
  uint16_t rtu_tx_off_{0};
  uint8_t pending_unit_{0};
  uint8_t pending_function_{0};
  bool server_{false};
  bool link_was_up_{false};
  bool wait_uart_{false};
  bool wait_tcp_{false};
  // Set while the request is still leaving the UART driver.
  bool arm_wait_uart_{false};
  uint8_t tcp_buf_[TCP_FRAME_SIZE]{};
  uint8_t uart_buf_[RTU_FRAME_SIZE]{};
  uint8_t rtu_tx_[RTU_FRAME_SIZE]{};
};

}  // namespace esphome::modbus_tcp
