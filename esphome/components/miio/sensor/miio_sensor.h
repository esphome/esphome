#pragma once

#include "esphome/core/component.h"
#include "esphome/components/miio/miio.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace miio {

class MiioSensor : public sensor::Sensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_sensor_id(uint8_t cluster, uint8_t key) { this->sensor_id_ = std::make_tuple(cluster, key); }
  void set_miio_parent(Miio *parent) { this->parent_ = parent; }

 protected:
  Miio *parent_;
  prop_t sensor_id_;
};

}  // namespace miio
}  // namespace esphome
