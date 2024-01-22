#include "ina2xx_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ina2xx_i2c {

static const char *const TAG = "ina2xx_i2c";

void INA2XXI2C::setup() {
  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  INA2XX::setup();
}

void INA2XXI2C::dump_config() {
  ESP_LOGCONFIG(TAG, "INA2xx:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with INA2xx failed!");
    return;
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Shunt resistance = %f Ohm", shunt_resistance_ohm_);
  ESP_LOGCONFIG(TAG, "  Max current = %f A", this->max_current_a_);
  ESP_LOGCONFIG(TAG, "  ADCRANGE = %d (%s)", (uint8_t) this->adc_range_, this->adc_range_ ? "±40.96 mV" : "±163.84 mV");
  ESP_LOGCONFIG(TAG, "  Configured: CURRENT_LSB = %f", this->current_lsb_);
  ESP_LOGCONFIG(TAG, "  Configured: SHUNT_CAL = %d", this->shunt_cal_);

  LOG_SENSOR("  ", "Shunt Voltage", this->shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Bus Voltage", this->bus_voltage_sensor_);
  LOG_SENSOR("  ", "Die Temperature", this->die_temperature_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Energy", this->energy_sensor_);
  LOG_SENSOR("  ", "Charge", this->charge_sensor_);
}

bool INA2XXI2C::read_2xx(uint8_t a_register, uint8_t *data, size_t len) {
  auto ret = this->read_register(a_register, data, len, false);
  return ret == i2c::ERROR_OK;
}

bool INA2XXI2C::write_2xx(uint8_t a_register, const uint8_t *data, size_t len) {
  auto ret = this->write_register(a_register, data, len);
  return ret == i2c::ERROR_OK;
}

}  // namespace ina2xx_i2c
}  // namespace esphome
