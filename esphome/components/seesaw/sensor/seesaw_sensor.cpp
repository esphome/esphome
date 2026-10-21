#include "seesaw_sensor.h"
#include "esphome/core/log.h"

namespace esphome::seesaw {

ESPHOME_LOG_TAG(TAG, "seesaw.sensor");

void SeesawSensor::setup() { seesaw_hub_failed(this, this->parent_); }

void SeesawSensor::update() {
  bool ok;
  switch (this->type_) {
    case SeesawSensorType::SEESAW_SENSOR_TYPE_ADC: {
      uint16_t value;
      if ((ok = this->parent_->analog_read(this->channel_, value)))
        this->publish_state(value);
      break;
    }
    case SeesawSensorType::SEESAW_SENSOR_TYPE_TOUCH: {
      uint16_t value;
      if ((ok = this->parent_->get_touch_value(this->channel_, value)))
        this->publish_state(value);
      break;
    }
    default: {
      float value;
      if ((ok = this->parent_->get_temperature(value)))
        this->publish_state(value);
      break;
    }
  }
  this->status_comm_result(ok);
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
