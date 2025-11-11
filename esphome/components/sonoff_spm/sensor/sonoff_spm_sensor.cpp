#include "sonoff_spm_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sonoff_spm {

static const char *const TAG = "sonoff_spm.sensor";

void SonoffSPMSensor::setup() { ESP_LOGCONFIG(TAG, "Setting up Sonoff SPM Sensor..."); }

void SonoffSPMSensor::loop() {
  // Update sensors every second
  uint32_t now = millis();
  if (now - this->last_update_ > 1000) {
    this->update_sensors_();
    this->last_update_ = now;
  }
}

void SonoffSPMSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Sonoff SPM Sensor:");
  ESP_LOGCONFIG(TAG, "  Relay ID: %d", this->relay_id_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Apparent Power", this->apparent_power_sensor_);
  LOG_SENSOR("  ", "Reactive Power", this->reactive_power_sensor_);
  LOG_SENSOR("  ", "Power Factor", this->power_factor_sensor_);
  LOG_SENSOR("  ", "Energy", this->energy_sensor_);
}

void SonoffSPMSensor::update_sensors_() {
  if (this->parent_ == nullptr) {
    return;
  }

  const ChannelData *data = this->parent_->get_channel_data(this->relay_id_);
  if (data == nullptr) {
    return;
  }

  // Only update if relay is on
  if (!data->relay_state) {
    return;
  }

  if (this->voltage_sensor_ != nullptr && data->voltage > 0) {
    this->voltage_sensor_->publish_state(data->voltage);
  }

  if (this->current_sensor_ != nullptr && data->current > 0) {
    this->current_sensor_->publish_state(data->current);
  }

  if (this->power_sensor_ != nullptr && data->active_power > 0) {
    this->power_sensor_->publish_state(data->active_power);
  }

  if (this->apparent_power_sensor_ != nullptr && data->apparent_power > 0) {
    this->apparent_power_sensor_->publish_state(data->apparent_power);
  }

  if (this->reactive_power_sensor_ != nullptr && data->reactive_power > 0) {
    this->reactive_power_sensor_->publish_state(data->reactive_power);
  }

  if (this->power_factor_sensor_ != nullptr && data->power_factor > 0) {
    this->power_factor_sensor_->publish_state(data->power_factor);
  }

  if (this->energy_sensor_ != nullptr && data->energy_total > 0) {
    this->energy_sensor_->publish_state(data->energy_total);
  }
}

}  // namespace sonoff_spm
}  // namespace esphome
