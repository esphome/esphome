#include "tfluna.h"
#include <cstddef>
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tfluna {

// see https://files.waveshare.com/upload/a/ac/SJ-PM-TF-Luna_A05_Product_Manual.pdf
static const uint8_t SAVE_REGISTER = 0x20;
static const uint8_t VERSION_REVISION_REGISTER = 0x0A;
static const uint8_t VERSION_MINOR_REGISTER = 0x0B;
static const uint8_t VERSION_MAJOR_REGISTER = 0x0C;
static const uint8_t AMP_LOW_REGISTER = 0x02;
static const uint8_t AMP_HIGH_REGISTER = 0x03;
static const uint8_t TEMPERATURE_LOW_REGISTER = 0x04;
static const uint8_t TEMPERATURE_HIGH_REGISTER = 0x05;
static const uint8_t TIMESTAMP_LOW_REGISTER = 0x06;
static const uint8_t TIMESTAMP_HIGH_REGISTER = 0x07;
static const uint8_t DISTANCE_LOW_REGISTER = 0x00;
static const uint8_t DISTANCE_HIGH_REGISTER = 0x01;
static const uint8_t RESTORE_FACTORY_DEFAULTS_REGISTER = 0x29;
static const uint8_t SHUTDOWN_REBOOT_REGISTER = 0x21;
static const uint8_t MODE_REGISTER = 0x23;
static const uint8_t MODE_CONTINUOUS = 0x00;
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
  uint8_t major;
  if (!this->read_byte(VERSION_MAJOR_REGISTER, &major)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->mark_failed();
    return;
  }
  uint8_t minor;
  if (!this->read_byte(VERSION_MINOR_REGISTER, &minor)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->mark_failed();
    return;
  }
  uint8_t revision;
  if (!this->read_byte(VERSION_REVISION_REGISTER, &revision)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->mark_failed();
    return;
  }
  auto version = str_snprintf("%d.%d.%d", major, minor, revision);
  ESP_LOGI(TAG, "Firmware: %s", version.c_str());

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

void TFLuna::update() {
  auto read_timestamp = [this](uint16_t *timestamp) -> bool {
    uint8_t timestamp_low;
    if (!this->read_byte(TIMESTAMP_LOW_REGISTER, &timestamp_low)) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return false;
    }
    uint8_t timestamp_high;
    if (!this->read_byte(TIMESTAMP_HIGH_REGISTER, &timestamp_high)) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return false;
    }
    *timestamp = timestamp_low + timestamp_high * 256;
    return true;
  };

  uint16_t previous_timestamp;
  if (!read_timestamp(&previous_timestamp)) {
    return;
  }

  if (!this->write_byte(TRIGGER_ONESHOT_REGISTER, 0x01)) {
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    return;
  }

  uint16_t timestamp = previous_timestamp;
  bool updated = false;
  for (uint8_t attempt = 0; attempt < 5; attempt++) {
    delay(5);
    if (!read_timestamp(&timestamp)) {
      return;
    }
    if (timestamp != previous_timestamp) {
      updated = true;
      break;
    }
  }

  if (!updated) {
    this->status_set_warning("Hung device, restarting...");
    this->restart();
    return;
  }

#ifdef USE_SENSOR
  if (this->timestamp_sensor_ != nullptr) {
    this->timestamp_sensor_->publish_state(timestamp);
  }
  if (this->distance_sensor_ != nullptr) {
    uint8_t distance_low;
    if (!this->read_byte(DISTANCE_LOW_REGISTER, &distance_low)) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return;
    }
    uint8_t distance_high;
    if (!this->read_byte(DISTANCE_HIGH_REGISTER, &distance_high)) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return;
    }
    uint16_t distance = distance_low + distance_high * 256;

    this->distance_sensor_->publish_state(distance);
  }

  if (this->temperature_sensor_ != nullptr) {
    uint8_t temperature_low;
    if (!this->read_byte(TEMPERATURE_LOW_REGISTER, &temperature_low)) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return;
    }
    uint8_t temperature_high;
    if (!this->read_byte(TEMPERATURE_HIGH_REGISTER, &temperature_high)) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return;
    }
    float temperature = (temperature_low + temperature_high * 256) / (float) 100;

    this->temperature_sensor_->publish_state(temperature);
  }

  if (this->signal_strength_sensor_ != nullptr) {
    uint8_t signal_strength_low;
    if (!this->read_byte(AMP_LOW_REGISTER, &signal_strength_low)) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return;
    }
    uint8_t signal_strength_high;
    if (!this->read_byte(AMP_HIGH_REGISTER, &signal_strength_high)) {
      this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
      return;
    }
    uint16_t signal_strength = signal_strength_low + signal_strength_high * 256;

    this->signal_strength_sensor_->publish_state(signal_strength);
  }
#endif
  this->status_clear_warning();
}

void TFLuna::factory_reset() {
  if (!this->write_byte(RESTORE_FACTORY_DEFAULTS_REGISTER, 1)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  ESP_LOGW(TAG, "Factory reset issued; device may be temporarily unavailable while applying defaults");
  this->status_set_warning("Factory reset issued; waiting for device to become ready");
}

void TFLuna::restart() {
  if (!this->write_byte(SHUTDOWN_REBOOT_REGISTER, 0x02)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }
}

}  // namespace tfluna
}  // namespace esphome
