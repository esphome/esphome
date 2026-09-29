#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
#include "lwip/ip_addr.h"
#endif

#include <atomic>
#include <cstdint>
#include <memory>

namespace esphome::tcp_uart {

/// TCP client presented as a UART. Bytes are copied unchanged.
class TcpUart : public uart::UARTComponent, public Component {
 public:
  void set_host(const char *host) { this->host_ = StringRef(host); }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_reconnect_interval(uint32_t ms) { this->reconnect_interval_ms_ = ms; }
  void set_connected_sensor(binary_sensor::BinarySensor *sensor) { this->connected_sensor_ = sensor; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override;

  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override;
  uart::UARTFlushResult flush() override;
  bool is_connected() override { return this->sock_ != nullptr && this->connected_; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

 protected:
  void check_logger_conflict() override {}
  void close_sock_();
  void try_resolve_();
  bool ip_ready_();
  void try_connect_();
  void read_socket_();
  void flush_tx_();
  void apply_socket_options_(socket::Socket *sock);
  void set_link_up_(bool up);
  void note_attempt_();
  void forget_addr_();
  bool in_backoff_() const;
#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
  static void dns_found(const char *name, const ip_addr_t *addr, void *arg);
#endif

  static constexpr size_t RX_BUFFER_SIZE = 1024;
  static constexpr size_t TX_BUFFER_SIZE = 1024;

  // 4-byte members, then the port, then the flags, then the byte buffers.
  StringRef host_;
  std::unique_ptr<socket::Socket> sock_;
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
  uint32_t last_attempt_ms_{0};
  uint32_t last_drop_log_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  size_t tx_len_{0};
  std::atomic<uint32_t> resolved_addr_{0};

  uint16_t port_{0};
  bool connecting_{false};
  bool connected_{false};
  bool offline_drop_logged_{false};
  // atomic<bool> is an out-of-line call on the ESP8266 and does not link.
  std::atomic<uint8_t> resolving_{0};
  std::atomic<uint8_t> resolve_failed_{0};
  std::atomic<uint8_t> have_addr_{0};
  char resolved_ip_[socket::SOCKADDR_STR_LEN]{};
  StaticRingBuffer<uint8_t, RX_BUFFER_SIZE> rx_;
  uint8_t tx_[TX_BUFFER_SIZE]{};
};

}  // namespace esphome::tcp_uart
