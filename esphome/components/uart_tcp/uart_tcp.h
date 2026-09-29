#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

#ifndef USE_HOST
#include "lwip/ip_addr.h"
#endif

#include <atomic>
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
  float get_setup_priority() const override;

 protected:
  void close_sock_();
  void close_listen_();
  void try_resolve_();
  bool ip_ready_();
  void try_connect_();
  void try_listen_();
  void accept_client_();
  void read_socket_();
  void read_uart_();
  void send_all_(const uint8_t *data, size_t len);
  void apply_socket_options_(socket::Socket *sock);
  void set_link_up_(bool up);
  void note_attempt_();
  bool in_backoff_() const;
#ifndef USE_HOST
  static void dns_found(const char *name, const ip_addr_t *addr, void *arg);
#endif

  StringRef host_;
  uint16_t port_{0};
  bool server_{false};
  std::unique_ptr<socket::Socket> sock_;
  std::unique_ptr<socket::ListenSocket> listen_;
  bool connecting_{false};
  bool connected_{false};
  uint32_t last_attempt_ms_{0};
  uint32_t reconnect_interval_ms_{5000};
  binary_sensor::BinarySensor *connected_sensor_{nullptr};

  std::atomic<bool> resolving_{false};
  std::atomic<bool> resolve_failed_{false};
  std::atomic<bool> have_addr_{false};
  std::atomic<uint32_t> resolved_addr_{0};
  char resolved_ip_[socket::SOCKADDR_STR_LEN]{};
};

}  // namespace esphome::uart_tcp
