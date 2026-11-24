// Datasheet https://wiki.dfrobot.com/_A02YYUW_Waterproof_Ultrasonic_Sensor_SKU_SEN0311

#include "a02yyuw.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::a02yyuw {

ESPHOME_LOG_TAG(TAG, "a02yyuw.sensor");

void A02yyuwComponent::loop() {
  this->read_available([this](uint8_t data) {
    if (const uint8_t *frame = this->reader_.feed(data))
      this->check_frame_(frame);
  });
}

void A02yyuwComponent::check_frame_(const uint8_t *frame) {
  uint8_t checksum = sum_bytes(frame, 3);
  if (frame[3] == checksum) {
    float distance = encode_uint16(frame[1], frame[2]);
    if (distance > 30) {
      ESP_LOGV(TAG, "Distance from sensor: %f mm", distance);
      this->publish_state(distance);
    } else {
      char hex_buf[format_hex_pretty_size(4)];
      ESP_LOGW(TAG, "Invalid data read from sensor: %s", format_hex_pretty_to(hex_buf, frame, 4));
    }
  } else {
    ESP_LOGW(TAG, "checksum failed: %02x != %02x", checksum, frame[3]);
  }
}

void A02yyuwComponent::dump_config() { LOG_SENSOR("", "A02yyuw Sensor", this); }

}  // namespace esphome::a02yyuw
