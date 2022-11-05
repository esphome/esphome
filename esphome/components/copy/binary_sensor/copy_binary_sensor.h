#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace copy {

class CopyBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void set_source(binary_sensor::BinarySensor *source) { source_ = source; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  binary_sensor::BinarySensor *source_;
};

}  // namespace copy
}  // namespace esphome
