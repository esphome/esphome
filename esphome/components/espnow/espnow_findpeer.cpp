#include "espnow_pairing.h"

#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <memory>
#include <map>

namespace esphome ::espnow_findpeer {

void ESPNowFindPeer::find(const uint8_t *mac, const uint8_t *data, size_t size) {
  if (this->current_status_ == FIND_PEER_WAITING ||
      memcmp(peer_address, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0 ||
      memcmp(peer_address, ESPNOW_MULTICAST_ADDR, ESP_NOW_ETH_ALEN) == 0) {
    return false;
  }
  this->start_channel_ = this->parent_->get_wifi_channel();
  this->current_channel_ = this->parent_->get_wifi_channel();

  this->current_address_ = mac;
  this->current_data_ = data;
  this->current_data_size_ = size;
  this->send_ping_();
}

void ESPNowFindPeer::loop() {
  if (this->current_loop_next_) {
    this->current_loop_next_ = false;
    this->current_channel_++;
    if (this->current_channel_ >= 11) {
      this->current_channel_ = 1;
    }
    this->parent_->set_wifi_channel(this->current_channel_);
    this->parent_->apply_wifi_channel();
    if (this->current_channel_ == this->start_channel_) {
      this->current_status_ == FIND_PEER_FAILED;
    } else {
      this->send_ping_();
    }
  }
}

void ESPNowFindPeer::send_ping_() {
  send_callback_t send_callback = [this](esp_err_t status) {
    if (this->current_status_ == FIND_PEER_WAITING) {
      if (status == ESP_OK) {
        this->current_status_ == FIND_PEER_SUCCESS;
      } else {
        this->current_loop_next_ = true;
      }
    }
  };
  esp_err_t err =
      this->parent_->send(this->current_address_, this->current_data_, this->current_data_size_, send_callback);
  if (err != ESP_OK) {
    this->current_status_ = FIND_PEER_WAITING;
  } else {
    this->current_status_ == FIND_PEER_FAILED;
  }
}
void ESPNowFindPeer::reset() {
  this->current_status_ = FIND_PEER_IDLE;
  this->start_channel_ = 0;
  this->current_channel_ = 0;
  this->current_address_ = nullptr;
  this->current_data_ = nullptr;
  this->current_data_size_ = 0;
}

}  // namespace esphome::espnow_findpeer
