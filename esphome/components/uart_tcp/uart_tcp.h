#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/socket/tcp_client_link.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include <cstdint>
#include <memory>

namespace esphome::uart_tcp {

/// Copies raw bytes between one hardware UART and one TCP socket.
class UartTcp : public Component, public uart::UARTDevice {
 public:
  void set_host(const char *host) { this->link_.set_host(host); }
  void set_port(uint16_t port) { this->link_.set_port(port); }
  void set_server(bool server) { this->server_ = server; }
  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void sync_link_();
  void try_listen_();
  void accept_client_();
  void read_socket_();
  void read_uart_();
  void flush_tx_();

  static constexpr size_t TX_BUFFER_SIZE = 1024;
  static constexpr size_t READ_CHUNK = 128;

  socket::TcpClientLink link_;
  std::unique_ptr<socket::ListenSocket> listen_;
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  uint16_t tx_len_{0};
  bool server_{false};
  // The link state loop() saw last; edges clear the buffer and publish the sensor.
  bool link_was_up_{false};
  // A read stopped before EAGAIN. ready() stays false until new data arrives.
  bool rx_pending_{false};
  uint8_t tx_[TX_BUFFER_SIZE]{};
};

}  // namespace esphome::uart_tcp
