#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include <cerrno>
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "udp_component.h"

namespace esphome::udp {

ESPHOME_LOG_TAG(TAG, "udp");

void UDPComponent::setup() {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  size_t address_count = 0;
  for (const char *const *it = this->addresses_; it != nullptr && *it != nullptr; it++)
    address_count++;
  this->sockaddrs_.init(address_count);
  for (const char *const *it = this->addresses_; it != nullptr && *it != nullptr; it++) {
    const char *address = *it;
    SockaddrEntry entry{};
    entry.len = socket::set_sockaddr(&entry.addr.sa, sizeof(entry.addr), address, this->broadcast_port_);
    if (entry.len == 0) {
      ESP_LOGW(TAG, "Invalid address %s", address);
      this->status_set_warning(LOG_STR("invalid address"));
      continue;
    }
    this->sockaddrs_.push_back(entry);
  }
#endif
}

bool UDPComponent::retry_if_transient_(const LogString *what) {
  const int err = errno;
  if (err != EADDRNOTAVAIL && err != ENODEV) {
    return false;
  }
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  this->listen_socket_.reset();
  this->broadcast_socket_.reset();
  this->send_socket_v6_.reset();
#endif
  this->retry_delay_ms_ = next_retry_delay_ms(this->retry_delay_ms_);
  this->next_retry_ms_ = App.get_loop_component_start_time() + this->retry_delay_ms_;
  ESP_LOGW(TAG, "%s failed (errno %d), retrying in %u ms", LOG_STR_ARG(what), err, this->retry_delay_ms_);
  this->status_set_warning(what);
  return true;
}

bool UDPComponent::open_sockets_() {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  uint32_t mcast_ifindex = 0;
  if (this->should_listen_) {
    struct sockaddr_storage server {};
    socklen_t server_len = 0;
    if (this->listen_address_ != nullptr) {
      // A multicast join needs a socket of the group's address family
      server_len =
          socket::set_sockaddr((struct sockaddr *) &server, sizeof(server), this->listen_address_, this->listen_port_);
      if (server_len == 0) {
        this->mark_failed(LOG_STR("Invalid listen address"));
        return false;
      }
      this->listen_socket_ = socket::socket_loop_monitored(server.ss_family, SOCK_DGRAM, IPPROTO_IP);
    } else {
      server_len = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->listen_port_);
      // Dual-stack on IPv6 builds so IPv4 datagrams arrive as mapped addresses
      this->listen_socket_ = socket::socket_ip_loop_monitored(SOCK_DGRAM, IPPROTO_IP);
#if USE_NETWORK_IPV6
      int disable = 0;
      if (this->listen_socket_ != nullptr &&
          this->listen_socket_->setsockopt(IPPROTO_IPV6, IPV6_V6ONLY, &disable, sizeof(disable)) < 0) {
        ESP_LOGE(TAG, "Failed to set IPV6_V6ONLY: errno %d", errno);
        this->mark_failed(LOG_STR("Failed to set IPV6_V6ONLY"));
        return false;
      }
#endif
    }
    if (this->listen_socket_ == nullptr) {
      this->mark_failed(LOG_STR("Could not create socket"));
      return false;
    }
    if (this->listen_socket_->setblocking(false) < 0) {
      ESP_LOGE(TAG, "Unable to set nonblocking: errno %d", errno);
      this->mark_failed(LOG_STR("Unable to set nonblocking"));
      return false;
    }
    int enable = 1;
    if (this->listen_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0)
      this->status_set_warning(LOG_STR("Socket unable to set reuseaddr"));

    if (this->listen_address_ != nullptr) {
      ESP_LOGD(TAG, "Join multicast %s", this->listen_address_);
      if (!socket::join_multicast_group(this->listen_socket_.get(), this->listen_address_, &mcast_ifindex)) {
        if (this->retry_if_transient_(LOG_STR("Join multicast group"))) {
          return false;
        }
        ESP_LOGE(TAG, "Failed to join multicast group. Error %d", errno);
        this->mark_failed(LOG_STR("Failed to join multicast group"));
        return false;
      }
#if defined(USE_HOST) || defined(USE_ZEPHYR)
      if (server_len == sizeof(sockaddr_in6) && mcast_ifindex != 0) {
        // POSIX bind() on link-local multicast requires sin6_scope_id; LwIP ignores it
        reinterpret_cast<sockaddr_in6 *>(&server)->sin6_scope_id = mcast_ifindex;
      }
#endif
    }

    if (this->listen_socket_->bind((struct sockaddr *) &server, server_len) != 0) {
      if (this->retry_if_transient_(LOG_STR("Bind"))) {
        return false;
      }
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      this->mark_failed(LOG_STR("Unable to bind socket"));
      return false;
    }
  }
  if (this->should_broadcast_) {
    // Broadcast needs an IPv4 socket
    this->broadcast_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (this->broadcast_socket_ == nullptr) {
      this->mark_failed(LOG_STR("Could not create socket"));
      return false;
    }
    int enable = 1;
    if (this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0)
      this->status_set_warning(LOG_STR("Socket unable to set reuseaddr"));
    if (this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) != 0)
      this->status_set_warning(LOG_STR("Socket unable to set broadcast"));
#if USE_NETWORK_IPV6
    const bool any_ipv6 = std::any_of(this->sockaddrs_.begin(), this->sockaddrs_.end(),
                                      [](const SockaddrEntry &entry) { return entry.addr.sa.sa_family == AF_INET6; });
    if (any_ipv6) {
      this->send_socket_v6_ = socket::socket(AF_INET6, SOCK_DGRAM, IPPROTO_IP);
      if (this->send_socket_v6_ == nullptr) {
        this->mark_failed(LOG_STR("Could not create IPv6 socket"));
        return false;
      }
      if (this->send_socket_v6_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0)
        this->status_set_warning(LOG_STR("IPv6 socket unable to set reuseaddr"));
      if (!socket::set_ipv6_multicast_if(this->send_socket_v6_.get(), mcast_ifindex)) {
        if (this->retry_if_transient_(LOG_STR("Set IPv6 multicast interface"))) {
          return false;
        }
        this->mark_failed(LOG_STR("Failed to set IPv6 multicast interface"));
        return false;
      }
    }
#endif
  }
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  // 8266 and RP2040 `Duino
  for (const char *const *it = this->addresses_; it != nullptr && *it != nullptr; it++) {
    const char *address = *it;
    auto ipaddr = IPAddress();
    if (!ipaddr.fromString(address)) {
      ESP_LOGW(TAG, "Invalid address %s", address);
      this->status_set_warning(LOG_STR("invalid address"));
      continue;
    }
    this->ipaddrs_.push_back(ipaddr);
  }
  if (this->should_listen_)
    this->udp_client_.begin(this->listen_port_);
