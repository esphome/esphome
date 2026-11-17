#include "jsn_sr04t.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Very basic support for JSN_SR04T V3.0 distance sensor in mode 2

namespace esphome::jsn_sr04t {

ESPHOME_LOG_TAG(TAG, "jsn_sr04t.sensor");

void Jsnsr04tComponent::update() {
  this->write_byte((this->model_ == AJ_SR04M) ? 0x01 : 0x55);
  ESP_LOGV(TAG, "Request read out from sensor");
}

void Jsnsr04tComponent::loop() {
  this->read_available([this](uint8_t data) {
    ESP_LOGV(TAG, "Read byte from sensor: %x", data);
    if (const uint8_t *frame = this->reader_.feed(data))
      this->check_frame_(frame);
  });
}

void Jsnsr04tComponent::check_frame_(const uint8_t *frame) {
  uint8_t checksum = sum_bytes(frame, 3);
  if (frame[3] == checksum) {
    uint16_t distance = encode_uint16(frame[1], frame[2]);
    if (distance > ((this->model_ == AJ_SR04M) ? 200 : 250)) {
      float meters = distance / 1000.0f;
      ESP_LOGV(TAG, "Distance from sensor: %umm, %.3fm", distance, meters);
      this->publish_state(meters);
    } else {
      char hex_buf[format_hex_pretty_size(4)];
      ESP_LOGW(TAG, "Invalid data read from sensor: %s", format_hex_pretty_to(hex_buf, frame, 4));
    }
  } else {
    ESP_LOGW(TAG, "checksum failed: %02x != %02x", checksum, frame[3]);
  }
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
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::jsn_sr04t
