#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#ifdef USE_NOISE_STREAM
#include "esphome/components/noise/noise_stream.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
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
class UartTcp final : public Component, public uart::UARTDevice {
 public:
  void set_host(const char *host) { this->link_.set_host(host); }
  void set_port(uint16_t port) { this->link_.set_port(port); }
  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }
  void set_timeout(uint32_t ms) { this->link_.set_idle_timeout(ms); }
#ifdef USE_NOISE_STREAM
  void set_noise_stream(noise::NoiseStream *stream) { this->noise_ = stream; }
#endif
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

 protected:
  /// The link is up and, with encryption, its session is secure.
  bool link_up_() {
#ifdef USE_NOISE_STREAM
    if (this->noise_ != nullptr) {
      return this->noise_->up(this->link_);
    }
#endif
    return this->link_.connected();
  }
  /// Send what is queued; true once the link's buffer is empty.
  bool flush_link_() {
#ifdef USE_NOISE_STREAM
    if (this->noise_ != nullptr) {
      return this->noise_->flush(this->link_);
    }
#endif
    return this->link_.flush_tx();
  }
  void sync_link_(bool up);
  void read_socket_();
  void read_uart_();
  void discard_uart_();

  static constexpr size_t READ_CHUNK = 128;
  // Scratch size for dropping stale UART bytes on connect.
  static constexpr size_t DISCARD_CHUNK = 32;

  socket::TcpClientLink link_;
#ifdef USE_SOCKET_TCP_LISTENER
  socket::TcpListener listener_;
#endif
#ifdef USE_NOISE_STREAM
  noise::NoiseStream *noise_{nullptr};
#endif
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  // Loop start time of the last socket-to-UART write; sizes the next paced write.
  uint32_t last_write_ms_{0};
#ifdef USE_SENSOR
  sensor::Sensor *disconnects_sensor_{nullptr};
  uint32_t disconnects_{0};
#endif
  bool server_{false};
  // The link state loop() saw last; edges clear the buffer and publish the sensor.
  bool link_was_up_{false};
  // A read stopped before EAGAIN. ready() stays false until new data arrives.
  bool rx_pending_{false};
};

}  // namespace esphome::uart_tcp
