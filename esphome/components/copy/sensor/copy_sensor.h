#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::copy {

class CopySensor : public sensor::Sensor, public Component {
 public:
  void set_source(sensor::Sensor *source) { source_ = source; }
  void setup() override;
  void dump_config() override;

 protected:
  sensor::Sensor *source_;
};

}  // namespace esphome::copy
