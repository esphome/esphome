#include "binary_sensor_map.h"
#include "esphome/core/log.h"

namespace esphome {
namespace binary_sensor_map {

static const char *TAG = "sensor.binary_sensor_map";

void BinarySensorMap::dump_config() { LOG_SENSOR("  ", "binary_sensor_map", this); }

void BinarySensorMap::loop() {
  switch (this->sensor_type_) {
    case BINARY_SENSOR_MAP_TYPE_GROUP:
      this->process_group_();
      break;
  }
}

void BinarySensorMap::process_group_() {
  float total_current_value = 0.0;
  uint8_t num_active_sensors = 0;
  bool any_active = false;
  uint64_t mask = 0x00;
  // check all binary_sensors for its state. when active add its value to total_current_value.
  // create a bitmask for the binary_sensor status on all channels
  for (size_t i = 0; i < this->sensors_.size(); i++) {
    auto *bs = this->sensors_[i];
    if (bs->binary_sensor->state) {
      any_active = true;
      num_active_sensors++;
      total_current_value += bs->sensor_value;
      mask |= 1 << i;
    }
  }
  // check if the sensor map was touched
  if (any_active) {
    // did the bit_mask change or is it a new sensor touch
    if ((last_mask_ != mask) || !this->last_any_active_) {
      this->last_any_active_ = true;
      float publish_value = total_current_value / num_active_sensors;
      ESP_LOGD(TAG, "%s - Publishing %.2f", this->name_.c_str(), publish_value);
      this->publish_state(publish_value);
      this->last_mask_ = mask;
    }
  } else {
    // is this a new sensor release
    if (this->last_any_active_) {
      this->last_any_active_ = false;
      ESP_LOGD(TAG, "%s - No binary sensor active, publishing NAN", this->name_.c_str());
      this->publish_state(NAN);
    }
  }
}

void BinarySensorMap::add_sensor(binary_sensor::BinarySensor *sensor, float value) {
  BinarySensorMapChannel *sensor_channel = new BinarySensorMapChannel;
  sensor_channel->binary_sensor = sensor;
  sensor_channel->sensor_value = value;
  this->sensors_.push_back(sensor_channel);
}

void BinarySensorMap::set_sensor_type(BinarySensorMapType sensor_type) { this->sensor_type_ = sensor_type; }

}  // namespace binary_sensor_map
}  // namespace esphome