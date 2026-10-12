#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "udp_component.h"

namespace esphome::udp {

ESPHOME_LOG_TAG(TAG, "udp");
static constexpr uint32_t NETWORK_POLL_ID = 0;
static constexpr uint32_t NETWORK_POLL_INTERVAL_MS = 1000;

void UDPComponent::setup() {
  size_t address_count = 0;
  for (const char *const *it = this->addresses_; it != nullptr && *it != nullptr; it++)
    address_count++;
  this->sockaddrs_.init(address_count);
#ifdef USE_UDP_IPV6
  bool any_v6 = false;
  bool any_v6_multicast = false;
#endif
  for (const char *const *it = this->addresses_; it != nullptr && *it != nullptr; it++) {
    const char *address = *it;
    SockaddrEntry entry{};
    entry.len = socket::set_sockaddr(&entry.addr.sa, sizeof(entry.addr), address, this->broadcast_port_);
    if (entry.len == 0) {
      ESP_LOGW(TAG, "Invalid address %s", address);
      // A dropped address silently receives nothing; surface the misconfiguration
      this->status_set_warning(LOG_STR("invalid address"));
      continue;
    }
#ifdef USE_UDP_IPV6
    if (entry.addr.sa.sa_family == AF_INET6) {
      any_v6 = true;
      any_v6_multicast = any_v6_multicast || entry.addr.in6.sin6_addr.s6_addr[0] == 0xff;
    }
#endif
    this->sockaddrs_.push_back(entry);
  }
  // set up broadcast socket
  if (this->should_broadcast_) {
    this->broadcast_socket_ = socket::socket_udp_send(AF_INET, IPPROTO_IP);
    if (this->broadcast_socket_ == nullptr) {
      this->status_set_error(LOG_STR("Could not create socket"));
      this->mark_failed();
      return;
    }
    int enable = 1;
    auto err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning(LOG_STR("Socket unable to set reuseaddr"));
      // we can still continue
    }
    err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning(LOG_STR("Socket unable to set broadcast"));
    }
#ifdef USE_UDP_IPV6
    if (any_v6) {
      this->send_socket_v6_ = socket::socket_udp_send(AF_INET6, IPPROTO_IP);
      if (this->send_socket_v6_ == nullptr) {
        this->status_set_error(LOG_STR("Could not create IPv6 socket"));
        this->mark_failed();
        return;
      }
      // Unicast needs no egress interface; multicast does
      this->needs_v6_multicast_if_ = any_v6_multicast;
    }
#endif
  }
  // create listening socket if we either want to subscribe to providers, or need to listen
  // for ping key broadcasts.
  if (this->should_listen_) {
    struct sockaddr_storage server {};
    socklen_t server_len;
    if (this->listen_address_ != nullptr) {
      server_len =
          socket::set_sockaddr((struct sockaddr *) &server, sizeof(server), this->listen_address_, this->listen_port_);
      // A multicast join needs a socket of the group's address family
      this->listen_socket_ = socket::socket_udp_loop_monitored(server.ss_family, IPPROTO_IP);
#ifdef USE_UDP_IPV6
      this->v6_bind_pending_ = server.ss_family == AF_INET6;
#endif
    } else {
#ifdef USE_UDP_IPV6
      server_len = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->listen_port_);
      this->listen_socket_ = socket::socket_udp_loop_monitored(AF_INET6, IPPROTO_IP);
      // Clear IPV6_V6ONLY so IPv4 datagrams also arrive, as mapped addresses
      int v6only = 0;
      if (this->listen_socket_ != nullptr &&
          this->listen_socket_->setsockopt(IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) != 0) {
        ESP_LOGW(TAG, "Failed to clear IPV6_V6ONLY: errno %d", errno);
        this->status_set_warning(LOG_STR("IPv4 datagrams may not be received"));
      }
#else
      auto *server4 = reinterpret_cast<struct sockaddr_in *>(&server);
      server4->sin_family = AF_INET;
      server4->sin_addr.s_addr = ESPHOME_INADDR_ANY;
      server4->sin_port = htons(this->listen_port_);
      server_len = sizeof(struct sockaddr_in);
      this->listen_socket_ = socket::socket_udp_loop_monitored(AF_INET, IPPROTO_IP);
#endif
    }
    if (this->listen_socket_ == nullptr) {
      this->status_set_error(LOG_STR("Could not create socket"));
      this->mark_failed();
      return;
    }
    auto err = this->listen_socket_->setblocking(false);
    if (err < 0) {
      ESP_LOGE(TAG, "Unable to set nonblocking: errno %d", errno);
      this->status_set_error(LOG_STR("Unable to set nonblocking"));
      this->mark_failed();
      return;
    }
    int enable = 1;
    err = this->listen_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (err != 0) {
      this->status_set_warning(LOG_STR("Socket unable to set reuseaddr"));
      // we can still continue
    }

