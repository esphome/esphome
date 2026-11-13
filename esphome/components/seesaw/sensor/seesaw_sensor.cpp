#include "seesaw_sensor.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw.sensor");

void SeesawSensor::setup() { seesaw_hub_failed(this, this->parent_); }

void SeesawSensor::update() {
  float value = 0;
  bool ok;
  switch (this->type_) {
    case SeesawSensorType::SEESAW_SENSOR_TYPE_ADC: {
      uint16_t raw = 0;
      ok = this->parent_->analog_read(this->channel_, raw);
      value = raw;
      break;
    }
    case SeesawSensorType::SEESAW_SENSOR_TYPE_TOUCH: {
      uint16_t raw = 0;
      ok = this->parent_->get_touch_value(this->channel_, raw);
      value = raw;
      break;
    }
    default:
      ok = this->parent_->get_temperature(value);
      break;
  }
  if (this->status_comm_result(ok))
    this->publish_state(value);
}

void SeesawSensor::dump_config() {
  LOG_SENSOR("", "Seesaw Sensor", this);
  switch (this->type_) {
    case SeesawSensorType::SEESAW_SENSOR_TYPE_ADC:
      ESP_LOGCONFIG(TAG, "  ADC pin: %u", this->channel_);
      break;
    case SeesawSensorType::SEESAW_SENSOR_TYPE_TOUCH:
      ESP_LOGCONFIG(TAG, "  Touch channel: %u", this->channel_);
      break;
    default:
      ESP_LOGCONFIG(TAG, "  Chip temperature");
      break;
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::seesaw
