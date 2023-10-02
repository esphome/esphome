#include "esphome/core/log.h"
#include "econet_text_sensor.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.text_sensor";

void EconetTextSensor::setup() {
  this->parent_->register_listener(this->sensor_id_, this->listen_only_, [this](const EconetDatapoint &datapoint) {
    ESP_LOGD(TAG, "MCU reported text sensor %s is: %s", this->sensor_id_.c_str(), datapoint.value_string.c_str());
    this->publish_state(datapoint.value_string);
  });
}

void EconetTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet Text Sensor:");
  ESP_LOGCONFIG(TAG, "  Text Sensor has datapoint ID %s", this->sensor_id_.c_str());
}

}  // namespace econet
}  // namespace esphome
