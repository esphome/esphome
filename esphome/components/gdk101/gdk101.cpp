#include "gdk101.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::gdk101 {

static const char *const TAG = "gdk101";
static constexpr uint8_t NUMBER_OF_READ_RETRIES = 5;
static constexpr uint8_t NUMBER_OF_RESET_RETRIES = 30;
static constexpr uint32_t RESET_INTERVAL_ID = 0;
static constexpr uint32_t RESET_INTERVAL_MS = 1000;

void GDK101Component::update() {
  if (!this->reset_complete_)
    return;

  uint8_t data[2];
  if (!this->read_dose_1m_(data)) {
    this->status_set_warning(LOG_STR("Failed to read dose 1m"));
    return;
  }

  if (!this->read_dose_10m_(data)) {
    this->status_set_warning(LOG_STR("Failed to read dose 10m"));
    return;
  }

  if (!this->read_status_(data)) {
    this->status_set_warning(LOG_STR("Failed to read status"));
    return;
  }

  if (!this->read_measurement_duration_(data)) {
    this->status_set_warning(LOG_STR("Failed to read measurement duration"));
    return;
  }
  this->status_clear_warning();
}

void GDK101Component::setup() {
  if (!this->try_reset_()) {
    // Sensor MCU boots slowly after power cycle — retry on a short interval
    this->reset_retries_remaining_ = NUMBER_OF_RESET_RETRIES;
    this->set_interval(RESET_INTERVAL_ID, RESET_INTERVAL_MS, [this]() {
      if (this->try_reset_()) {
        if (this->reset_complete_) {
          this->update();
        }
        return;
      }
      if (--this->reset_retries_remaining_ == 0) {
        this->cancel_interval(RESET_INTERVAL_ID);
        this->mark_failed(LOG_STR("Reset failed after retries"));
      }
    });
  }
}

/// Attempt to reset the sensor and read firmware version. Returns true on success or hard failure.
bool GDK101Component::try_reset_() {
  uint8_t data[2] = {0};
  if (!this->reset_sensor_(data)) {
    this->status_set_warning(LOG_STR("Sensor not answering reset, will retry"));
    return false;
  }
  if (data[0] != 1) {
    this->status_set_warning(LOG_STR("Reset not acknowledged, will retry"));
    return false;
  }
  delay(10);
  if (!this->read_fw_version_(data)) {
    this->cancel_interval(RESET_INTERVAL_ID);
    this->mark_failed(LOG_STR("Failed to read firmware version"));
    return true;
  }
  this->reset_complete_ = true;
  this->status_clear_warning();
  this->cancel_interval(RESET_INTERVAL_ID);
  return true;
}

void GDK101Component::dump_config() {
  ESP_LOGCONFIG(TAG, "GDK101:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Average Radaition Dose per 1 minute", this->rad_1m_sensor_);
  LOG_SENSOR("  ", "Average Radaition Dose per 10 minutes", this->rad_10m_sensor_);
  LOG_SENSOR("  ", "Status", this->status_sensor_);
  LOG_SENSOR("  ", "Measurement Duration", this->measurement_duration_sensor_);
#endif  // USE_SENSOR

#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Vibration Status", this->vibration_binary_sensor_);
#endif  // USE_BINARY_SENSOR

#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Firmware Version", this->fw_version_text_sensor_);
#endif  // USE_TEXT_SENSOR
}

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
  return this->read_bytes_with_retry_(GDK101_REG_RESET, data, 2);
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
#ifdef USE_TEXT_SENSOR
  if (this->fw_version_text_sensor_ != nullptr) {
    if (!this->read_bytes(GDK101_REG_READ_FIRMWARE, data, 2)) {
      ESP_LOGE(TAG, "Updating GDK101 failed!");
      return false;
    }

    // max 8: "255.255" (7 chars) + null
    char buf[8];
    snprintf(buf, sizeof(buf), "%d.%d", data[0], data[1]);
    this->fw_version_text_sensor_->publish_state(buf);
  }
#endif  // USE_TEXT_SENSOR
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

}  // namespace esphome::gdk101
