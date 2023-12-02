#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace tuya {

class TuyaSensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_sensor_id(uint8_t sensor_id) { this->sensor_id_ = sensor_id; }

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }

 protected:
  Tuya *parent_;
  uint8_t sensor_id_{0};
};

}  // namespace tuya
}  // namespace esphome
