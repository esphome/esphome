#include "aquaping.h"
#include "esphome/core/log.h"
#include <string>

namespace esphome {
namespace aquaping {

static const char *const TAG = "aquaping";

void AQUAPINGComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up aquaping...");

  // Get firmware version
}

void AQUAPINGComponent::dump_config() { LOG_I2C_DEVICE(this); }

void AQUAPINGComponent::update() {
  const uint8_t num_bytes = 12;
  uint8_t buffer[num_bytes];

  bool read_success = this->read_bytes_raw(buffer, num_bytes);

  if (read_success) {
    this->status_clear_warning();

#ifdef USE_SENSOR
    if (this->quiet_count_sensor_ != nullptr) {
      this->quiet_count_sensor_->publish_state(buffer[2]);
    }

    if (this->leak_count_sensor_ != nullptr) {
      this->leak_count_sensor_->publish_state(buffer[3]);
    }

    if (this->noise_count_sensor_ != nullptr) {
      this->noise_count_sensor_->publish_state(buffer[4]);
    }

    if (this->version_sensor_ != nullptr) {
      this->version_sensor_->publish_state(buffer[11]);
    }
#endif

#ifdef USE_BINARY_SENSOR
    if (this->alarm_binary_sensor_ != nullptr) {
      this->alarm_binary_sensor_->publish_state(buffer[0]);
    }

    if (this->noise_alert_binary_sensor_ != nullptr) {
      this->noise_alert_binary_sensor_->publish_state(buffer[1]);
    }

    if (this->led_binary_sensor_ != nullptr) {
      this->led_binary_sensor_->publish_state(buffer[9]);
    }

    if (this->sleep_binary_sensor_ != nullptr) {
      this->sleep_binary_sensor_->publish_state(buffer[10]);
    }
#endif

  } else {
    this->status_set_warning();
    ESP_LOGD(TAG, "Read failure. Skipping update.");
  }
}

}  // namespace aquaping
}  // namespace esphome
