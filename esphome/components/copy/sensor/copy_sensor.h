#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace copy {

class CopySensor : public sensor::Sensor, public Component {
 public:
  void set_source(sensor::Sensor *source) { source_ = source; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  sensor::Sensor *source_;
};

}  // namespace copy
}  // namespace esphome
