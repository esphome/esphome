#include "midea_dongle.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace midea_dongle {

static const char *TAG = "midea_dongle";

void MideaDongle::loop() {
  while (this->available()) {
    const uint8_t rx = this->read();
    switch (this->idx_) {
      case OFFSET_START:
        if (rx != SYNC_BYTE)
          continue;
        break;
      case OFFSET_LENGTH:
        if (!rx || rx >= sizeof(buf_)) {
          this->reset_();
          continue;
        }
        this->cnt_ = rx;
    }
    this->buf_[this->idx_++] = rx;
    if (--this->cnt_)
      continue;
    this->reset_();
    BaseFrame frame(this->buf_);
    if (frame.get_type() == DEVICE_NETWORK) {
      this->need_notify_ = false;
      continue;
    }
    if (!frame.is_valid())
      continue;
    if (this->appliance_ != nullptr)
      this->appliance_->on_frame(frame);
  }
}

void MideaDongle::update() {
  bool is_conn = WiFi.isConnected();
  uint8_t wifi_stretch = 0;
  if (this->wifi_sensor_ == nullptr || !this->wifi_sensor_->has_state()) {
    if (is_conn)
      wifi_stretch = 4;
  } else {
    float dbm = this->wifi_sensor_->get_state();
    if (dbm >= -62.5)
      wifi_stretch = 4;
    else if (dbm > -100.0)
      wifi_stretch = static_cast<uint8_t>(0.08 * dbm) + 9;
  }
  if (this->notify_.is_connected() != is_conn) {
    this->notify_.set_connected(is_conn);
    this->need_notify_ = true;
  }
  if (this->notify_.get_signal_stretch() != wifi_stretch) {
    this->notify_.set_signal_stretch(wifi_stretch);
    this->need_notify_ = true;
  }
  if (this->need_notify_) {
    this->notify_.finalize();
    this->write_frame(this->notify_);
    return;
  }
  if (this->appliance_ != nullptr)
    this->appliance_->on_update();
}

}  // namespace midea_dongle
}  // namespace esphome
