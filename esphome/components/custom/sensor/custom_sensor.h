#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include <vector>

namespace esphome {
namespace custom {

class CustomSensorConstructor : public Component {
 public:
  CustomSensorConstructor(const std::function<std::vector<sensor::Sensor *>()> &init) { this->sensors_ = init(); }

  sensor::Sensor *get_sensor(int i) { return this->sensors_[i]; }

  void dump_config() override;

 protected:
  std::vector<sensor::Sensor *> sensors_;
};

}  // namespace custom
}  // namespace esphome
