#pragma once

#include "../systa_bus.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::systa_bus {

class SystaSolarAquaTextSensor : public SystaBusListener, public Component {
 public:
  void dump_config() override;
  void set_error_code_text_sensor(text_sensor::TextSensor *sensor) { this->error_code_text_sensor_ = sensor; }
  void handle_message(std::vector<uint8_t> &message) override;
 protected:
  text_sensor::TextSensor *error_code_text_sensor_{nullptr};
};

}  // namespace esphome::systa_bus
