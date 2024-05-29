#include "jsn_sr04t.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Very basic support for JSN_SR04T V3.0 distance sensor in mode 2

namespace esphome {
namespace jsn_sr04t {

static const char *const TAG = "jsn_sr04t.sensor";

void Jsnsr04tComponent::update() {
  this->write_byte(0x55);
  ESP_LOGV(TAG, "Request read out from sensor");
}

void Jsnsr04tComponent::loop() {
  while (this->available() > 0) {
    uint8_t data;
    this->read_byte(&data);

    ESP_LOGV(TAG, "Read byte from sensor: %x", data);

    if (this->buffer_.empty() && data != 0xFF)
      continue;

    this->buffer_.push_back(data);
    if (this->buffer_.size() == 4)
      this->check_buffer_();
  }
}

void Jsnsr04tComponent::check_buffer_() {
  uint8_t checksum = this->buffer_[0] + this->buffer_[1] + this->buffer_[2];
  if (this->buffer_[3] == checksum) {
    uint16_t distance = encode_uint16(this->buffer_[1], this->buffer_[2]);
    if (distance > 250) {
      float meters = distance / 1000.0f;
      ESP_LOGV(TAG, "Distance from sensor: %umm, %.3fm", distance, meters);
      this->publish_state(meters);
    } else {
      ESP_LOGW(TAG, "Invalid data read from sensor: %s", format_hex_pretty(this->buffer_).c_str());
    }
  } else {
    ESP_LOGW(TAG, "checksum failed: %02x != %02x", checksum, this->buffer_[3]);
  }
  this->buffer_.clear();
}

void Jsnsr04tComponent::dump_config() {
  LOG_SENSOR("", "JST_SR04T Sensor", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace jsn_sr04t
}  // namespace esphome
