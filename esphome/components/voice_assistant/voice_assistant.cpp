#include "voice_assistant.h"

#include "esphome/core/log.h"

#include "esphome/components/api/api_server.h"

namespace esphome {
namespace voice_assistant {

static const char *const TAG = "voice_assistant";

bool VoiceAssistant::setup_udp_socket_() {
  ESP_LOGCONFIG(TAG, "Setting up PushToTalk...");
  this->socket_ = socket::socket_ip(SOCK_DGRAM, IPPROTO_IP);
  if (socket_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket.");
    return false;
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
    return false;
  }

  this->mic_->add_data_callback([this](const std::vector<uint8_t> &data) {
    if (!this->running_) {
      return;
    }
    ESP_LOGD(TAG, "Received %d samples", data.size());
    this->socket_->sendto(data.data(), data.size(), 0, (struct sockaddr *) &this->dest_addr_, sizeof(this->dest_addr_));
  });
  return true;
}

void VoiceAssistant::start(struct sockaddr_storage *addr, uint16_t port) {
  ESP_LOGD(TAG, "Starting Voice Assistant...");
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

void VoiceAssistant::request_start_() {
  ESP_LOGV(TAG, "Requesting start...");
  api::global_api_server->start_voice_assistant();
}

void VoiceAssistant::signal_stop_() {
  ESP_LOGV(TAG, "Signaling stop...");
  this->mic_->stop();
  this->running_ = false;
  api::global_api_server->stop_voice_assistant();
}

VoiceAssistant *global_voice_assistant = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace voice_assistant
}  // namespace esphome
