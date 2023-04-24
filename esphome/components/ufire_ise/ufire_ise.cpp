#include "esphome/core/log.h"
#include "ufire_ise.h"

#include <cmath>

namespace esphome {
namespace ufire_ise {

static const char *const TAG = "ufire_ise";

void UFireISEComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up uFire_ise...");

  uint8_t version;
  if (!this->read_byte(REGISTER_VERSION, &version) && version != 0xFF) {
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Found uFire_ise board version 0x%02X", version);

  // Write option for temperature adjustments
  uint8_t config;
  this->read_byte(REGISTER_CONFIG, &config);
  if (this->temperature_sensor_ == nullptr && this->temperature_sensor_external_ == nullptr) {
    config &= ~CONFIG_TEMP_COMPENSATION;
  } else {
    config |= CONFIG_TEMP_COMPENSATION;
  }
  this->write_byte(REGISTER_CONFIG, config);
}

void UFireISEComponent::update() {
  int wait = 0;
  if (this->temperature_sensor_ != nullptr) {
    this->write_byte(REGISTER_TASK, COMMAND_MEASURE_TEMP);
    wait += 750;
  }
  if (this->ph_sensor_ != nullptr) {
    this->write_byte(REGISTER_TASK, COMMAND_MEASURE_MV);
    wait += 750;
  }

  // Wait until measurement are taken
  this->set_timeout("data", wait, [this]() { this->update_internal_(); });
}

void UFireISEComponent::update_internal_() {
  float temperature = 0;

  // Read temperature internal and populate it
  if (this->temperature_sensor_ != nullptr) {
    temperature = this->measure_temperature_();
    this->temperature_sensor_->publish_state(temperature);
  }
  // Get temperature from external only for adjustments
  else if (this->temperature_sensor_external_ != nullptr) {
    temperature = this->temperature_sensor_external_->state;
  }

  if (this->ph_sensor_ != nullptr) {
    this->ph_sensor_->publish_state(this->measure_ph_(temperature));
  }
}

float UFireISEComponent::measure_temperature_() { return this->read_data_(REGISTER_TEMP); }

float UFireISEComponent::measure_mv_() { return this->read_data_(REGISTER_MV); }

float UFireISEComponent::measure_ph_(float temperature) {
  float mv, ph;

  mv = this->measure_mv_();
  if (mv == -1)
    return -1;

  ph = fabs(7.0 - (mv / PROBE_MV_TO_PH));

  // Determine the temperature correction
  float distance_from_7 = std::abs(7 - roundf(ph));
  float distance_from_25 = std::floor(std::abs(25 - roundf(temperature)) / 10);
  float temp_multiplier = (distance_from_25 * distance_from_7) * PROBE_TMP_CORRECTION;
  if ((ph >= 8.0) && (temperature >= 35))
    temp_multiplier *= -1;
  if ((ph <= 6.0) && (temperature <= 15))
    temp_multiplier *= -1;

  ph += temp_multiplier;
  if ((ph <= 0.0) || (ph > 14.0))
    ph = -1;
  if (std::isinf(ph))
    ph = -1;
  if (std::isnan(ph))
    ph = -1;

  return ph;
}

void UFireISEComponent::set_solution_(float solution) {
  solution = (7 - solution) * PROBE_MV_TO_PH;
  this->write_data_(REGISTER_SOLUTION, solution);
}

void UFireISEComponent::calibrate_probe_low(float solution) {
  this->set_solution_(solution);
  this->write_byte(REGISTER_TASK, COMMAND_CALIBRATE_LOW);
}

void UFireISEComponent::calibrate_probe_high(float solution) {
  this->set_solution_(solution);
  this->write_byte(REGISTER_TASK, COMMAND_CALIBRATE_HIGH);
}

void UFireISEComponent::reset_board() {
  this->write_data_(REGISTER_REFHIGH, NAN);
  this->write_data_(REGISTER_REFLOW, NAN);
  this->write_data_(REGISTER_READHIGH, NAN);
  this->write_data_(REGISTER_READLOW, NAN);
}

float UFireISEComponent::read_data_(uint8_t reg) {
  float f;
  uint8_t temp[4];

  this->write(&reg, 1);
  delay(10);

  for (uint8_t i = 0; i < 4; i++) {
    this->read_bytes_raw(temp + i, 1);
  }
  memcpy(&f, temp, sizeof(f));

  return f;
}

void UFireISEComponent::write_data_(uint8_t reg, float data) {
  uint8_t temp[4];

  memcpy(temp, &data, sizeof(data));
  this->write_bytes(reg, temp, 4);
  delay(10);
}

void UFireISEComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "uFire-ISE");
  LOG_I2C_DEVICE(this)
  LOG_UPDATE_INTERVAL(this)
  LOG_SENSOR("  ", "PH Sensor", this->ph_sensor_)
  LOG_SENSOR("  ", "Temperature Sensor", this->temperature_sensor_)
  LOG_SENSOR("  ", "Temperature Sensor external", this->temperature_sensor_external_)
}

}  // namespace ufire_ise
}  // namespace esphome
