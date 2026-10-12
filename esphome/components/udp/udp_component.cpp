#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "udp_component.h"

namespace esphome::udp {

ESPHOME_LOG_TAG(TAG, "udp");
static constexpr uint32_t JOIN_POLL_ID = 0;
static constexpr uint32_t JOIN_POLL_INTERVAL_MS = 1000;

void UDPComponent::setup() {
  for (const char *const *it = this->addresses_; it != nullptr && *it != nullptr; it++) {
    const char *address = *it;
    struct sockaddr saddr {};
    if (socket::set_sockaddr(&saddr, sizeof(saddr), address, this->broadcast_port_) == 0) {
      ESP_LOGW(TAG, "Invalid address %s", address);
      // A dropped address silently receives nothing; surface the misconfiguration
      this->status_set_warning(LOG_STR("invalid address"));
      continue;
    }
    this->sockaddrs_.push_back(saddr);
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
  }
  // create listening socket if we either want to subscribe to providers, or need to listen
  // for ping key broadcasts.
  if (this->should_listen_) {
    this->listen_socket_ = socket::socket_udp(AF_INET, IPPROTO_IP);
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
    struct sockaddr_in server {};

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = ESPHOME_INADDR_ANY;
    server.sin_port = htons(this->listen_port_);

    if (this->listen_address_ != nullptr) {
      inet_aton(this->listen_address_, &server.sin_addr);
      // Joined once the network is up; RP2040 has no interface to join before then
      this->set_interval(JOIN_POLL_ID, JOIN_POLL_INTERVAL_MS, [this]() {
        if (network::is_connected() && this->join_multicast_())
          this->cancel_interval(JOIN_POLL_ID);
      });
    }

    err = this->listen_socket_->bind((struct sockaddr *) &server, sizeof(server));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      this->status_set_error(LOG_STR("Unable to bind socket"));
      this->mark_failed();
      return;
    }
  }
}

bool UDPComponent::join_multicast_() {
  if (socket::join_multicast_group(this->listen_socket_.get(), this->listen_address_)) {
    ESP_LOGD(TAG, "Joined multicast %s", this->listen_address_);
    if (this->join_warning_) {
      this->join_warning_ = false;
      this->status_clear_warning();
    }
    return true;
  }
  // Log the first failure only; the poll keeps retrying every second
  if (!this->join_failed_) {
    this->join_failed_ = true;
    ESP_LOGW(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
    // One warning bit: only a warning raised here is cleared on success, one from setup() stays
    if (!this->status_has_warning()) {
      this->status_set_warning(LOG_STR("Failed to set IP_ADD_MEMBERSHIP"));
      this->join_warning_ = true;
    }
  }
  return false;
}

void UDPComponent::loop() {
  if (this->should_listen_) {
    std::array<uint8_t, MAX_PACKET_SIZE> buf;
    for (;;) {
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
  for (const auto &saddr : this->sockaddrs_) {
    auto result = this->broadcast_socket_->sendto(data, size, 0, &saddr, sizeof(saddr));
    if (result < 0) {
      ESP_LOGW(TAG, "sendto() error %d", errno);
    }
  }
}
}  // namespace esphome::udp

#endif
