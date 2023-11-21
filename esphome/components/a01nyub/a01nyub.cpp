// Datasheet https://wiki.dfrobot.com/A01NYUB%20Waterproof%20Ultrasonic%20Sensor%20SKU:%20SEN0313

#include "a01nyub.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace a01nyub {

static const char *const TAG = "a01nyub.sensor";
static const uint8_t MAX_DATA_LENGTH_BYTES = 4;

void A01nyubComponent::loop() {
  uint8_t data;
  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      buffer_.push_back(data);
      this->check_buffer_();
    }
  }
}

void A01nyubComponent::check_buffer_() {
  if (this->buffer_.size() >= MAX_DATA_LENGTH_BYTES) {
    size_t i;
    for (i = 0; i < this->buffer_.size(); i++) {
      // Look for the first packet
      if (this->buffer_[i] == 0xFF) {
        if (i + 1 + 3 < this->buffer_.size()) {  // Packet is not complete
          return;                                // Wait for completion
        }

        uint8_t checksum = (this->buffer_[i] + this->buffer_[i + 1] + this->buffer_[i + 2]) & 0xFF;
        if (this->buffer_[i + 3] == checksum) {
          float distance = (this->buffer_[i + 1] << 8) + this->buffer_[i + 2];
          if (distance > 280) {
            float meters = distance / 1000.0;
            ESP_LOGV(TAG, "Distance from sensor: %f mm, %f m", distance, meters);
            this->publish_state(meters);
          } else {
            ESP_LOGW(TAG, "Invalid data read from sensor: %s", format_hex_pretty(this->buffer_).c_str());
          }
        }
        break;
      }
    }
    this->buffer_.clear();
  }
}

void A01nyubComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "A01nyub Sensor:");
  LOG_SENSOR("  ", "Distance", this);
}

}  // namespace a01nyub
}  // namespace esphome