#endif
  ESP_LOGD(TAG, "Sockets ready");
  return true;
}

void UDPComponent::loop() {
  if (!this->net_started_) {
    if (!network::is_connected())
      return;
    if (this->retry_delay_ms_ != 0 && (int32_t) (App.get_loop_component_start_time() - this->next_retry_ms_) < 0)
      return;
    if (!this->open_sockets_())
      return;
    if (this->retry_delay_ms_ != 0)
      this->status_clear_warning();
    this->net_started_ = true;
    if (!this->should_listen_) {
      this->disable_loop();
      return;
    }
  }
  std::array<uint8_t, MAX_PACKET_SIZE> buf;
  for (;;) {
    int len = 0;
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
    if (!this->listen_socket_->ready()) {
      break;
    }
    len = this->listen_socket_->read(buf.data(), buf.size());
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
    len = this->udp_client_.parsePacket();
    if (len > 0) {
      len = this->udp_client_.read(buf.data(), buf.size());
    }
#endif
    if (len <= 0) {
      break;
    }
    size_t packet_len = static_cast<size_t>(len);
    ESP_LOGV(TAG, "Received packet of length %zu", packet_len);
    this->packet_listeners_.call(std::span<const uint8_t>(buf.data(), packet_len));
  }
}

void UDPComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UDP:\n"
                "  Listen Port: %u\n"
                "  Broadcast Port: %u",
                this->listen_port_, this->broadcast_port_);
  for (const char *const *it = this->addresses_; it != nullptr && *it != nullptr; it++) {
    const char *address = *it;
    ESP_LOGCONFIG(TAG, "  Address: %s", address);
  }
  if (this->listen_address_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Listen address: %s", this->listen_address_);
  }
  ESP_LOGCONFIG(TAG,
                "  Broadcasting: %s\n"
                "  Listening: %s",
                YESNO(this->should_broadcast_), YESNO(this->should_listen_));
}

void UDPComponent::send_packet(const uint8_t *data, size_t size) {
  if (!this->net_started_) {
    ESP_LOGD(TAG, "Sockets not open yet, dropping packet");
    return;
  }
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  for (const auto &entry : this->sockaddrs_) {
    socket::Socket *sock = this->broadcast_socket_.get();
#if USE_NETWORK_IPV6
    if (entry.addr.sa.sa_family == AF_INET6)
      sock = this->send_socket_v6_.get();
#endif
    if (sock == nullptr) {
      ESP_LOGW(TAG, "Send socket unavailable, dropping packet");
      continue;
    }
    if (sock->sendto(data, size, 0, &entry.addr.sa, entry.len) < 0) {
      ESP_LOGW(TAG, "sendto() error %d", errno);
    }
  }
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  auto iface = IPAddress(0, 0, 0, 0);
  for (const auto &saddr : this->ipaddrs_) {
    if (this->udp_client_.beginPacketMulticast(saddr, this->broadcast_port_, iface, 128) != 0) {
      this->udp_client_.write(data, size);
      auto result = this->udp_client_.endPacket();
      if (result == 0) {
        ESP_LOGW(TAG, "udp.write() error");
      }
    }
  }
#endif
}
}  // namespace esphome::udp

#endif
