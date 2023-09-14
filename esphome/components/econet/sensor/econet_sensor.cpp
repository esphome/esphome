#include "esphome/core/log.h"
#include "econet_sensor.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.sensor";

void EconetSensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](const EconetDatapoint &datapoint) {
    if (datapoint.type == EconetDatapointType::FLOAT) {
      ESP_LOGV(TAG, "MCU reported sensor %s is: %f", this->sensor_id_.c_str(), datapoint.value_float);
      this->publish_state(datapoint.value_float);
    } else if (datapoint.type == EconetDatapointType::ENUM_TEXT) {
      ESP_LOGV(TAG, "MCU reported sensor %s is: %u", this->sensor_id_.c_str(), datapoint.value_enum);
      this->publish_state(datapoint.value_enum);
    }
  });
}

void EconetSensor::dump_config() {
  LOG_SENSOR("", "Econet Sensor", this);
  ESP_LOGCONFIG(TAG, "  Sensor has datapoint ID %s", this->sensor_id_.c_str());
}

}  // namespace econet
}  // namespace esphome
