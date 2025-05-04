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
    struct sockaddr saddr {};
    socket::set_sockaddr(&saddr, sizeof(saddr), address, this->broadcast_port_);
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
    auto err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning("Socket unable to set reuseaddr");
      // we can still continue
    }
    err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
    if (err != 0) {
      this->status_set_warning("Socket unable to set broadcast");
    }
  }
  // create listening socket if we either want to subscribe to providers, or need to listen
  // for ping key broadcasts.
  if (this->should_listen_) {
    this->listen_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (this->listen_socket_ == nullptr) {
      this->mark_failed();
      this->status_set_error("Could not create socket");
      return;
    }
    auto err = this->listen_socket_->setblocking(false);
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
    struct sockaddr_in server {};

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = ESPHOME_INADDR_ANY;
    server.sin_port = htons(this->listen_port_);

    if (this->listen_address_.has_value()) {
      struct ip_mreq imreq = {};
      imreq.imr_interface.s_addr = ESPHOME_INADDR_ANY;
      inet_aton(this->listen_address_.value().str().c_str(), &imreq.imr_multiaddr);
      server.sin_addr.s_addr = imreq.imr_multiaddr.s_addr;
      ESP_LOGD(TAG, "Join multicast %s", this->listen_address_.value().str().c_str());
      err = this->listen_socket_->setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(imreq));
      if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        this->mark_failed();
        this->status_set_error("Failed to set IP_ADD_MEMBERSHIP");
        return;
      }
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
  ESP_LOGCONFIG(TAG, "UDP:");
  ESP_LOGCONFIG(TAG, "  Listen Port: %u", this->listen_port_);
  ESP_LOGCONFIG(TAG, "  Broadcast Port: %u", this->broadcast_port_);
  for (const auto &address : this->addresses_)
    ESP_LOGCONFIG(TAG, "  Address: %s", address.c_str());
  if (this->listen_address_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Listen address: %s", this->listen_address_.value().str().c_str());
  }
  ESP_LOGCONFIG(TAG, "  Broadcasting: %s", YESNO(this->should_broadcast_));
  ESP_LOGCONFIG(TAG, "  Listening: %s", YESNO(this->should_listen_));
}

void UDPComponent::send_packet(const uint8_t *data, size_t size) {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  for (const auto &saddr : this->sockaddrs_) {
    auto result = this->broadcast_socket_->sendto(data, size, 0, &saddr, sizeof(saddr));
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
}  // namespace udp
}  // namespace esphome

#endif
