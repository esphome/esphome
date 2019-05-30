#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace custom {

class CustomBinarySensorConstructor : public Component {
 public:
  CustomBinarySensorConstructor(const std::function<std::vector<binary_sensor::BinarySensor *>()> &init) {
    this->binary_sensors_ = init();
  }

  binary_sensor::BinarySensor *get_binary_sensor(int i) { return this->binary_sensors_[i]; }

  void dump_config() override;

 protected:
  std::vector<binary_sensor::BinarySensor *> binary_sensors_;
};

}  // namespace custom
}  // namespace esphome
