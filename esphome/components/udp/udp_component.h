#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/helpers.h"
#include "esphome/components/socket/socket.h"

// The raw lwIP implementation (ESP8266, RP2040) has no IPv6 membership support. Its IPv6
// unicast should work but is untested, so it stays off until someone can test it there.
#if USE_NETWORK_IPV6 && (defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS))
#define USE_UDP_IPV6
#endif
#include <array>
#include <initializer_list>
#include <span>
#include <vector>

namespace esphome::udp {

static const size_t MAX_PACKET_SIZE = 508;
class UDPComponent final : public Component {
 public:
  /// nullptr-terminated table of addresses; must outlive the component.
  void set_addresses(const char *const *addresses) { this->addresses_ = addresses; }
  /// Multicast group literal; must outlive the component.
  void set_listen_address(const char *listen_addr) { this->listen_address_ = listen_addr; }
  void set_listen_port(uint16_t port) { this->listen_port_ = port; }
  void set_broadcast_port(uint16_t port) { this->broadcast_port_ = port; }
  void set_should_broadcast() { this->should_broadcast_ = true; }
  void set_should_listen() { this->should_listen_ = true; }
  template<typename F> void add_listener(F &&listener) { this->packet_listeners_.add(std::forward<F>(listener)); }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void send_packet(const uint8_t *data, size_t size);
  void send_packet(const std::vector<uint8_t> &buf) { this->send_packet(buf.data(), buf.size()); }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; };

 protected:
  struct SockaddrEntry {
    union {
      sockaddr sa;
      sockaddr_in in4;
#ifdef USE_UDP_IPV6
      sockaddr_in6 in6;
#endif
    } addr{};
    socklen_t len{0};
  };

  /// One pass of the network poll; true once nothing is left to do.
  bool network_poll_();
  /// Joins the listen group and, for IPv6, binds after the join; true once both are done.
  bool join_multicast_();
  void set_poll_warning_(const LogString *what);

  CallbackManager<void(std::span<const uint8_t>)> packet_listeners_{};
  std::unique_ptr<socket::UDPSendSocket> broadcast_socket_ = nullptr;
  std::unique_ptr<socket::UDPSocket> listen_socket_ = nullptr;
#ifdef USE_UDP_IPV6
  std::unique_ptr<socket::UDPSendSocket> send_socket_v6_ = nullptr;
#endif
  FixedVector<SockaddrEntry> sockaddrs_{};
  const char *const *addresses_{nullptr};
  const char *listen_address_{nullptr};
#ifdef USE_UDP_IPV6
  uint32_t mcast_ifindex_{0};
#endif
  uint16_t listen_port_{};
  uint16_t broadcast_port_{};
  bool should_broadcast_{};
  bool should_listen_{};
  bool joined_{};
  bool poll_failed_{};
  bool poll_warning_{};
#ifdef USE_UDP_IPV6
  bool v6_bind_pending_{};
  bool needs_v6_multicast_if_{};
#endif
};

}  // namespace esphome::udp
#endif
