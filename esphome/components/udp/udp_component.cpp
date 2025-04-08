#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "udp_component.h"

namespace esphome {
namespace udp {

static const char *const TAG = "udp";

void UDPComponent::setup() {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  for (const auto &address : this->addresses_) {
#if USE_NETWORK_IPV6
    struct sockaddr_in6 saddr {};
#else
    struct sockaddr saddr {};
#endif

    auto err = socket::set_sockaddr(reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr), address, this->port_);
    if (err == 0) {
      ESP_LOGV(TAG, "Couldn't set sockaddr %d", errno);
    }
    this->sockaddrs_.push_back(saddr);
  }
  // set up broadcast socket
  if (this->should_broadcast_) {
    this->broadcast_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (this->broadcast_socket_ == nullptr) {
      this->mark_failed();
      this->status_set_error("Could not create socket");
      return;
    }
    int enable = 1;
    int8_t err;
#if USE_NETWORK_IPV6
    this->broadcast_socket6_ = socket::socket(AF_INET6, SOCK_DGRAM, IPPROTO_IPV6);
    if (this->broadcast_socket6_ == nullptr) {
      this->mark_failed();
      this->status_set_error("Could not create IPv6 socket");
      return;
    }
    err = this->broadcast_socket6_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning("IPv6 Socket unable to set reuseaddr");
      // we can still continue
    }

    uint8_t netif_index = 2;
    err = this->broadcast_socket6_->setsockopt(IPPROTO_IPV6, IPV6_MULTICAST_IF, &netif_index, sizeof(uint8_t));
    if (err != 0) {
      this->status_set_warning("IPv6 Socket unable to set multicast");
    }
#endif
    err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning(LOG_STR("Socket unable to set reuseaddr"));
      // we can still continue
    }
    err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning(LOG_STR("Socket unable to set broadcast"));
    }
  }
  // create listening socket if we either want to subscribe to providers, or need to listen
  // for ping key broadcasts.
  if (this->should_listen_) {
#if USE_NETWORK_IPV6
    struct sockaddr_in6 server {};
#else
    struct sockaddr_in server {};
#endif
    int8_t err;
    if (this->listen_address_.has_value()) {
#if USE_NETWORK_IPV6
      server.sin6_port = htons(this->port_);
      struct ipv6_mreq v6imreq {};

      if (this->listen_address_.value().is_ip4()) {
        this->listen_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (this->listen_socket_ == nullptr) {
          this->mark_failed();
          this->status_set_error("Could not create socket");
          return;
        }
        server.sin6_family = AF_INET;
        err = inet_aton(this->listen_address_.value().str().c_str(), &v6imreq.ipv6mr_multiaddr);
        ESP_LOGI(TAG, "Configured multicast address %s", inet_ntoa(v6imreq.ipv6mr_multiaddr));
        if (err != 1) {
          ESP_LOGE(TAG, "Configured multicast address '%s' is invalid.", this->listen_address_.value().str().c_str());
          this->mark_failed();
          this->status_set_error("Unable to convert");
          return;
        }
        server.sin6_addr = v6imreq.ipv6mr_multiaddr;
        err = this->listen_socket_->setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, &v6imreq, sizeof(v6imreq));
        if (err < 0) {
          ESP_LOGE(TAG, "Socket unable to join: errno %d", errno);
          this->mark_failed();
          this->status_set_error("Unable to join socket");
          return;
        }
      }
      if (this->listen_address_.value().is_ip6()) {
        uint8_t netif_index = 2;
        this->listen_socket_ = socket::socket(AF_INET6, SOCK_DGRAM, IPPROTO_IPV6);
        if (this->listen_socket_ == nullptr) {
          this->mark_failed();
          this->status_set_error("Could not create socket");
          return;
        }
        server.sin6_family = AF_INET6;
        err = inet6_aton(this->listen_address_.value().str().c_str(), &v6imreq.ipv6mr_multiaddr);
        ESP_LOGI(TAG, "Configured multicast address %s", inet6_ntoa(v6imreq.ipv6mr_multiaddr));
        if (err != 1) {
          ESP_LOGE(TAG, "Configured multicast address '%s' is invalid.", this->listen_address_.value().str().c_str());
          this->mark_failed();
          this->status_set_error("Unable to convert");
          return;
        }
        // server.sin6_addr = v6imreq.ipv6mr_multiaddr;
        err = this->listen_socket_->setsockopt(IPPROTO_IPV6, IPV6_MULTICAST_IF, &netif_index, sizeof(uint8_t));
        if (err < 0) {
          ESP_LOGE(TAG, "Failed to set IPV6_MULTICAST_IF. Error %d", errno);
          this->mark_failed();
          this->status_set_error("Unable to join socket");
          return;
        }
        v6imreq.ipv6mr_interface = netif_index;
        err = this->listen_socket_->setsockopt(IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &v6imreq, sizeof(v6imreq));
        if (err < 0) {
          ESP_LOGE(TAG, "Socket unable to join: errno %d", errno);
          this->mark_failed();
          this->status_set_error("Unable to join socket");
          return;
        }
      }
#else
      this->listen_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
      server.sin_family = AF_INET;
      server.sin_port = htons(this->port_);
      struct ip_mreq imreq = {};
      inet_aton(this->listen_address_.value().str().c_str(), &imreq.imr_multiaddr);
      server.sin_addr.s_addr = imreq.imr_multiaddr.s_addr;
      err = this->listen_socket_->setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(imreq));
      if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        this->mark_failed();
        this->status_set_error("Failed to set IP_ADD_MEMBERSHIP");
        return;
      }
