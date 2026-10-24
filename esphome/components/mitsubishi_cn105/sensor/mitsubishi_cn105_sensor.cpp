#include "mitsubishi_cn105_sensor.h"

#include "esphome/core/log.h"

namespace esphome::mitsubishi_cn105 {

ESPHOME_LOG_TAG(TAG, "mitsubishi_cn105.sensor");

void MitsubishiCN105Sensor::setup() {
  this->parent_->add_on_status_callback([this]() { this->publish_reading_(); });
  if (this->parent_->is_status_initialized()) {
    this->publish_reading_();
  }
}

void MitsubishiCN105Sensor::dump_config() { LOG_SENSOR("", "Mitsubishi CN105 Sensor", this); }

float MitsubishiCN105Sensor::reading_() const {
  const auto &status = this->parent_->status();
  switch (this->kind_) {
    case MitsubishiCN105SensorKind::OUTSIDE_AIR_TEMPERATURE:
      return status.outside_air_temperature;
    case MitsubishiCN105SensorKind::INPUT_POWER:
      return status.input_power;
    case MitsubishiCN105SensorKind::ENERGY:
      return status.energy;
  }
  return NAN;
}

void MitsubishiCN105Sensor::publish_reading_() {
  const float value = this->reading_();
  // Keep the last value until the first real sample arrives. A later NAN (the outdoor
  // unit stopped reporting) clears the sensor so Home Assistant does not keep a stale value.
  if (std::isnan(value) && !this->has_state()) {
    return;
  }
  this->publish_state(value);
}

}  // namespace esphome::mitsubishi_cn105
