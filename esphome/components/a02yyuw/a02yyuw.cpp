// Datasheet https://wiki.dfrobot.com/_A02YYUW_Waterproof_Ultrasonic_Sensor_SKU_SEN0311

#include "a02yyuw.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace a02yyuw {

static const char *const TAG = "a02yyuw.sensor";
static const uint8_t MAX_DATA_LENGTH_BYTES = 4;

void A02yyuwComponent::loop() {
  uint8_t data;
  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      buffer_.push_back(data);
      this->check_buffer_();
    }
  }
}

void A02yyuwComponent::check_buffer_() {
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
          if (distance > 30) {
            ESP_LOGV(TAG, "Distance from sensor: %f mm", distance);
            this->publish_state(distance);
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

void A02yyuwComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "A02yyuw Sensor:");
  LOG_SENSOR("  ", "Distance", this);
}

}  // namespace a02yyuw
}  // namespace esphome