#endif
    } else {
      server.sin_family = AF_INET;
      server.sin_addr.s_addr = ESPHOME_INADDR_ANY;
      server.sin_port = htons(this->port_);
    }
    err = this->listen_socket_->setblocking(false);
    if (err < 0) {
      ESP_LOGE(TAG, "Unable to set nonblocking: errno %d", errno);
      this->mark_failed();
      this->status_set_error("Unable to set nonblocking");
      return;
    }
    int enable = 1;
    err = this->listen_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (err != 0) {
      this->status_set_warning("Socket unable to set reuseaddr");
      // we can still continue
    }
    err = this->listen_socket_->bind((struct sockaddr *) &server, sizeof(server));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      this->mark_failed();
      this->status_set_error("Unable to bind socket");
      return;
    }
  }
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  // 8266 and RP2040 `Duino
  for (const auto &address : this->addresses_) {
    auto ipaddr = IPAddress();
    ipaddr.fromString(address.c_str());
    this->ipaddrs_.push_back(ipaddr);
  }
  if (this->should_listen_)
    this->udp_client_.begin(this->listen_port_);
#endif
}

void UDPComponent::loop() {
  auto buf = std::vector<uint8_t>(MAX_PACKET_SIZE);
  if (this->should_listen_) {
    for (;;) {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
      auto len = this->listen_socket_->read(buf.data(), buf.size());
#endif
#ifdef USE_SOCKET_IMPL_LWIP_TCP
      auto len = this->udp_client_.parsePacket();
      if (len > 0)
        len = this->udp_client_.read(buf.data(), buf.size());
#endif
      if (len <= 0)
        break;
      buf.resize(len);
      ESP_LOGV(TAG, "Received packet of length %zu", len);
      this->packet_listeners_.call(buf);
    }
  }
}

void UDPComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UDP:\n"
                "  Listen Port: %u\n"
                "  Broadcast Port: %u",
                this->listen_port_, this->broadcast_port_);
  for (const auto &address : this->addresses_)
    ESP_LOGCONFIG(TAG, "  Address: %s", address.c_str());
  if (this->listen_address_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Listen address: %s", this->listen_address_.value().str().c_str());
  }
  ESP_LOGCONFIG(TAG,
                "  Broadcasting: %s\n"
                "  Listening: %s",
                YESNO(this->should_broadcast_), YESNO(this->should_listen_));
}

void UDPComponent::send_packet(const uint8_t *data, size_t size) {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  for (const auto &saddr : this->sockaddrs_) {
#if USE_NETWORK_IPV6
    if (saddr.sin6_family == AF_INET) {
      auto result =
          this->broadcast_socket_->sendto(data, len, 0, reinterpret_cast<const sockaddr *>(&saddr), sizeof(saddr));
      if (result < 0)
        ESP_LOGW(TAG, "sendto() error %d", errno);
    }
    if (saddr.sin6_family == AF_INET6) {
      auto result =
          this->broadcast_socket6_->sendto(data, len, 0, reinterpret_cast<const sockaddr *>(&saddr), sizeof(saddr));
      if (result < 0)
        ESP_LOGW(TAG, "sendto() error %d", errno);
    }
#else
    auto result = this->broadcast_socket_->sendto(data, len, 0, &saddr, sizeof(saddr));
    if (result < 0)
      ESP_LOGW(TAG, "sendto() error %d", errno);
#endif
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
}  // namespace udp
}  // namespace esphome

#endif
