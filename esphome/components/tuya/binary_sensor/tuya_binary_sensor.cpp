#include "esphome/core/log.h"
#include "tuya_binary_sensor.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.binary_sensor";

void TuyaBinarySensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](const TuyaDatapoint &datapoint) {
    ESP_LOGV(TAG, "MCU reported binary sensor %u is: %s", datapoint.id, ONOFF(datapoint.value_bool));
    this->publish_state(datapoint.value_bool);
  });
}

void TuyaBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Binary Sensor:");
  ESP_LOGCONFIG(TAG, "  Binary Sensor has datapoint ID %u", this->sensor_id_);
}

}  // namespace tuya
}  // namespace esphome
