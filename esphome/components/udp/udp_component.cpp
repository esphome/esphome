#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#ifdef USE_UDP_OMR_SOURCE
#include "esphome/components/openthread/openthread.h"
#endif
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
  for (const char *const *it = this->addresses_; it != nullptr && *it != nullptr; it++) {
    const char *address = *it;
    SockaddrEntry entry{};
    if (socket::set_sockaddr(&entry.sa, sizeof(entry), address, this->broadcast_port_) == 0) {
      ESP_LOGW(TAG, "Invalid address %s", address);
      // A dropped address silently receives nothing; surface the misconfiguration
      this->status_set_warning(LOG_STR("invalid address"));
      continue;
    }
    this->sockaddrs_.push_back(entry);
  }
  // set up broadcast socket
  if (this->should_broadcast_) {
#ifdef USE_UDP_IPV6_SEND
    if (this->ipv4_send_)
#endif
    {
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
    }
#ifdef USE_UDP_IPV6_SEND
    if (this->ipv6_send_) {
      this->send_socket_v6_ = socket::socket_udp_send(AF_INET6, IPPROTO_IP);
      if (this->send_socket_v6_ == nullptr) {
        this->status_set_error(LOG_STR("Could not create IPv6 socket"));
        this->mark_failed();
        return;
      }
      // Unicast needs no egress interface; multicast does
      if (this->ipv6_multicast_)
        this->poll_pending_ |= POLL_MCAST_IF;
    }
#endif
  }
#ifdef USE_UDP_OMR_SOURCE
  // set_omr_source() asked for the bind; without a send socket there is nothing to bind
  if (this->send_socket_v6_ == nullptr)
    this->poll_pending_ &= ~POLL_SRC_BIND;
#endif
  // create listening socket if we either want to subscribe to providers, or need to listen
  // for ping key broadcasts.
  if (this->should_listen_) {
    struct sockaddr_storage server{};
    socklen_t server_len;
    if (this->listen_address_ != nullptr) {
      server_len =
          socket::set_sockaddr((struct sockaddr *) &server, sizeof(server), this->listen_address_, this->listen_port_);
      // A multicast join needs a socket of the group's address family
      this->listen_socket_ = socket::socket_udp_loop_monitored(server.ss_family, IPPROTO_IP);
      this->poll_pending_ |= POLL_JOIN;
#ifdef USE_SOCKET_IPV6
      // An IPv6 group binds after the join, which supplies the scope id link-local groups need
      if (server.ss_family == AF_INET6)
        this->poll_pending_ |= POLL_BIND;
#endif
    } else {
#ifdef USE_SOCKET_IPV6
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
      // Must stay IPv4: raw lwIP can have USE_NETWORK_IPV6 without IPv6 sockets
      server_len = socket::set_sockaddr((struct sockaddr *) &server, sizeof(server), "0.0.0.0", this->listen_port_);
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

    if ((this->poll_pending_ & POLL_BIND) == 0) {
      err = this->listen_socket_->bind((struct sockaddr *) &server, server_len);
      if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        this->status_set_error(LOG_STR("Unable to bind socket"));
        this->mark_failed();
        return;
      }
    }
  }
  if (this->poll_pending_ != 0) {
    // Joins and the IPv6 egress interface need the network up; RP2040 has no interface before then
    this->set_interval(NETWORK_POLL_ID, NETWORK_POLL_INTERVAL_MS, [this]() {
      if (network::is_connected() && this->network_poll_())
        this->cancel_interval(NETWORK_POLL_ID);
    });
  }
}

bool UDPComponent::network_poll_() {
  bool ok = true;
  if (this->poll_pending_ & POLL_JOIN) {
#ifdef USE_SOCKET_IPV6
    uint32_t *if_index = &this->mcast_ifindex_;
#else
    uint32_t *if_index = nullptr;
#endif
    if (socket::join_multicast_group(this->listen_socket_.get(), this->listen_address_, if_index)) {
      ESP_LOGD(TAG, "Joined multicast %s", this->listen_address_);
      this->poll_pending_ &= ~POLL_JOIN;
      this->poll_failed_ &= ~POLL_JOIN;
    } else {
      this->set_poll_warning_(POLL_JOIN, LOG_STR("Failed to join multicast group"), errno);
      ok = false;
    }
  }
#ifdef USE_SOCKET_IPV6
  if (ok && (this->poll_pending_ & POLL_BIND)) {
    struct sockaddr_storage server{};
    socklen_t server_len =
        socket::set_sockaddr((struct sockaddr *) &server, sizeof(server), this->listen_address_, this->listen_port_);
    // POSIX bind() on a link-local group needs it; lwIP ignores it
    reinterpret_cast<sockaddr_in6 *>(&server)->sin6_scope_id = this->mcast_ifindex_;
    if (this->listen_socket_->bind((struct sockaddr *) &server, server_len) == 0) {
      this->poll_pending_ &= ~POLL_BIND;
      this->poll_failed_ &= ~POLL_BIND;
    } else {
      this->set_poll_warning_(POLL_BIND, LOG_STR("Unable to bind socket"), errno);
      ok = false;
    }
  }
#endif
#ifdef USE_UDP_IPV6_SEND
  // Only an IPv6 group's join supplies the index, so a failing IPv4 join must not hold back sends
  const bool index_pending = (this->poll_pending_ & (POLL_JOIN | POLL_BIND)) == (POLL_JOIN | POLL_BIND);
  if ((this->poll_pending_ & POLL_MCAST_IF) && !index_pending) {
    // Index 0 picks the first eligible interface
    if (socket::set_ipv6_multicast_if(this->send_socket_v6_.get(), this->mcast_ifindex_)) {
      this->poll_pending_ &= ~POLL_MCAST_IF;
      this->poll_failed_ &= ~POLL_MCAST_IF;
    } else {
      this->set_poll_warning_(POLL_MCAST_IF, LOG_STR("Failed to set IPv6 multicast interface"), errno);
      ok = false;
    }
  }
#endif
#ifdef USE_UDP_OMR_SOURCE
  if (this->poll_pending_ & POLL_SRC_BIND) {
    // An unbound socket picks a mesh-local source that global destinations reject; bind the OMR address
    auto *ot = openthread::global_openthread_component;
    auto omr = ot != nullptr ? ot->get_omr_address() : std::nullopt;
    if (!omr.has_value()) {
      // Nothing failed yet, so there is no errno to report
      this->set_poll_warning_(POLL_SRC_BIND, LOG_STR("No routable IPv6 source address"), 0);
      ok = false;
    } else {
      struct sockaddr_in6 src{};
      src.sin6_family = AF_INET6;
      memcpy(&src.sin6_addr, omr->mFields.m8, sizeof(src.sin6_addr));
      if (this->send_socket_v6_->bind((struct sockaddr *) &src, sizeof(src)) == 0) {
        this->poll_pending_ &= ~POLL_SRC_BIND;
        this->poll_failed_ &= ~POLL_SRC_BIND;
      } else {
        this->set_poll_warning_(POLL_SRC_BIND, LOG_STR("Unable to bind the OMR source address"), errno);
        ok = false;
      }
    }
  }
#endif
  if (!ok)
    return false;
  if (this->poll_warning_) {
    this->poll_warning_ = false;
    this->status_clear_warning();
  }
  return true;
}

