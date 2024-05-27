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
  struct sockaddr_storage saddr {};
  socket::set_sockaddr(reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr), "255.255.255.255", this->port_);
  uint8_t buffer[6 + sizeof this->macaddr_ * 16];
  memcpy(buffer, PREFIX, sizeof(PREFIX));
  for (size_t i = 0; i != 16; i++) {
    memcpy(buffer + i * sizeof(this->macaddr_) + sizeof(PREFIX), this->macaddr_, sizeof(this->macaddr_));
  }
  auto result = this->broadcast_socket_->sendto(buffer, sizeof(buffer), 0, reinterpret_cast<const sockaddr *>(&saddr),
                                                sizeof(saddr));
  if (result < 0)
    ESP_LOGW(TAG, "sendto() error %d", errno);
}

void WakeOnLanButton::setup() {
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

}  // namespace wake_on_lan
}  // namespace esphome