#include "esphome/core/log.h"
#include "ufire_ec.h"

namespace esphome {
namespace ufire_ec {

static const char *TAG = "ufire_ec";

void UFireECComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up uFire_ec...");

  uint8_t version;
  if (!this->read_byte(REGISTER_VERSION, &version, 10) && version != 0xFF) {
    this->mark_failed();
    this->status_set_error();
    return;
  }
  ESP_LOGI(TAG, "Found ufire_ec board version 0x%02X", version);

  // Write option for temperature adjustments
  uint8_t config;
  this->read_byte(REGISTER_CONFIG, &config, 10);
  if (this->temperature_sensor_ == nullptr && this->temperature_sensor_external_ == nullptr)
    config &= ~CONFIG_TEMP_COMPENSATION;
  else
    config |= CONFIG_TEMP_COMPENSATION;
  this->write_byte(REGISTER_CONFIG, config);

  // Update temperature compensation
  this->set_compensation_(this->temperature_compensation_);
}

void UFireECComponent::update() {
  int wait = 0;

  if (this->temperature_sensor_ != nullptr) {
    this->write_byte(REGISTER_TASK, COMMAND_MEASURE_TEMP);
    wait += 750;
  } else if (this->temperature_sensor_external_ != nullptr) {
    float temperature = this->temperature_sensor_external_->get_state();
    this->set_temperature_(temperature);
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

void UFireECComponent::set_solution_(float solution) {
  uint8_t temp[4];

  solution /= (1 - (PROBE_COEFFICIENT * (this->temperature_compensation_ - 25)));
  memcpy(temp, &solution, sizeof(solution));
  this->write_bytes(REGISTER_SOLUTION, temp, 4);
  delay(10);
}

void UFireECComponent::set_compensation_(float temperature) {
  uint8_t temp[4];

  memcpy(temp, &temperature, sizeof(temperature));
  this->write_bytes(REGISTER_COMPENSATION, temp, 4);
  delay(10);
}

void UFireECComponent::set_temperature_(float temperature) {
  uint8_t temp[4];

  memcpy(temp, &temperature, sizeof(temperature));
  this->write_bytes(REGISTER_TEMP, temp, 4);
  delay(10);
}

void UFireECComponent::calibrate_probe(float solution) {
  this->set_solution_(solution);
  this->write_byte(REGISTER_TASK, COMMAND_CALIBRATE_PROBE);
}

float UFireECComponent::read_data_(uint8_t reg) {
  float f;
  uint8_t temp[4];

  this->write_bytes_raw(&reg, 1);
  delay(10);

  for (uint8_t i = 0; i < 4; i++) {
    this->read_bytes_raw(temp + i, 1);
  }
  memcpy(&f, temp, sizeof(f));

  return f;
}

void UFireECComponent::dump_config() {}

}  // namespace ufire_ec
}  // namespace esphome
