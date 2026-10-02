#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/socket/tcp_client_link.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"

#include <cstdint>

namespace esphome::tcp_uart {

/// TCP client presented as a UART. Bytes are copied unchanged.
class TcpUart : public uart::UARTComponent, public Component {
 public:
  TcpUart(const char *host, uint16_t port) {
    this->link_.set_host(host);
    this->link_.set_port(port);
    this->rx_buffer_size_ = RX_BUFFER_SIZE;
  }

  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override { this->link_.close(); }
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
#endif

 protected:
  void check_logger_conflict() override {}
  void sync_link_();
  void read_socket_();

  static constexpr size_t RX_BUFFER_SIZE = 1024;

  socket::TcpClientLink link_;
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  uint32_t last_drop_log_ms_{0};
  // rx_[rx_start_, rx_end_) holds unread bytes; read_socket_() compacts to the front.
  uint16_t rx_start_{0};
  uint16_t rx_end_{0};
  // The link state loop() saw last; edges clear the buffers and publish the sensor.
  bool link_was_up_{false};
  // A read stopped before EAGAIN. ready() stays false until new data arrives.
  bool rx_pending_{false};
  uint8_t rx_[RX_BUFFER_SIZE]{};
};

}  // namespace esphome::tcp_uart
