#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/socket/ipv4_resolve.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

#include <cstdint>
#include <memory>

namespace esphome::uart_tcp {

/// Copies raw bytes between one hardware UART and one TCP socket.
class UartTcp : public Component, public uart::UARTDevice {
 public:
  void set_host(const char *host) { this->host_ = StringRef(host); }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_server(bool server) { this->server_ = server; }
  void set_reconnect_interval(uint32_t ms) { this->reconnect_interval_ms_ = ms; }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void close_sock_();
  void close_listen_();
  void try_connect_();
  void try_listen_();
  void accept_client_();
  void read_socket_();
  void read_uart_();
  void apply_socket_options_(socket::Socket *sock);
  void set_link_up_(bool up);
  void note_attempt_() { this->last_attempt_ms_ = App.get_loop_component_start_time(); }
  bool in_backoff_() const {
    return App.get_loop_component_start_time() - this->last_attempt_ms_ < this->reconnect_interval_ms_;
  }
  void flush_tx_();

  static constexpr size_t TX_BUFFER_SIZE = 1024;
  static constexpr size_t READ_CHUNK = 128;

  // 4-byte members, then the port, then the flags, then the byte buffer.
  StringRef host_;
  std::unique_ptr<socket::Socket> sock_;
  std::unique_ptr<socket::ListenSocket> listen_;
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  uint32_t last_attempt_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  size_t tx_len_{0};
  socket::Ipv4Resolve resolved_;

  uint16_t port_{0};
  bool server_{false};
  bool connecting_{false};
  bool connected_{false};
  // A read stopped before EAGAIN. ready() stays false until new data arrives.
  bool rx_pending_{false};
  uint8_t tx_[TX_BUFFER_SIZE]{};
};

}  // namespace esphome::uart_tcp
