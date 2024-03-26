#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../econet.h"

namespace esphome {
namespace econet {

class EconetTextSensor : public text_sensor::TextSensor, public Component, public EconetClient {
 public:
  void setup() override;
  void dump_config() override;
  void set_sensor_id(const std::string &sensor_id) { this->sensor_id_ = sensor_id; }

 protected:
  std::string sensor_id_{""};
};

}  // namespace econet
}  // namespace esphome
