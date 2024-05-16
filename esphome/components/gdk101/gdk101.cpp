#include "gdk101.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gdk101 {

static const char *const TAG = "gdk101";
static const uint8_t NUMBER_OF_READ_RETRIES = 5;

void GDK101Component::update() {
  uint8_t data[2];
  if (!this->read_dose_1m_(data)) {
    this->status_set_warning("Failed to read dose 1m");
    return;
  }

  if (!this->read_dose_10m_(data)) {
    this->status_set_warning("Failed to read dose 10m");
    return;
  }

  if (!this->read_status_(data)) {
    this->status_set_warning("Failed to read status");
    return;
  }

  if (!this->read_measurement_duration_(data)) {
    this->status_set_warning("Failed to read measurement duration");
    return;
  }
  this->status_clear_warning();
}

void GDK101Component::setup() {
  uint8_t data[2];
  ESP_LOGCONFIG(TAG, "Setting up GDK101...");
  // first, reset the sensor
  if (!this->reset_sensor_(data)) {
    this->status_set_error("Reset failed!");
    this->mark_failed();
    return;
  }
  // sensor should acknowledge success of the reset procedure
  if (data[0] != 1) {
    this->status_set_error("Reset not acknowledged!");
    this->mark_failed();
    return;
  }
  delay(10);
  // read firmware version
  if (!this->read_fw_version_(data)) {
    this->status_set_error("Failed to read firmware version");
    this->mark_failed();
    return;
  }
}

void GDK101Component::dump_config() {
  ESP_LOGCONFIG(TAG, "GDK101:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with GDK101 failed!");
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Firmware Version", this->fw_version_sensor_);
  LOG_SENSOR("  ", "Average Radaition Dose per 1 minute", this->rad_1m_sensor_);
  LOG_SENSOR("  ", "Average Radaition Dose per 10 minutes", this->rad_10m_sensor_);
  LOG_SENSOR("  ", "Status", this->status_sensor_);
  LOG_SENSOR("  ", "Measurement Duration", this->measurement_duration_sensor_);
#endif  // USE_SENSOR

#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Vibration Status", this->vibration_binary_sensor_);
#endif  // USE_BINARY_SENSOR
}

float GDK101Component::get_setup_priority() const { return setup_priority::DATA; }

bool GDK101Component::read_bytes_with_retry_(uint8_t a_register, uint8_t *data, uint8_t len) {
  uint8_t retry = NUMBER_OF_READ_RETRIES;
  bool status = false;
  while (!status && retry) {
    status = this->read_bytes(a_register, data, len);
    retry--;
  }
  return status;
}

bool GDK101Component::reset_sensor_(uint8_t *data) {
  // It looks like reset is not so well designed in that sensor
  // After sending reset command it looks that sensor start performing reset and is unresponsible during read
  // after a while we can send another reset command and read "0x01" as confirmation
  // Documentation not going in to such details unfortunately
  if (!this->read_bytes_with_retry_(GDK101_REG_RESET, data, 2)) {
    ESP_LOGE(TAG, "Updating GDK101 failed!");
    return false;
  }

  return true;
}

bool GDK101Component::read_dose_1m_(uint8_t *data) {
#ifdef USE_SENSOR
  if (this->rad_1m_sensor_ != nullptr) {
    if (!this->read_bytes(GDK101_REG_READ_1MIN_AVG, data, 2)) {
      ESP_LOGE(TAG, "Updating GDK101 failed!");
      return false;
    }

    const float dose = data[0] + (data[1] / 100.0f);

    this->rad_1m_sensor_->publish_state(dose);
  }
#endif  // USE_SENSOR
  return true;
}

bool GDK101Component::read_dose_10m_(uint8_t *data) {
#ifdef USE_SENSOR
  if (this->rad_10m_sensor_ != nullptr) {
    if (!this->read_bytes(GDK101_REG_READ_10MIN_AVG, data, 2)) {
      ESP_LOGE(TAG, "Updating GDK101 failed!");
      return false;
    }

    const float dose = data[0] + (data[1] / 100.0f);

    this->rad_10m_sensor_->publish_state(dose);
  }
#endif  // USE_SENSOR
  return true;
}

bool GDK101Component::read_status_(uint8_t *data) {
  if (!this->read_bytes(GDK101_REG_READ_STATUS, data, 2)) {
    ESP_LOGE(TAG, "Updating GDK101 failed!");
    return false;
  }

#ifdef USE_SENSOR
  if (this->status_sensor_ != nullptr) {
    this->status_sensor_->publish_state(data[0]);
  }
#endif  // USE_SENSOR

#ifdef USE_BINARY_SENSOR
  if (this->vibration_binary_sensor_ != nullptr) {
    this->vibration_binary_sensor_->publish_state(data[1]);
  }
#endif  // USE_BINARY_SENSOR

  return true;
}

bool GDK101Component::read_fw_version_(uint8_t *data) {
#ifdef USE_SENSOR
  if (this->fw_version_sensor_ != nullptr) {
    if (!this->read_bytes(GDK101_REG_READ_FIRMWARE, data, 2)) {
      ESP_LOGE(TAG, "Updating GDK101 failed!");
      return false;
    }

    const float fw_version = data[0] + (data[1] / 10.0f);

    this->fw_version_sensor_->publish_state(fw_version);
  }
#endif  // USE_SENSOR
  return true;
}

bool GDK101Component::read_measurement_duration_(uint8_t *data) {
#ifdef USE_SENSOR
  if (this->measurement_duration_sensor_ != nullptr) {
    if (!this->read_bytes(GDK101_REG_READ_MEASURING_TIME, data, 2)) {
      ESP_LOGE(TAG, "Updating GDK101 failed!");
      return false;
    }

    const float meas_time = (data[0] * 60) + data[1];

    this->measurement_duration_sensor_->publish_state(meas_time);
  }
#endif  // USE_SENSOR
  return true;
}

}  // namespace gdk101
}  // namespace esphome
