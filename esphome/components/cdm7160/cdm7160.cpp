#include "cdm7160.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cdm7160 {

static const uint8_t CDM7160_DATA_CO2_LO = 0x03;
static const uint8_t CDM7160_DATA_CO2_HI = 0x04;
static const uint8_t CDM7160_HPA_PRESSURE = 0x09;
static const uint8_t CDM7160_HIT_ALTITUDE = 0x0A;

static const char *const TAG = "cdm7160";

void CDM7160Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CDM7160...");
  if (!this->write_altitude_()) {
    ESP_LOGE(TAG, "Communication with MLX90614 failed!");
    this->mark_failed();
    return;
  }
}

bool CDM7160Component::write_altitude_() {
  if (std::isnan(this->altitude_))
    return true;
  uint8_t value = (uint8_t) (this->altitude_ / 10);
  if (!this->write_bytes(CDM7160_HIT_ALTITUDE, &value, 1)) {
    return false;
  }
  delay(10);
  if (!this->write_bytes(CDM7160_HIT_ALTITUDE, &value, 1)) {
    return false;
  }
  delay(10);
  return true;
}


void CDM7160Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CDM7160:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with CDM7160 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "co2", this->co2_sensor_);
}

float CDM7160Component::get_setup_priority() const { return setup_priority::DATA; }

void CDM7160Component::update() {
  uint8_t raw_co2[2];
  if (this->read_register(CDM7160_DATA_CO2_LO, raw_co2, 2, false) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  int co2 = encode_uint16(raw_co2[1], raw_co2[0]);

  ESP_LOGD(TAG, "Got CO2=%dppm", co2);

  if (this->co2_sensor_ != nullptr && !std::isnan(co2))
    this->co2_sensor_->publish_state(co2);
  this->status_clear_warning();
}

}  // namespace cdm7160
}  // namespace esphome
