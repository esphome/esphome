#include "tfluna.h"
#include <cstddef>
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome::tfluna {

// see https://files.waveshare.com/upload/a/ac/SJ-PM-TF-Luna_A05_Product_Manual.pdf
static const uint8_t SAVE_REGISTER = 0x20;
static const uint8_t VERSION_REVISION_REGISTER = 0x0A;
static const uint8_t DISTANCE_LOW_REGISTER = 0x00;
static const uint8_t RESTORE_FACTORY_DEFAULTS_REGISTER = 0x29;
static const uint8_t SHUTDOWN_REBOOT_REGISTER = 0x21;
static const uint8_t MODE_REGISTER = 0x23;
static const uint8_t MODE_TRIGGER = 0x01;
static const uint8_t TRIGGER_ONESHOT_REGISTER = 0x24;
static const char *const TAG = "tfluna";

void TFLuna::dump_config() {
  ESP_LOGCONFIG(TAG, "TF-Luna (i2c):");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Distance:", this->distance_sensor_);
  LOG_SENSOR("  ", "Temperature:", this->temperature_sensor_);
  LOG_SENSOR("  ", "Signal Strength:", this->signal_strength_sensor_);
  LOG_SENSOR("  ", "Timestamp:", this->timestamp_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Version:", this->version_text_sensor_);
#endif
#ifdef USE_BUTTON
  LOG_BUTTON("  ", "Factory Reset:", this->reset_button_);
  LOG_BUTTON("  ", "Restart:", this->restart_button_);
#endif
}

void TFLuna::setup() {
  uint8_t buf[3];

  if (!this->read_bytes(VERSION_REVISION_REGISTER, buf, sizeof(buf))) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  char version[11];
  snprintf(version, sizeof(version), "%d.%d.%d", buf[2], buf[1], buf[0]);
  ESP_LOGI(TAG, "Firmware: %s", version);

#ifdef USE_TEXT_SENSOR
  if (this->version_text_sensor_ != nullptr) {
    this->version_text_sensor_->publish_state(version);
  }
#endif

  if (!this->write_byte(MODE_REGISTER, MODE_TRIGGER)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->mark_failed();
    return;
  }

  if (!this->write_byte(SAVE_REGISTER, 1)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->mark_failed();
    return;
  }
}

[[nodiscard]] bool TFLuna::read_data_() {
  uint8_t buf[8];
  if (!this->read_bytes(DISTANCE_LOW_REGISTER, buf, sizeof(buf))) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    return false;
  }
  // Layout:
  // buf[0..1] = distance (LE), buf[2..3] = signal (LE),
  // buf[4..5] = temperature (LE), buf[6..7] = timestamp (LE)
  uint16_t distance = encode_uint16(buf[1], buf[0]);
  uint16_t signal_strength = encode_uint16(buf[3], buf[2]);
  uint16_t temperature_raw = encode_uint16(buf[5], buf[4]);
  uint16_t timestamp = encode_uint16(buf[7], buf[6]);

  if (timestamp == previous_timestamp_) {
    return false;
  }

#ifdef USE_SENSOR
  if (this->timestamp_sensor_ != nullptr) {
    this->timestamp_sensor_->publish_state(timestamp);
  }
  if (this->distance_sensor_ != nullptr) {
    this->distance_sensor_->publish_state(distance);
  }

  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temperature_raw / (float) 100);
  }

  if (this->signal_strength_sensor_ != nullptr) {
    this->signal_strength_sensor_->publish_state(signal_strength);
  }
#endif
  this->status_clear_warning();
  this->previous_timestamp_ = timestamp;
  return true;
}

void TFLuna::read_data_timeout_() {
  if (this->read_data_()) {
    this->attempt_ = 0;
  } else {
    if (this->attempt_ < 5) {
      this->attempt_++;
      this->set_timeout("read_data_timeout_", 5, [this]() { this->read_data_timeout_(); });
    } else {
      this->status_set_warning("Hung device, restarting...");
      this->restart();
    }
  }
}

void TFLuna::update() {
  if (!this->write_byte(TRIGGER_ONESHOT_REGISTER, 0x01)) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  this->read_data_timeout_();
}

void TFLuna::factory_reset() {
  if (!this->write_byte(RESTORE_FACTORY_DEFAULTS_REGISTER, 1)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  this->status_set_warning("Factory reset issued; waiting for device to become ready");
}

void TFLuna::restart() {
  if (!this->write_byte(SHUTDOWN_REBOOT_REGISTER, 0x02)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }
}

}  // namespace esphome::tfluna