void UDPComponent::set_poll_warning_(PollStep step, const LogString *what, int err) {
  // Log the first failure of each step only; the poll keeps retrying every second
  if (this->poll_failed_ & step)
    return;
  this->poll_failed_ |= step;
  if (err != 0) {
    ESP_LOGW(TAG, "%s. Error %d", LOG_STR_ARG(what), err);
  } else {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(what));
  }
  // One warning bit: only a warning raised here is cleared on success, one from setup() stays
  if (!this->status_has_warning()) {
    this->status_set_warning(what);
    this->poll_warning_ = true;
  }
}

void UDPComponent::loop() {
  if (!this->should_listen_) {
    this->disable_loop();
    return;
  }
  // One byte more than the largest packet, as a read cuts a longer datagram to fit and so cannot report it
  std::array<uint8_t, MAX_PACKET_SIZE + 1> buf;
  for (;;) {
    if (!this->listen_socket_->ready())
      break;
    auto len = this->listen_socket_->read(buf.data(), buf.size());
    if (len <= 0)
      break;
    size_t packet_len = static_cast<size_t>(len);
    if (packet_len > MAX_PACKET_SIZE) {
      ESP_LOGV(TAG, "Packet longer than %zu bytes dropped", MAX_PACKET_SIZE);
      continue;
    }
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

bool UDPComponent::send_packet(const uint8_t *data, size_t size) {
  if (this->is_failed())
    return false;  // a send socket may be missing
  bool sent = true;
  for (const auto &entry : this->sockaddrs_) {
#ifdef USE_UDP_IPV6_SEND
    const bool v6 = entry.sa.sa_family == AF_INET6;
    // Until the egress interface is set, a multicast packet would leave on the wrong one
    if (v6 && (this->poll_pending_ & POLL_MCAST_IF) && entry.in6.sin6_addr.s6_addr[0] == 0xff) {
      ESP_LOGV(TAG, "Egress interface not set yet, multicast packet dropped");
      continue;
    }
#ifdef USE_UDP_OMR_SOURCE
    // Zephyr registers the socket on its first sendto() and bind() then fails with EISCONN, so even
    // mesh-scope destinations wait for the routable one
    if (v6 && (this->poll_pending_ & POLL_SRC_BIND)) {
      if ((this->poll_failed_ & POLL_SEND_HELD) == 0) {
        this->poll_failed_ |= POLL_SEND_HELD;
        ESP_LOGW(TAG, "IPv6 sends are held until a Thread border router provides an OMR address. "
                      "Put mesh-only destinations in a separate udp block to send them without waiting");
      }
      ESP_LOGV(TAG, "IPv6 source not bound yet, packet dropped");
      continue;
    }
#endif
    socket::UDPSendSocket *sock = v6 ? this->send_socket_v6_.get() : this->broadcast_socket_.get();
    socklen_t sockaddr_size = v6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
#else
    socket::UDPSendSocket *sock = this->broadcast_socket_.get();
    socklen_t sockaddr_size = sizeof(sockaddr_in);
#endif
    if (sock == nullptr) {
      ESP_LOGW(TAG, "Broadcast socket not initialized");
      return false;
    }
    auto result = sock->sendto(data, size, 0, &entry.sa, sockaddr_size);
    if (result < 0) {
      // lwIP refuses a packet with ENOMEM while the destination's ARP lookup is pending and its queue is full;
      // the packet can be sent again later
      if (errno == ENOMEM) {
        ESP_LOGV(TAG, "sendto() out of memory");
      } else {
        ESP_LOGW(TAG, "sendto() error %d", errno);
      }
      sent = false;
    }
  }
  return sent;
}
}  // namespace esphome::udp

#endif
