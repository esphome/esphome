#include "tmp102.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tmp102 {

static const char *const TAG = "tmp102";

static const uint8_t TMP102_ADDRESS = 0x48;
static const uint8_t TMP102_REGISTER_TEMPERATURE = 0x00;
static const uint8_t TMP102_REGISTER_CONFIGURATION = 0x01;
static const uint8_t TMP102_REGISTER_LOW_LIMIT = 0x02;
static const uint8_t TMP102_REGISTER_HIGH_LIMIT = 0x03;

static const float TMP102_CONVERSION_FACTOR = 0.0625;

void TMP102Component::setup() { ESP_LOGCONFIG(TAG, "Setting up TMP102..."); }

void TMP102Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TMP102:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with TMP102 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this);
}

void TMP102Component::update() {
  if (this->write(&TMP102_REGISTER_TEMPERATURE, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  this->set_timeout("read_temp", 50, [this]() {
    int16_t raw_temperature;
    if (this->read(reinterpret_cast<uint8_t *>(&raw_temperature), 2) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
    raw_temperature = i2c::i2ctohs(raw_temperature);
    raw_temperature = raw_temperature >> 4;
    float temperature = raw_temperature * TMP102_CONVERSION_FACTOR;
    ESP_LOGD(TAG, "Got Temperature=%.1f°C", temperature);

    this->publish_state(temperature);
    this->status_clear_warning();
  });
}

float TMP102Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace tmp102
}  // namespace esphome
