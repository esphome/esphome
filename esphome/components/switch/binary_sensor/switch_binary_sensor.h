#pragma once

#include "../switch.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace switch_ {

class SwitchBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void set_source(Switch *source) { source_ = source; }
  void setup() override;
  void dump_config() override;

 protected:
  Switch *source_;
};

}  // namespace switch_
}  // namespace esphome
