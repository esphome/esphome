#include "ajsr04m.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ajsr04m {

static const char *const TAG = "ajsr04m";

static const uint8_t AJSR04M_START_BYTE = 0xFF;

void AJSR04MComponent::loop() {
  // Expected frame is:
  //   0xFF - start byte
  //   0xnn - distance, upper byte
  //   0xnn - distance, lower byte
  //   0xnn - checksum: upper + lower == 1
  uint8_t data[3];
  while (available() >= 4) {
    if (read() != AJSR04M_START_BYTE) {
      continue;
    }

    read_array(data, 3);
    ESP_LOGD(TAG, "Read bytes %02X %02X %02X", data[0], data[1], data[2]);

    uint8_t expected_checksum = data[0] + data[1];
    if (expected_checksum - data[2] != 1) {
      ESP_LOGE(TAG, "Checksum mismatch. Expected %02X. Got %02X.", expected_checksum, data[2]);
      continue;
    }

    float distance = ((data[0] << 8) + data[1]) / 10.0f;
    ESP_LOGD(TAG, "Calculated distance %.1fcm", distance);

    this->distance_sensor_->publish_state(distance);
  }
}

float AJSR04MComponent::get_setup_priority() const { return setup_priority::DATA; }

void AJSR04MComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "AJSR04M:");
  LOG_SENSOR("  ", "Distance", this->distance_sensor_);
  this->check_uart_settings(9600);
}

}  // namespace ajsr04m
}  // namespace esphome
