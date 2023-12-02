#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <vector>

namespace esphome {
namespace custom {

class CustomTextSensorConstructor : public Component {
 public:
  CustomTextSensorConstructor(const std::function<std::vector<text_sensor::TextSensor *>()> &init) {
    this->text_sensors_ = init();
  }

  text_sensor::TextSensor *get_text_sensor(int i) { return this->text_sensors_[i]; }

  void dump_config() override;

 protected:
  std::vector<text_sensor::TextSensor *> text_sensors_;
};

}  // namespace custom
}  // namespace esphome
