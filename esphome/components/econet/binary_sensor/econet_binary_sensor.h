#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../econet.h"

namespace esphome {
namespace econet {

class EconetBinarySensor : public binary_sensor::BinarySensor, public Component, public EconetClient {
 public:
  void setup() override;
  void dump_config() override;
  void set_sensor_id(const std::string &sensor_id) { this->sensor_id_ = sensor_id; }

 protected:
  std::string sensor_id_{""};
};

}  // namespace econet
}  // namespace esphome
