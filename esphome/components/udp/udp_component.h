#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/helpers.h"
#include "esphome/components/socket/socket.h"
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
#ifdef USE_UDP_IPV6_SEND
  /// This instance has IPv6 destinations; multicast ones also need an egress interface.
  void set_ipv6_send(bool multicast) {
    this->ipv6_send_ = true;
    this->ipv6_multicast_ = multicast;
  }
  /// An instance whose destinations are all IPv6 needs no IPv4 send socket
  void set_ipv4_send(bool ipv4_send) { this->ipv4_send_ = ipv4_send; }
#endif
  template<typename F> void add_listener(F &&listener) { this->packet_listeners_.add(std::forward<F>(listener)); }
  /// The sender of the packet being passed to the listeners; valid only during a listener call.
  const sockaddr *get_rx_source() const { return this->rx_source_; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  /// Returns false if the packet could not be sent to every destination.
  bool send_packet(const uint8_t *data, size_t size);
  bool send_packet(const std::vector<uint8_t> &buf) { return this->send_packet(buf.data(), buf.size()); }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; };

 protected:
  enum PollStep : uint8_t { POLL_JOIN = 1, POLL_BIND = 2, POLL_MCAST_IF = 4 };

  union SockaddrEntry {
    sockaddr sa;
#ifdef USE_UDP_IPV6_SEND
    sockaddr_in6 in6;  // sizes the storage for IPv6 destinations
#endif
  };

  /// One pass of the network poll; true once nothing is left to do.
  bool network_poll_();
  void set_poll_warning_(PollStep step, const LogString *what);

  LazyCallbackManager<void(std::span<const uint8_t>)> packet_listeners_{};
  std::unique_ptr<socket::UDPSendSocket> broadcast_socket_ = nullptr;
  std::unique_ptr<socket::UDPSocket> listen_socket_ = nullptr;
#ifdef USE_UDP_IPV6_SEND
  std::unique_ptr<socket::UDPSendSocket> send_socket_v6_ = nullptr;
#endif
  FixedVector<SockaddrEntry> sockaddrs_{};
  const char *const *addresses_{nullptr};
  const char *listen_address_{nullptr};
  const sockaddr *rx_source_{nullptr};
#ifdef USE_SOCKET_IPV6
  uint32_t mcast_ifindex_{0};
#endif
  uint16_t listen_port_{};
  uint16_t broadcast_port_{};
  bool should_broadcast_{};
  bool should_listen_{};
  uint8_t poll_pending_{};  // PollStep bits
  uint8_t poll_failed_{};   // PollStep bits already logged
  bool poll_warning_{};
#ifdef USE_UDP_IPV6_SEND
  bool ipv4_send_{true};
  bool ipv6_send_{};
  bool ipv6_multicast_{};
#endif
};

}  // namespace esphome::udp
#endif
