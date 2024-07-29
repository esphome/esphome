#include "wake_on_lan.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace wake_on_lan {

static const char *const TAG = "wake_on_lan.button";
static const uint8_t PREFIX[6] = {255, 255, 255, 255, 255, 255};

void WakeOnLanButton::set_macaddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
  macaddr_[0] = a;
  macaddr_[1] = b;
  macaddr_[2] = c;
  macaddr_[3] = d;
  macaddr_[4] = e;
  macaddr_[5] = f;
}

void WakeOnLanButton::dump_config() {
  LOG_BUTTON("", "Wake-on-LAN Button", this);
  ESP_LOGCONFIG(TAG, "  Target MAC address: %02X:%02X:%02X:%02X:%02X:%02X", this->macaddr_[0], this->macaddr_[1],
                this->macaddr_[2], this->macaddr_[3], this->macaddr_[4], this->macaddr_[5]);
}

void WakeOnLanButton::press_action() {
  if (!network::is_connected()) {
    ESP_LOGW(TAG, "Network not connected");
    return;
  }
  ESP_LOGI(TAG, "Sending Wake-on-LAN Packet...");
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  struct sockaddr_storage saddr {};
  auto addr_len =
      socket::set_sockaddr(reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr), "255.255.255.255", this->port_);
  uint8_t buffer[6 + sizeof this->macaddr_ * 16];
  memcpy(buffer, PREFIX, sizeof(PREFIX));
  for (size_t i = 0; i != 16; i++) {
    memcpy(buffer + i * sizeof(this->macaddr_) + sizeof(PREFIX), this->macaddr_, sizeof(this->macaddr_));
  }
  if (this->broadcast_socket_->sendto(buffer, sizeof(buffer), 0, reinterpret_cast<const sockaddr *>(&saddr),
                                      addr_len) <= 0)
    ESP_LOGW(TAG, "sendto() error %d", errno);
#else
  IPAddress broadcast = IPAddress(255, 255, 255, 255);
  for (auto ip : esphome::network::get_ip_addresses()) {
    if (ip.is_ip4()) {
      if (this->udp_client_.beginPacketMulticast(broadcast, 9, ip, 128) != 0) {
        this->udp_client_.write(PREFIX, 6);
        for (size_t i = 0; i < 16; i++) {
          this->udp_client_.write(macaddr_, 6);
        }
        if (this->udp_client_.endPacket() != 0)
          return;
        ESP_LOGW(TAG, "WOL broadcast failed");
        return;
      }
    }
  }
  ESP_LOGW(TAG, "No ip4 addresses to broadcast to");
#endif
}

void WakeOnLanButton::setup() {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
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
#endif
}

}  // namespace wake_on_lan
}  // namespace esphome
