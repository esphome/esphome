#include "espnow_transport.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace espnow {

static const char *const TAG = "espnow.transport";

bool ESPNowTransport::should_send() { return this->parent_ != nullptr && !this->parent_->is_failed(); }

void ESPNowTransport::setup() {
  PacketTransport::setup();

  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "ESPNow component not set");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Registering ESP-NOW handlers");
  ESP_LOGI(TAG, "Peer address: %02X:%02X:%02X:%02X:%02X:%02X", this->peer_address_[0], this->peer_address_[1],
           this->peer_address_[2], this->peer_address_[3], this->peer_address_[4], this->peer_address_[5]);

  // Register received handler
  this->parent_->register_received_handler(this);

  // Register broadcasted handler
  this->parent_->register_broadcasted_handler(this);
}

void ESPNowTransport::send_packet(const std::vector<uint8_t> &buf) const {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "ESPNow component not set");
    return;
  }

  if (buf.empty()) {
    ESP_LOGW(TAG, "Attempted to send empty packet");
    return;
  }

  if (buf.size() > ESP_NOW_MAX_DATA_LEN) {
    ESP_LOGE(TAG, "Packet too large: %zu bytes (max %d)", buf.size(), ESP_NOW_MAX_DATA_LEN);
    return;
  }

  // Send to configured peer address
  this->parent_->send(this->peer_address_.data(), buf.data(), buf.size(), [](esp_err_t err) {
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Send failed: %d", err);
    }
  });
}

bool ESPNowTransport::on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) {
  ESP_LOGV(TAG, "Received packet of size %u from %02X:%02X:%02X:%02X:%02X:%02X", size, info.src_addr[0],
           info.src_addr[1], info.src_addr[2], info.src_addr[3], info.src_addr[4], info.src_addr[5]);

  if (data == nullptr || size == 0) {
    ESP_LOGW(TAG, "Received empty or null packet");
    return false;
  }

  this->packet_buffer_.resize(size);
  memcpy(this->packet_buffer_.data(), data, size);
  this->process_(this->packet_buffer_);
  return false;  // Allow other handlers to run
}

bool ESPNowTransport::on_broadcasted(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) {
  ESP_LOGV(TAG, "Received broadcast packet of size %u from %02X:%02X:%02X:%02X:%02X:%02X", size, info.src_addr[0],
           info.src_addr[1], info.src_addr[2], info.src_addr[3], info.src_addr[4], info.src_addr[5]);

  if (data == nullptr || size == 0) {
    ESP_LOGW(TAG, "Received empty or null broadcast packet");
    return false;
  }

  this->packet_buffer_.resize(size);
  memcpy(this->packet_buffer_.data(), data, size);
  this->process_(this->packet_buffer_);
  return false;  // Allow other handlers to run
}

}  // namespace espnow
}  // namespace esphome

#endif  // USE_ESP32
