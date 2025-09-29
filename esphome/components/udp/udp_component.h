#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/components/network/ip_address.h"
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#include "esphome/components/socket/socket.h"
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
#include <WiFiUdp.h>
#endif
#include <vector>

namespace esphome {
namespace udp {

static const size_t MAX_PACKET_SIZE = 508;
class UDPComponent : public Component {
 public:
  void add_address(const char *addr) { this->addresses_.emplace_back(addr); }
  void set_listen_address(const char *listen_addr) { this->listen_address_ = network::IPAddress(listen_addr); }
  void set_listen_port(uint16_t port) { this->listen_port_ = port; }
  void set_broadcast_port(uint16_t port) { this->broadcast_port_ = port; }
  void set_should_broadcast() { this->should_broadcast_ = true; }
  void set_should_listen() { this->should_listen_ = true; }
  void add_listener(std::function<void(std::vector<uint8_t> &)> &&listener) {
    this->packet_listeners_.add(std::move(listener));
  }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void send_packet(const uint8_t *data, size_t size);
  void send_packet(const std::vector<uint8_t> &buf) { this->send_packet(buf.data(), buf.size()); }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; };

 protected:
  uint16_t listen_port_{};
  uint16_t broadcast_port_{};
  bool should_broadcast_{};
  bool should_listen_{};
  CallbackManager<void(std::vector<uint8_t> &)> packet_listeners_{};

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  std::unique_ptr<socket::Socket> broadcast_socket_ = nullptr;
  std::unique_ptr<socket::Socket> listen_socket_ = nullptr;
  std::vector<struct sockaddr> sockaddrs_{};
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  std::vector<IPAddress> ipaddrs_{};
  WiFiUDP udp_client_{};
#endif
  std::vector<std::string> addresses_{};

  optional<network::IPAddress> listen_address_{};
};

}  // namespace udp
}  // namespace esphome
#endif
