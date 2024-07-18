#include "jsn_sr04t.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Very basic support for JSN_SR04T V3.0 distance sensor in mode 2

namespace esphome {
namespace jsn_sr04t {

static const char *const TAG = "jsn_sr04t.sensor";

void Jsnsr04tComponent::update() {
  switch (this->model_) {
    case JSN_SR04T:
    case AJ_SR04M:
      this->write_byte(0x55);
      break;
    case RCWL_1655:
      this->buffer_.clear();
      this->write_byte(0xA0);
      break;
  }
  ESP_LOGV(TAG, "Request read out from sensor");
}

void Jsnsr04tComponent::loop() {
  uint8_t data;
  if (this->model_ == RCWL_1655) {
    while (this->available() > 0) {
      this->read_byte(&data);

      ESP_LOGV(TAG, "Read byte [%d] from sensor: %02X", this->buffer_.size(), data);
      this->buffer_.push_back(data);

      if (this->buffer_.size() == 3) {
        this->parse_buffer_rclw_1655_();
      }
    }
  } else {
    while (this->available() > 0) {
      uint8_t data;
      this->read_byte(&data);

      ESP_LOGV(TAG, "Read byte from sensor: %02X", data);

      if (this->buffer_.empty() && data != 0xFF)
        continue;

      this->buffer_.push_back(data);
      if (this->buffer_.size() == 4) {
        this->check_buffer_();
      }
    }
  }
}

void Jsnsr04tComponent::check_buffer_() {
  uint8_t checksum = 0;
  switch (this->model_) {
    case JSN_SR04T:
      checksum = this->buffer_[0] + this->buffer_[1] + this->buffer_[2];
      break;
    case AJ_SR04M:
      checksum = this->buffer_[1] + this->buffer_[2];
      break;
    default:
      return;
  }

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

void Jsnsr04tComponent::parse_buffer_rclw_1655_() {
  uint32_t distance = encode_uint24(this->buffer_[0], this->buffer_[1], this->buffer_[2]);
  float millimeters = distance / 1000.0f;
  float meters = millimeters / 1000.0f;
  ESP_LOGV(TAG, "Distance from sensor: %.0fmm, %.3fm", millimeters, meters);
  this->publish_state(meters);
  this->buffer_.clear();
}

void Jsnsr04tComponent::dump_config() {
  LOG_SENSOR("", "JST_SR04T Sensor", this);
  switch (this->model_) {
    case JSN_SR04T:
      ESP_LOGCONFIG(TAG, "  sensor model: jsn_sr04t");
      break;
    case AJ_SR04M:
      ESP_LOGCONFIG(TAG, "  sensor model: aj_sr04m");
      break;
    case RCWL_1655:
      ESP_LOGCONFIG(TAG, "  sensor model: RCWL-1655");
      break;
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace jsn_sr04t
}  // namespace esphome
