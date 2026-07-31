#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include <cerrno>
#include <cstring>
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#ifdef USE_OPENTHREAD
#include "esphome/components/openthread/openthread.h"
#endif
#include "udp_component.h"

namespace esphome::udp {

static const char *const TAG = "udp";

void UDPComponent::setup() {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  // Build destination address list.  sockaddr_storage is large enough for both IPv4 and IPv6.
  for (const auto &address : this->addresses_) {
    struct sockaddr_storage saddr {};
    socklen_t addrlen =
        socket::set_sockaddr(reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr), address, this->broadcast_port_);
    if (addrlen == 0) {
      ESP_LOGE(TAG, "Invalid address '%s': errno %d", address, errno);
      this->status_set_error(LOG_STR("Invalid address"));
      this->mark_failed();
      return;
    }
    this->sockaddrs_.push_back(saddr);
  }
  // Broadcast (send) socket
  if (this->should_broadcast_) {
    this->broadcast_socket_ = socket::socket_ip(SOCK_DGRAM, IPPROTO_IP);
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
#ifndef USE_ZEPHYR
    // IPv6 has no broadcast concept — SO_BROADCAST is IPv4-only
    err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning(LOG_STR("Socket unable to set broadcast"));
    }
#endif
  }
  // Listen socket
  if (this->should_listen_) {
    this->listen_socket_ = socket::socket_ip(SOCK_DGRAM, IPPROTO_IP);
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
    struct sockaddr_storage server {};
    socklen_t server_len =
        socket::set_sockaddr_any(reinterpret_cast<sockaddr *>(&server), sizeof(server), this->listen_port_);
    if (server_len == 0) {
      ESP_LOGE(TAG, "Unable to set sockaddr: errno %d", errno);
      this->status_set_error(LOG_STR("Unable to set sockaddr"));
      this->mark_failed();
      return;
    }
#ifndef USE_ZEPHYR
    // IPv4 multicast group join (not yet implemented on Zephyr)
    if (this->listen_address_.has_value()) {
      char addr_buf[network::IP_ADDRESS_BUFFER_SIZE];
      this->listen_address_.value().str_to(addr_buf);
      struct sockaddr_in group {};
      struct ip_mreq imreq = {};
      imreq.imr_interface.s_addr = ESPHOME_INADDR_ANY;
      inet_aton(addr_buf, &imreq.imr_multiaddr);
      group.sin_family = AF_INET;
      group.sin_addr.s_addr = imreq.imr_multiaddr.s_addr;
      group.sin_port = htons(this->listen_port_);
      err = this->listen_socket_->bind((struct sockaddr *) &group, sizeof(group));
      if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind to multicast group: errno %d", errno);
        this->status_set_error(LOG_STR("Unable to bind socket"));
        this->mark_failed();
        return;
      }
      ESP_LOGD(TAG, "Join multicast %s", addr_buf);
      err = this->listen_socket_->setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(imreq));
      if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        this->status_set_error(LOG_STR("Failed to set IP_ADD_MEMBERSHIP"));
        this->mark_failed();
        return;
      }
    } else {
      err = this->listen_socket_->bind(reinterpret_cast<sockaddr *>(&server), server_len);
      if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        this->status_set_error(LOG_STR("Unable to bind socket"));
        this->mark_failed();
        return;
      }
    }
#else
    err = this->listen_socket_->bind(reinterpret_cast<sockaddr *>(&server), server_len);
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      this->status_set_error(LOG_STR("Unable to bind socket"));
      this->mark_failed();
      return;
    }
#endif
  }
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  // 8266 and RP2040 `Duino
  for (const auto &address : this->addresses_) {
    auto ipaddr = IPAddress();
    ipaddr.fromString(address);
    this->ipaddrs_.push_back(ipaddr);
  }
  if (this->should_listen_)
    this->udp_client_.begin(this->listen_port_);
#endif
}

