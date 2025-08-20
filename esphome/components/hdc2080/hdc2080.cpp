#include "hdc2080.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hdc2080 {

static const char *const TAG = "hdc2080";

static const uint8_t HDC2080_CMD_CONFIGURATION = 0x0E;
static const uint8_t HDC2080_CMD_MEASUREMENT_CONFIGURATION = 0x0F;
static const uint8_t HDC2080_CMD_TEMPERATURE = 0x00;

void HDC2080Component::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  const uint8_t data[2] = {
      0b00000000,  // automatic reading mode, 1 sample for minute
      0b00000001   // resolution 14bit for both humidity and temperature and start measurement
  };

  if (!this->write_bytes(HDC2080_CMD_CONFIGURATION, data, 2)) {
    // as instruction is same as powerup defaults (for now), interpret as warning if this fails
    ESP_LOGW(TAG, "HDC2080 initial config instruction error");
    this->status_set_warning();
    return;
  }
}

void HDC2080Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HDC2080:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void HDC2080Component::update() {
  if (this->temperature_sensor_ != nullptr)
    this->state_ = READ_TEMPERATURE;
  else if (this->humidity_sensor_ != nullptr)
    this->state_ = READ_HUMIDITY;
  else
    return;
  this->enable_loop();
}

void HDC2080Component::loop() {
  if (this->state_ == READ_TEMPERATURE) {
    this->read_temperature_();
  } else if (this->state_ == READ_HUMIDITY) {
    this->read_humidity_();
  }
  this->disable_loop();
}

void HDC2080Component::read_temperature_() {
  if (this->write(&HDC2080_CMD_TEMPERATURE, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  this->set_timeout(20, [this]() {
    uint16_t raw_temp;
    if (this->read(reinterpret_cast<uint8_t *>(&raw_temp), 2) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
    float temp = raw_temp * 0.0025177f - 40.5f;  // raw * 2^-16 * 165 - 40.5
    this->temperature_sensor_->publish_state(temp);

    this->status_clear_warning();

    if (this->humidity_sensor_ != nullptr) {
      this->state_ = READ_HUMIDITY;
      this->enable_loop();
    } else {
      this->state_ = IDLE;
    }
  });
}

void HDC2080Component::read_humidity_() {
  if (this->write(&HDC2080_CMD_HUMIDITY, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  this->set_timeout(20, [this]() {
    uint16_t raw_humidity;
    if (this->read(reinterpret_cast<uint8_t *>(&raw_humidity), 2) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
    raw_humidity = i2c::i2ctohs(raw_humidity);
    float humidity = raw_humidity * 0.001525879f;  // raw * 2^-16 * 100
    this->humidity_sensor_->publish_state(humidity);

    this->status_clear_warning();

    this->state_ = IDLE;
  });
}

}  // namespace hdc2080
}  // namespace esphome
