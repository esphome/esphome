#include "midea_dongle.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace midea_dongle {

static const char *TAG = "midea_dongle";

void MideaDongle::loop() {
  while (this->available()) {
    const uint8_t rx = this->read();
    if (this->idx_ <= OFFSET_LENGTH) {
      if (this->idx_ == OFFSET_LENGTH) {
        if (rx <= OFFSET_BODY || rx >= sizeof(this->buf_)) {
          this->reset_();
          continue;
        }
        this->cnt_ = rx;
      } else if (rx != SYNC_BYTE) {
        continue;
      }
    }
    this->buf_[this->idx_++] = rx;
    if (--this->cnt_)
      continue;
    this->reset_();
    const BaseFrame frame(this->buf_);
    ESP_LOGD(TAG, "RX: %s", frame.to_string().c_str());
    if (!frame.is_valid()) {
      ESP_LOGW(TAG, "RX: frame check failed!");
      continue;
    }
    if (frame.get_type() == QUERY_NETWORK) {
      this->notify_.set_type(QUERY_NETWORK);
      this->need_notify_ = true;
      continue;
    }
    if (this->appliance_ != nullptr)
      this->appliance_->on_frame(frame);
  }
}

void MideaDongle::update() {
  const bool is_conn = WiFi.isConnected();
  uint8_t wifi_strength = 0;
  if (!this->rssi_timer_) {
    if (is_conn)
      wifi_strength = 4;
  } else if (is_conn) {
    if (--this->rssi_timer_) {
      wifi_strength = this->notify_.get_signal_strength();
    } else {
      this->rssi_timer_ = 60;
      const long dbm = WiFi.RSSI();
      if (dbm > -63)
        wifi_strength = 4;
      else if (dbm > -75)
        wifi_strength = 3;
      else if (dbm > -88)
        wifi_strength = 2;
      else if (dbm > -100)
        wifi_strength = 1;
    }
  } else {
    this->rssi_timer_ = 1;
  }
  if (this->notify_.is_connected() != is_conn) {
    this->notify_.set_connected(is_conn);
    this->need_notify_ = true;
  }
  if (this->notify_.get_signal_strength() != wifi_strength) {
    this->notify_.set_signal_strength(wifi_strength);
    this->need_notify_ = true;
  }
  if (!--this->notify_timer_) {
    this->notify_.set_type(NETWORK_NOTIFY);
    this->need_notify_ = true;
  }
  if (this->need_notify_) {
    ESP_LOGD(TAG, "TX: notify WiFi STA %s, signal strength %d", is_conn ? "connected" : "not connected", wifi_strength);
    this->need_notify_ = false;
    this->notify_timer_ = 600;
    this->notify_.finalize();
    this->write_frame(this->notify_);
    return;
  }
  if (this->appliance_ != nullptr)
    this->appliance_->on_update();
}

void MideaDongle::write_frame(const Frame &frame) {
  this->write_array(frame.data(), frame.size());
  ESP_LOGD(TAG, "TX: %s", frame.to_string().c_str());
}

}  // namespace midea_dongle
}  // namespace esphome
