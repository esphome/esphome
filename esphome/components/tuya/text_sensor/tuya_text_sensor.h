#pragma once

#include "esphome/components/tuya/tuya.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::tuya {

class TuyaTextSensor final : public text_sensor::TextSensor {
 public:
  // Called from generated code before setup, after the datapoint setters
  void register_listeners();
  // Called from the hub's dump_config()
  void dump_child_config();
  void set_sensor_id(uint8_t sensor_id) { this->sensor_id_ = sensor_id; }

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }

 protected:
  Tuya *parent_;
  uint8_t sensor_id_{0};
};

}  // namespace esphome::tuya
