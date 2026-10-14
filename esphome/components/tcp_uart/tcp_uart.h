#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/socket/tcp_client_link.h"
#ifdef USE_SOCKET_TCP_LISTENER
#include "esphome/components/socket/tcp_listener.h"
#endif
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::tcp_uart {

/// TCP client or server presented as a UART. Bytes are copied unchanged.
class TcpUart : public uart::UARTComponent, public Component {
 public:
  TcpUart() { this->rx_buffer_size_ = RX_BUFFER_SIZE; }

  void set_host(const char *host) { this->link_.set_host(host); }
  void set_port(uint16_t port) { this->link_.set_port(port); }
  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }
#ifdef USE_SENSOR
  void set_disconnects_sensor(sensor::Sensor *sensor) { this->disconnects_sensor_ = sensor; }
#endif
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

  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override { return static_cast<size_t>(this->rx_end_ - this->rx_start_); }
  // Same room write_array() grants, so consumers can apply backpressure.
  size_t available_for_write() override { return this->link_.tx_free(); }
  uart::UARTFlushResult flush() override;
  bool is_connected() override { return this->link_.connected(); }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
  using UARTComponent::load_settings;  // also bring in the no-arg overload for convenience
#endif

 protected:
  void check_logger_conflict() override {}
  void sync_link_();
  void read_socket_();

  static constexpr size_t RX_BUFFER_SIZE = 1024;

  socket::TcpClientLink link_;
#ifdef USE_SOCKET_TCP_LISTENER
  socket::TcpListener listener_;
#endif
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
#ifdef USE_SENSOR
  sensor::Sensor *disconnects_sensor_{nullptr};
  uint32_t disconnects_{0};
#endif
  uint32_t last_drop_log_ms_{0};
  // rx_[rx_start_, rx_end_) holds unread bytes; read_socket_() compacts to the front.
  uint16_t rx_start_{0};
  uint16_t rx_end_{0};
  bool server_{false};
  // The link state loop() saw last; edges publish the sensor, the up edge clears rx_.
  bool link_was_up_{false};
  // A read stopped before EAGAIN. ready() stays false until new data arrives.
  bool rx_pending_{false};
  uint8_t rx_[RX_BUFFER_SIZE]{};
};

}  // namespace esphome::tcp_uart
