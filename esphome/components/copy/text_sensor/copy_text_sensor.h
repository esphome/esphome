#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace copy {

class CopyTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_source(text_sensor::TextSensor *source) { source_ = source; }
  void setup() override;
  void dump_config() override;

 protected:
  text_sensor::TextSensor *source_;
};

}  // namespace copy
}  // namespace esphome
