#pragma once

#include "../text.h"
#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::text {

class TextTextSensor : public text_sensor::TextSensor, public Component {
 public:
  explicit TextTextSensor(Text *source) : source_(source) {}
  void setup() override;
  void dump_config() override;

 protected:
  Text *source_;
};

}  // namespace esphome::text
