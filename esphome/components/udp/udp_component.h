#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/helpers.h"
#include "esphome/components/network/ip_address.h"
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
  void set_listen_address(const char *listen_addr) { this->listen_address_ = network::IPAddress(listen_addr); }
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
  bool join_multicast_();

  uint32_t join_attempt_at_{};
  uint16_t listen_port_{};
  uint16_t broadcast_port_{};
  bool should_broadcast_{};
  bool should_listen_{};
  bool join_pending_{};
  bool join_failed_{};
  CallbackManager<void(std::span<const uint8_t>)> packet_listeners_{};

  std::unique_ptr<socket::UDPSendSocket> broadcast_socket_ = nullptr;
  std::unique_ptr<socket::UDPSocket> listen_socket_ = nullptr;
  std::vector<struct sockaddr> sockaddrs_{};
  const char *const *addresses_{nullptr};

  optional<network::IPAddress> listen_address_{};
};

}  // namespace esphome::udp
#endif
