#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#include "esphome/components/socket/socket.h"
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
#include <WiFiUdp.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

namespace esphome::udp {

static const size_t MAX_PACKET_SIZE = 508;
static constexpr uint16_t RETRY_INITIAL_MS = 1000;
static constexpr uint16_t RETRY_MAX_MS = 60000;

/// Wait before the next socket retry: RETRY_INITIAL_MS first, then doubled up to RETRY_MAX_MS.
inline uint16_t next_retry_delay_ms(uint16_t current_ms) {
  return current_ms == 0 ? RETRY_INITIAL_MS : static_cast<uint16_t>(std::min<uint32_t>(current_ms * 2u, RETRY_MAX_MS));
}

class UDPComponent final : public Component {
 public:
  /// nullptr-terminated table of addresses; must outlive the component.
  void set_addresses(const char *const *addresses) { this->addresses_ = addresses; }
  /// Multicast group to join; a string literal that must outlive the component.
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

 protected:
  /// Opens the sockets; false means setup failed or will be retried later.
  bool open_sockets_();
  /// If errno is a temporary error, closes the sockets and arms a retry with back-off; true when it did.
  bool retry_if_transient_(const LogString *what);

  CallbackManager<void(std::span<const uint8_t>)> packet_listeners_{};

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  struct SockaddrEntry {
    union {
      sockaddr sa;
      sockaddr_in in4;
#if USE_NETWORK_IPV6
      sockaddr_in6 in6;
#endif
    } addr{};
    socklen_t len{0};
  };
  std::unique_ptr<socket::Socket> broadcast_socket_ = nullptr;
  std::unique_ptr<socket::Socket> listen_socket_ = nullptr;
#if USE_NETWORK_IPV6
  std::unique_ptr<socket::Socket> send_socket_v6_ = nullptr;
#endif
  FixedVector<SockaddrEntry> sockaddrs_{};
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  std::vector<IPAddress> ipaddrs_{};
  WiFiUDP udp_client_{};
#endif
  const char *const *addresses_{nullptr};
  const char *listen_address_{nullptr};
  uint32_t next_retry_ms_{0};
  uint16_t listen_port_{};
  uint16_t broadcast_port_{};
  uint16_t retry_delay_ms_{0};
  bool should_broadcast_{};
  bool should_listen_{};
  bool net_started_{false};
};

}  // namespace esphome::udp
#endif
