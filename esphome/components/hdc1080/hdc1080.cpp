#include "hdc1080.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace hdc1080 {

static const char *const TAG = "hdc1080";

static const uint8_t HDC1080_CMD_CONFIGURATION = 0x02;
static const uint8_t HDC1080_CMD_TEMPERATURE = 0x00;
static const uint8_t HDC1080_CMD_HUMIDITY = 0x01;

// Retry configuration to reduce log spam
static const uint8_t HDC1080_MAX_RETRIES = 3;
static const uint8_t HDC1080_RETRY_DELAY = 20;

void HDC1080Component::setup() {
  const uint8_t config[2] = {0x00, 0x00};  // resolution 14bit for both humidity and temperature

  // if configuration fails - there is a problem
  if (this->write_register(HDC1080_CMD_CONFIGURATION, config, 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to configure HDC1080");
    this->status_set_warning();
    return;
  }
}

void HDC1080Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HDC1080:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
}

void HDC1080Component::update() {
  // regardless of what sensor/s are defined in yaml configuration
  // the hdc1080 setup configuration used, requires both temperature and humidity to be read

  this->status_clear_warning();

  uint8_t retry_count = 0;
  i2c::ErrorCode error;

  // Retry temperature reading
  do {
    error = this->write(&HDC1080_CMD_TEMPERATURE, 1);
    if (error != i2c::ERROR_OK) {
      retry_count++;
      if (retry_count < HDC1080_MAX_RETRIES) {
        ESP_LOGD(TAG, "Temperature read failed, retry %d/%d", retry_count, HDC1080_MAX_RETRIES);
        delay(HDC1080_RETRY_DELAY);
      }
    }
  } while (error != i2c::ERROR_OK && retry_count < HDC1080_MAX_RETRIES);

  if (error != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read temperature after %d attempts", HDC1080_MAX_RETRIES);
    this->status_set_warning();
    return;
  }

  this->set_timeout(20, [this]() {
    uint16_t raw_temperature;
    if (this->read(reinterpret_cast<uint8_t *>(&raw_temperature), 2) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }

    if (this->temperature_ != nullptr) {
      raw_temperature = i2c::i2ctohs(raw_temperature);
      float temperature = raw_temperature * 0.0025177f - 40.0f;  // raw * 2^-16 * 165 - 40
      this->temperature_->publish_state(temperature);
    }

    // Read humidity with retry
    uint8_t humidity_retry_count = 0;
    i2c::ErrorCode humidity_error;

    do {
      humidity_error = this->write(&HDC1080_CMD_HUMIDITY, 1);
      if (humidity_error != i2c::ERROR_OK) {
        humidity_retry_count++;
        if (humidity_retry_count < HDC1080_MAX_RETRIES) {
          ESP_LOGD(TAG, "Humidity write failed, retry %d/%d", humidity_retry_count, HDC1080_MAX_RETRIES);
          delay(HDC1080_RETRY_DELAY);
        }
      }
    } while (humidity_error != i2c::ERROR_OK && humidity_retry_count < HDC1080_MAX_RETRIES);

    if (humidity_error != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Failed to read humidity after %d attempts", HDC1080_MAX_RETRIES);
      this->status_set_warning();
      return;
    }

    this->set_timeout(20, [this]() {
      uint16_t raw_humidity;
      if (this->read(reinterpret_cast<uint8_t *>(&raw_humidity), 2) != i2c::ERROR_OK) {
        this->status_set_warning();
        return;
      }

      if (this->humidity_ != nullptr) {
        raw_humidity = i2c::i2ctohs(raw_humidity);
        float humidity = raw_humidity * 0.001525879f;  // raw * 2^-16 * 100
        this->humidity_->publish_state(humidity);
      }
    });
  });
}

}  // namespace hdc1080
}  // namespace esphome
