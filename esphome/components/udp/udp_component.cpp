#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "udp_component.h"

namespace esphome::udp {

static const char *const TAG = "udp";

void UDPComponent::setup() {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  this->sockaddrs_.init(this->addresses_.size());
  for (const auto &address : this->addresses_) {
    SockaddrEntry entry{};
    entry.len =
        socket::set_sockaddr((struct sockaddr *) &entry.addr, sizeof(entry.addr), address, this->broadcast_port_);
    this->sockaddrs_.push_back(entry);
  }
  // set up send socket(s)
  if (this->should_broadcast_) {
    // AF_INET for IPv4 (broadcast requires IPv4 socket)
    this->broadcast_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (this->broadcast_socket_ == nullptr) {
      this->status_set_error(LOG_STR("Could not create socket"));
      this->mark_failed();
      return;
    }
    int enable = 1;
    auto err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (err != 0)
      this->status_set_warning(LOG_STR("Socket unable to set reuseaddr"));
    err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));
    if (err != 0)
      this->status_set_warning(LOG_STR("Socket unable to set broadcast"));
#if USE_NETWORK_IPV6
    // Create IPv6 send socket if any destination is IPv6
    for (const auto &entry : this->sockaddrs_) {
      if (entry.addr.ss_family == AF_INET6) {
        this->send_socket_v6_ = socket::socket(AF_INET6, SOCK_DGRAM, IPPROTO_IP);
        if (this->send_socket_v6_ == nullptr) {
          this->status_set_error(LOG_STR("Could not create IPv6 socket"));
          this->mark_failed();
          return;
        }
        err = this->send_socket_v6_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
        if (err != 0)
          this->status_set_warning(LOG_STR("IPv6 socket unable to set reuseaddr"));
        break;
      }
    }
#endif
  }
  // create listening socket
  if (this->should_listen_) {
    if (this->listen_address_.has_value()) {
      // Multicast: create socket with family matching the multicast address
      char addr_buf[network::IP_ADDRESS_BUFFER_SIZE];
      this->listen_address_.value().str_to(addr_buf);
      int af = (strchr(addr_buf, ':') != nullptr) ? AF_INET6 : AF_INET;
      this->listen_socket_ = socket::socket_loop_monitored(af, SOCK_DGRAM, IPPROTO_IP);
    } else {
      // Non-multicast: dual-stack on IPv6 builds, AF_INET on IPv4 builds
      this->listen_socket_ = socket::socket_ip_loop_monitored(SOCK_DGRAM, IPPROTO_IP);
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
    if (err != 0)
      this->status_set_warning(LOG_STR("Socket unable to set reuseaddr"));

    struct sockaddr_storage server {};
    socklen_t server_len = 0;

    if (this->listen_address_.has_value()) {
      char addr_buf[network::IP_ADDRESS_BUFFER_SIZE];
      this->listen_address_.value().str_to(addr_buf);
      ESP_LOGD(TAG, "Join multicast %s", addr_buf);
      uint8_t mcast_ifindex = 0;
      server_len = socket::join_multicast_group(this->listen_socket_.get(), (struct sockaddr *) &server, sizeof(server),
                                                addr_buf, this->listen_port_, &mcast_ifindex);
      if (server_len == 0) {
        ESP_LOGE(TAG, "Failed to join multicast group. Error %d", errno);
        this->status_set_error(LOG_STR("Failed to join multicast group"));
        this->mark_failed();
        return;
      }
#if USE_NETWORK_IPV6
      // Set outgoing multicast interface on the IPv6 send socket using the same
      // netif index that join_multicast_group found. IPV6_MULTICAST_IF on ESP-IDF
      // LwIP always returns success regardless of index, so we must use a known-valid
      // index from the join probe rather than probing independently.
      if (mcast_ifindex != 0 && this->send_socket_v6_ != nullptr) {
        uint32_t ifidx = mcast_ifindex;
        this->send_socket_v6_->setsockopt(IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifidx, sizeof(ifidx));
      }
#endif
    } else {
      server_len = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->listen_port_);
    }

    err = this->listen_socket_->bind((struct sockaddr *) &server, server_len);
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      this->status_set_error(LOG_STR("Unable to bind socket"));
      this->mark_failed();
      return;
    }
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
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
      if (!this->listen_socket_->ready())
        break;
      auto len = this->listen_socket_->read(buf.data(), buf.size());
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
      auto len = this->udp_client_.parsePacket();
      if (len > 0)
        len = this->udp_client_.read(buf.data(), buf.size());
#endif
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
  for (const auto &entry : this->sockaddrs_) {
#if USE_NETWORK_IPV6
    if (entry.addr.ss_family == AF_INET6) {
      if (this->send_socket_v6_ != nullptr) {
        auto result = this->send_socket_v6_->sendto(data, size, 0,
                                                    reinterpret_cast<const struct sockaddr *>(&entry.addr), entry.len);
        if (result < 0)
          ESP_LOGW(TAG, "sendto() IPv6 error %d", errno);
      }
      continue;
    }
#endif
    auto result = this->broadcast_socket_->sendto(data, size, 0, reinterpret_cast<const struct sockaddr *>(&entry.addr),
                                                  entry.len);
    if (result < 0)
      ESP_LOGW(TAG, "sendto() error %d", errno);
  }
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