#ifdef USE_UDP_IPV6
    // An IPv6 group binds after the join, which supplies the scope id link-local groups need
    if (!this->v6_bind_pending_)
#endif
    {
      err = this->listen_socket_->bind((struct sockaddr *) &server, server_len);
      if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        this->status_set_error(LOG_STR("Unable to bind socket"));
        this->mark_failed();
        return;
      }
    }
  }
  bool poll_needed = this->should_listen_ && this->listen_address_ != nullptr;
#ifdef USE_UDP_IPV6
  poll_needed = poll_needed || this->needs_v6_multicast_if_;
#endif
  if (poll_needed) {
    // Joins and the IPv6 egress interface need the network up; RP2040 has no interface before then
    this->set_interval(NETWORK_POLL_ID, NETWORK_POLL_INTERVAL_MS, [this]() {
      if (network::is_connected() && this->network_poll_())
        this->cancel_interval(NETWORK_POLL_ID);
    });
  }
}

bool UDPComponent::network_poll_() {
  if (this->should_listen_ && this->listen_address_ != nullptr && !this->join_multicast_())
    return false;
#ifdef USE_UDP_IPV6
  if (this->needs_v6_multicast_if_) {
    // Index 0 picks the first eligible interface
    if (!socket::set_ipv6_multicast_if(this->send_socket_v6_.get(), this->mcast_ifindex_)) {
      this->set_poll_warning_(LOG_STR("Failed to set IPv6 multicast interface"));
      return false;
    }
    this->needs_v6_multicast_if_ = false;
    this->poll_failed_ = false;
  }
#endif
  if (this->poll_warning_) {
    this->poll_warning_ = false;
    this->status_clear_warning();
  }
  return true;
}

bool UDPComponent::join_multicast_() {
  if (!this->joined_) {
#ifdef USE_UDP_IPV6
    uint32_t *if_index = &this->mcast_ifindex_;
#else
    uint32_t *if_index = nullptr;
#endif
    if (!socket::join_multicast_group(this->listen_socket_.get(), this->listen_address_, if_index)) {
      this->set_poll_warning_(LOG_STR("Failed to join multicast group"));
      return false;
    }
    ESP_LOGD(TAG, "Joined multicast %s", this->listen_address_);
    this->joined_ = true;
    this->poll_failed_ = false;
  }
#ifdef USE_UDP_IPV6
  if (this->v6_bind_pending_) {
    struct sockaddr_storage server {};
    socklen_t server_len =
        socket::set_sockaddr((struct sockaddr *) &server, sizeof(server), this->listen_address_, this->listen_port_);
#if defined(USE_HOST) || defined(USE_ZEPHYR)
    // POSIX bind() on a link-local group requires sin6_scope_id; lwIP ignores it
    reinterpret_cast<sockaddr_in6 *>(&server)->sin6_scope_id = this->mcast_ifindex_;
#endif
    if (this->listen_socket_->bind((struct sockaddr *) &server, server_len) != 0) {
      this->set_poll_warning_(LOG_STR("Unable to bind socket"));
      return false;
    }
    this->v6_bind_pending_ = false;
    this->poll_failed_ = false;
  }
#endif
  return true;
}

void UDPComponent::set_poll_warning_(const LogString *what) {
  // Log the first failure of each step only; the poll keeps retrying every second
  if (this->poll_failed_)
    return;
  this->poll_failed_ = true;
  ESP_LOGW(TAG, "%s. Error %d", LOG_STR_ARG(what), errno);
  // One warning bit: only a warning raised here is cleared on success, one from setup() stays
  if (!this->status_has_warning()) {
    this->status_set_warning(what);
    this->poll_warning_ = true;
  }
}

void UDPComponent::loop() {
  if (this->should_listen_) {
    std::array<uint8_t, MAX_PACKET_SIZE> buf;
    for (;;) {
      if (!this->listen_socket_->ready())
        break;
      auto len = this->listen_socket_->read(buf.data(), buf.size());
      if (len <= 0)
        break;
      size_t packet_len = static_cast<size_t>(len);
      ESP_LOGV(TAG, "Received packet of length %zu", packet_len);
      this->packet_listeners_.call(std::span<const uint8_t>(buf.data(), packet_len));
    }
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
  for (const auto &entry : this->sockaddrs_) {
    socket::UDPSendSocket *sock = this->broadcast_socket_.get();
#ifdef USE_UDP_IPV6
    if (entry.addr.sa.sa_family == AF_INET6)
      sock = this->send_socket_v6_.get();
#endif
    auto result = sock->sendto(data, size, 0, &entry.addr.sa, entry.len);
    if (result < 0) {
      ESP_LOGW(TAG, "sendto() error %d", errno);
    }
  }
}
}  // namespace esphome::udp

#endif
