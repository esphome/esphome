// Datasheet https://wiki.dfrobot.com/A01NYUB%20Waterproof%20Ultrasonic%20Sensor%20SKU:%20SEN0313

#include "a01nyub.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::a01nyub {

ESPHOME_LOG_TAG(TAG, "a01nyub.sensor");

void A01nyubComponent::loop() {
  this->read_available([this](uint8_t data) {
    if (const uint8_t *frame = this->reader_.feed(data))
      this->check_frame_(frame);
  });
}

void A01nyubComponent::check_frame_(const uint8_t *frame) {
  uint8_t checksum = sum_bytes(frame, 3);
  if (frame[3] == checksum) {
    float distance = encode_uint16(frame[1], frame[2]);
    if (distance > 280) {
      float meters = distance / 1000.0f;
      ESP_LOGV(TAG, "Distance from sensor: %f mm, %f m", distance, meters);
      this->publish_state(meters);
    } else {
      char hex_buf[format_hex_pretty_size(4)];
      ESP_LOGW(TAG, "Invalid data read from sensor: %s", format_hex_pretty_to(hex_buf, frame, 4));
    }
  } else {
    ESP_LOGW(TAG, "checksum failed: %02x != %02x", checksum, frame[3]);
  }
}

void A01nyubComponent::dump_config() { LOG_SENSOR("", "A01nyub Sensor", this); }

}  // namespace esphome::a01nyub
