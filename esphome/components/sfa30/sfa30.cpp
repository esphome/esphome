#include "sfa30.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sfa30 {

static const char *const TAG = "sfa30";

static const uint16_t SFA30_CMD_GET_DEVICE_MARKING = 0xD060;
static const uint16_t SFA30_CMD_START_CONTINUOUS_MEASUREMENTS = 0x0006;
static const uint16_t SFA30_CMD_READ_MEASUREMENT = 0x0327;

void SFA30Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sfa30...");

  // Serial Number identification
  uint16_t raw_device_marking[16];
  if (!this->get_register(SFA30_CMD_GET_DEVICE_MARKING, raw_device_marking, 16, 5)) {
    ESP_LOGE(TAG, "Failed to read device marking");
    this->error_code_ = DEVICE_MARKING_READ_FAILED;
    this->mark_failed();
    return;
  }

  for (size_t i = 0; i < 16; i++) {
    this->device_marking_[i * 2] = static_cast<char>(raw_device_marking[i] >> 8);
    this->device_marking_[i * 2 + 1] = static_cast<char>(raw_device_marking[i] & 0xFF);
  }
  ESP_LOGD(TAG, "Device Marking: '%s'", this->device_marking_);

  if (!this->write_command(SFA30_CMD_START_CONTINUOUS_MEASUREMENTS)) {
    ESP_LOGE(TAG, "Error starting measurements.");
    this->error_code_ = MEASUREMENT_INIT_FAILED;
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "Sensor initialized");
}

void SFA30Component::dump_config() {
  ESP_LOGCONFIG(TAG, "sfa30:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case DEVICE_MARKING_READ_FAILED:
        ESP_LOGW(TAG, "Unable to read device marking!");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement initialization failed!");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Device Marking: '%s'", this->device_marking_);
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

void SFA30Component::update() {
  if (!this->write_command(SFA30_CMD_READ_MEASUREMENT)) {
    ESP_LOGW(TAG, "Error reading measurement!");
    this->status_set_warning();
    return;
  }

  this->set_timeout(5, [this]() {
    uint16_t raw_data[3];
    if (!this->read_data(raw_data, 3)) {
      ESP_LOGW(TAG, "Error reading measurement data!");
      this->status_set_warning();
      return;
    }

    if (this->formaldehyde_sensor_ != nullptr) {
      const float formaldehyde = raw_data[0] / 5.0f;
      this->formaldehyde_sensor_->publish_state(formaldehyde);
    }

    if (this->humidity_sensor_ != nullptr) {
      const float humidity = raw_data[1] / 100.0f;
      this->humidity_sensor_->publish_state(humidity);
    }

    if (this->temperature_sensor_ != nullptr) {
      const float temperature = raw_data[2] / 200.0f;
      this->temperature_sensor_->publish_state(temperature);
    }

    this->status_clear_warning();
  });
}

}  // namespace sfa30
}  // namespace esphome
