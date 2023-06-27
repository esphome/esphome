#include "esphome/core/log.h"
#include "ufire_ec.h"

namespace esphome {
namespace ufire_ec {

static const char *const TAG = "ufire_ec";

void UFireECComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up uFire_ec...");

  uint8_t version;
  if (!this->read_byte(REGISTER_VERSION, &version) && version != 0xFF) {
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Found ufire_ec board version 0x%02X", version);

  // Write option for temperature adjustments
  uint8_t config;
  this->read_byte(REGISTER_CONFIG, &config);
  if (this->temperature_sensor_ == nullptr && this->temperature_sensor_external_ == nullptr) {
    config &= ~CONFIG_TEMP_COMPENSATION;
  } else {
    config |= CONFIG_TEMP_COMPENSATION;
  }
  this->write_byte(REGISTER_CONFIG, config);

  // Update temperature compensation
  this->set_compensation_(this->temperature_compensation_);
  this->set_coefficient_(this->temperature_coefficient_);
}

void UFireECComponent::update() {
  int wait = 0;

  if (this->temperature_sensor_ != nullptr) {
    this->write_byte(REGISTER_TASK, COMMAND_MEASURE_TEMP);
    wait += 750;
  } else if (this->temperature_sensor_external_ != nullptr) {
    this->set_temperature_(this->temperature_sensor_external_->state);
  }

  if (this->ec_sensor_ != nullptr) {
    this->write_byte(REGISTER_TASK, COMMAND_MEASURE_EC);
    wait += 750;
  }

  if (wait > 0) {
    this->set_timeout("data", wait, [this]() { this->update_internal_(); });
  }
}

void UFireECComponent::update_internal_() {
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(this->measure_temperature_());
  if (this->ec_sensor_ != nullptr)
    this->ec_sensor_->publish_state(this->measure_ms_());
}

float UFireECComponent::measure_temperature_() { return this->read_data_(REGISTER_TEMP); }

float UFireECComponent::measure_ms_() { return this->read_data_(REGISTER_MS); }

void UFireECComponent::set_solution_(float solution, float temperature) {
  solution /= (1 - (this->temperature_coefficient_ * (temperature - 25)));
  this->write_data_(REGISTER_SOLUTION, solution);
}

void UFireECComponent::set_compensation_(float temperature) { this->write_data_(REGISTER_COMPENSATION, temperature); }

void UFireECComponent::set_coefficient_(float coefficient) { this->write_data_(REGISTER_COEFFICENT, coefficient); }

void UFireECComponent::set_temperature_(float temperature) { this->write_data_(REGISTER_TEMP, temperature); }

void UFireECComponent::calibrate_probe(float solution, float temperature) {
  this->set_solution_(solution, temperature);
  this->write_byte(REGISTER_TASK, COMMAND_CALIBRATE_PROBE);
}

void UFireECComponent::reset_board() { this->write_data_(REGISTER_CALIBRATE_OFFSET, NAN); }

float UFireECComponent::read_data_(uint8_t reg) {
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

void UFireECComponent::write_data_(uint8_t reg, float data) {
  uint8_t temp[4];

  memcpy(temp, &data, sizeof(data));
  this->write_bytes(reg, temp, 4);
  delay(10);
}

void UFireECComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "uFire-EC");
  LOG_I2C_DEVICE(this)
  LOG_UPDATE_INTERVAL(this)
  LOG_SENSOR("  ", "EC Sensor", this->ec_sensor_)
  LOG_SENSOR("  ", "Temperature Sensor", this->temperature_sensor_)
  LOG_SENSOR("  ", "Temperature Sensor external", this->temperature_sensor_external_)
  ESP_LOGCONFIG(TAG, "  Temperature Compensation: %f", this->temperature_compensation_);
  ESP_LOGCONFIG(TAG, "  Temperature Coefficient: %f", this->temperature_coefficient_);
}

}  // namespace ufire_ec
}  // namespace esphome