void UDPComponent::loop() {
  if (this->should_listen_) {
    std::array<uint8_t, MAX_PACKET_SIZE> buf;
    for (;;) {
      ssize_t len = 0;
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
      len = this->listen_socket_->read(buf.data(), buf.size());
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
      len = this->udp_client_.parsePacket();
      if (len > 0)
        len = this->udp_client_.read(buf.data(), buf.size());
#endif
      if (len <= 0)
        break;
      size_t packet_len = static_cast<size_t>(len);
#ifdef USE_ZEPHYR
      // nano libc on Zephyr toolchains does not support %zu
      ESP_LOGV(TAG, "Received packet of length %u", (unsigned) packet_len);
#else
      ESP_LOGV(TAG, "Received packet of length %zu", packet_len);
#endif
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
  for (const char *address : this->addresses_)
    ESP_LOGCONFIG(TAG, "  Address: %s", address);
  if (this->listen_address_.has_value()) {
    char addr_buf[network::IP_ADDRESS_BUFFER_SIZE];
    ESP_LOGCONFIG(TAG, "  Listen address: %s", this->listen_address_.value().str_to(addr_buf));
  }
  ESP_LOGCONFIG(TAG,
                "  Broadcasting: %s\n"
                "  Listening: %s",
                YESNO(this->should_broadcast_), YESNO(this->should_listen_));
}

void UDPComponent::send_packet(const uint8_t *data, size_t size) {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#ifdef USE_ZEPHYR
  // Zephyr is IPv6-only here, so every stored address is a sockaddr_in6 (pass its real length,
  // not sizeof(sockaddr_storage), which is 128 bytes).
  //
  // On Thread the device holds only ULA addresses. A cold, device-initiated sendto() on an
  // unbound socket fails source-address selection for a globally-scoped destination with ENOENT.
  // Bind the send socket once to the OMR (off-mesh-routable) address so the stack has a concrete
  // routable source -- this is what an inbound packet implicitly provides for the reply/OTA paths.
#ifdef USE_OPENTHREAD
  // The broadcast socket may have been closed by a previous ENOENT recovery; rebuild it
  // before selecting send_socket so the recreate is reachable.
  if (this->should_broadcast_ && this->broadcast_socket_ == nullptr) {
    this->broadcast_socket_ = socket::socket_ip(SOCK_DGRAM, IPPROTO_IP);
    if (this->broadcast_socket_ == nullptr) {
      ESP_LOGW(TAG, "Could not recreate send socket");
      return;
    }
    int enable = 1;
    auto err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (err != 0) {
      ESP_LOGW(TAG, "Socket unable to set reuseaddr on recreated socket");
    }
  }
#endif
  socket::Socket *send_socket = this->broadcast_socket_ ? this->broadcast_socket_.get() : this->listen_socket_.get();
  if (send_socket == nullptr)
    return;
#ifdef USE_OPENTHREAD
  if (send_socket == this->broadcast_socket_.get() && !this->broadcast_socket_bound_) {
    if (openthread::global_openthread_component == nullptr)
      return;
    auto omr = openthread::global_openthread_component->get_omr_address();
    if (!omr.has_value()) {
      ESP_LOGV(TAG, "OMR address not available yet; dropping packet");
      return;
    }
    struct sockaddr_in6 src {};
    src.sin6_family = AF_INET6;
    memcpy(&src.sin6_addr, omr->mFields.m8, sizeof(src.sin6_addr));
    if (this->broadcast_socket_->bind(reinterpret_cast<sockaddr *>(&src), sizeof(src)) != 0) {
      ESP_LOGW(TAG, "Bind send socket to OMR source failed: errno %d", errno);
      return;
    }
    this->broadcast_socket_bound_ = true;
  }
#endif
  for (const auto &saddr : this->sockaddrs_) {
    auto result =
        send_socket->sendto(data, size, 0, reinterpret_cast<const sockaddr *>(&saddr), sizeof(struct sockaddr_in6));
    if (result < 0) {
      ESP_LOGW(TAG, "sendto() error %d", errno);
      // A bound socket cannot be re-bound: just resetting the flag would make the
      // next send try bind() again, which fails with EINVAL. Close the socket so
      // the next send rebuilds it with a fresh OMR bind.
      if (errno == ENOENT) {
        this->broadcast_socket_.reset();
        this->broadcast_socket_bound_ = false;
        break;  // socket is closed; remaining addresses won't succeed either
      }
    }
  }
#else
  for (const auto &saddr : this->sockaddrs_) {
    auto result =
        this->broadcast_socket_->sendto(data, size, 0, reinterpret_cast<const sockaddr *>(&saddr), sizeof(saddr));
    if (result < 0)
      ESP_LOGW(TAG, "sendto() error %d", errno);
  }
#endif
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  auto iface = IPAddress(0, 0, 0, 0);
  for (const auto &saddr : this->ipaddrs_) {
    if (this->udp_client_.beginPacketMulticast(saddr, this->broadcast_port_, iface, 128) != 0) {
      this->udp_client_.write(data, size);
      auto result = this->udp_client_.endPacket();
      if (result == 0)
        ESP_LOGW(TAG, "udp.write() error");
    }
  }
#endif
}
}  // namespace esphome::udp

#endif
