#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/socket/tcp_client_link.h"
#ifdef USE_SOCKET_TCP_LISTENER
#include "esphome/components/socket/tcp_listener.h"
#endif
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
  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }
  void set_timeout(uint32_t ms) { this->timeout_ms_ = ms; }
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
  void sync_link_();
  void read_socket_();
  void read_uart_();
  void discard_uart_();
  void close_idle_();
  // 0 disables. Unsigned elapsed time, so a wrapped millis() does not close early.
  // A zero loop clock means it has not started; last_io_ 0 means there is no link yet.
  void note_io_() {
    uint32_t now = App.get_loop_component_start_time();
    this->last_io_ms_ = now == 0 ? 1 : now;
  }
  void check_idle_() {
    if (this->timeout_ms_ == 0 || this->last_io_ms_ == 0) {
      return;
    }
    uint32_t now = App.get_loop_component_start_time();
    if (now == 0 || now - this->last_io_ms_ < this->timeout_ms_) {
      return;
    }
    this->close_idle_();
  }

  static constexpr size_t READ_CHUNK = 128;

  socket::TcpClientLink link_;
#ifdef USE_SOCKET_TCP_LISTENER
  socket::TcpListener listener_;
#endif
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  uint32_t last_io_ms_{0};
  uint32_t timeout_ms_{0};
  bool server_{false};
  // The link state loop() saw last; edges clear the buffer and publish the sensor.
  bool link_was_up_{false};
  // A read stopped before EAGAIN. ready() stays false until new data arrives.
  bool rx_pending_{false};
};

}  // namespace esphome::uart_tcp
