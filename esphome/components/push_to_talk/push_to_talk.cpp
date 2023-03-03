#include "push_to_talk.h"

#ifdef USE_PUSH_TO_TALK

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esphome/components/network/ip_address.h"

#include "esphome/components/api/api_server.h"

#include <esp_netif.h>

namespace esphome {
namespace push_to_talk {

static const char *const TAG = "push_to_talk";

PushToTalk::PushToTalk() { global_push_to_talk = this; }

void PushToTalk::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PushToTalk...");
  this->socket_ = socket::socket_ip(SOCK_DGRAM, IPPROTO_IP);
  if (socket_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket.");
    this->mark_failed();
    return;
  }
  int enable = 1;
  int err = socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = socket_->setblocking(false);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    this->mark_failed();
    return;
  }

  this->binary_sensor_->add_on_state_callback([this](bool state) {
    if (state) {
      api::global_api_server->start_push_to_talk();
    } else {
      this->mic_->stop();
      this->running_ = false;
      api::global_api_server->stop_push_to_talk();

    }
  });

  this->mic_->add_data_callback([this](const std::vector<uint8_t> &data) {
    if (!this->running_) {
      return;
    }
    ESP_LOGD(TAG, "Received %d samples", data.size());
    this->socket_->sendto(data.data(), data.size(), 0, (struct sockaddr *) &this->dest_addr_, sizeof(this->dest_addr_));
  });
}

void PushToTalk::start(struct sockaddr_storage *addr, uint16_t port) {
  ESP_LOGD(TAG, "Starting PushToTalk...");
  memcpy(&this->dest_addr_, addr, sizeof(this->dest_addr_));
  if (this->dest_addr_.ss_family == AF_INET) {
    ((struct sockaddr_in *) &this->dest_addr_)->sin_port = htons(port);
  }
#if LWIP_IPV6
  else if (this->dest_addr_.ss_family == AF_INET6) {
    ((struct sockaddr_in6 *) &this->dest_addr_)->sin6_port = htons(port);
  }
#endif
  else {
    ESP_LOGW(TAG, "Unknown address family: %d", this->dest_addr_.ss_family);
    return;
  }
  this->running_ = true;
  this->mic_->start();
}

float PushToTalk::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

PushToTalk *global_push_to_talk = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#endif

}  // namespace push_to_talk
}  // namespace esphome
