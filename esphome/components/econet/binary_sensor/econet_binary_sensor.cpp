#include "esphome/core/log.h"
#include "econet_binary_sensor.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.binary_sensor";

void EconetBinarySensor::setup() {
  this->parent_->register_listener(this->sensor_id_, this->request_mod_, [this](const EconetDatapoint &datapoint) {
    ESP_LOGV(TAG, "MCU reported binary sensor %s is: %s", this->sensor_id_.c_str(), ONOFF(datapoint.value_enum));
    this->publish_state(datapoint.value_enum);
  });
}

void EconetBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Econet Binary Sensor:");
  ESP_LOGCONFIG(TAG, "  Binary Sensor has datapoint ID %s", this->sensor_id_.c_str());
}

}  // namespace econet
}  // namespace esphome
